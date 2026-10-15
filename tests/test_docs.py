"""Checks that keep docs/API.md, the C headers, the Python tools, the linker scripts, the README, the other documents and the CI file telling the same story.

Standard library only, plus gcc, make and the 'cryptography' package that the signing tool and the C host tests already need.
Run from anywhere:  python3 -m unittest discover -s tests -v
"""
import ast
import hashlib
import importlib.util
import os
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
_spec = importlib.util.spec_from_file_location("make_api_docs", ROOT / "tools" / "make_api_docs.py")
gen = importlib.util.module_from_spec(_spec)
sys.modules["make_api_docs"] = gen
_spec.loader.exec_module(gen)

DOCS = ["README.md", "SECURITY.md", "docs/API.md", "docs/CRA_MAPPING.md", "docs/IEC62443_SELF_ASSESSMENT.md", "docs/THREAT_MODEL.md"]
NUMBER_DOCS = ["README.md", "docs/CRA_MAPPING.md", "docs/IEC62443_SELF_ASSESSMENT.md", "docs/THREAT_MODEL.md"]


def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8")


def run(cmd, cwd=ROOT):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)


def python_constants(rel):
    """Integer constants assigned at the top level of a Python file ({name: value}); simple arithmetic is evaluated, anything else is skipped."""
    ops = {ast.Add: lambda a, b: a + b, ast.Sub: lambda a, b: a - b, ast.Mult: lambda a, b: a * b, ast.LShift: lambda a, b: a << b}

    def ev(node, env):
        if isinstance(node, ast.Constant) and isinstance(node.value, int):
            return node.value
        if isinstance(node, ast.Name):
            return env[node.id]
        if isinstance(node, ast.BinOp):
            return ops[type(node.op)](ev(node.left, env), ev(node.right, env))
        if isinstance(node, ast.Tuple):
            return tuple(ev(e, env) for e in node.elts)
        raise ValueError(type(node).__name__)

    env = {}
    for node in ast.parse(read(rel)).body:
        if isinstance(node, ast.Assign):
            try:
                value = ev(node.value, env)
            except (ValueError, KeyError, TypeError):
                continue
            for t in node.targets:
                if isinstance(t, ast.Name):
                    env[t.id] = value
                elif isinstance(t, ast.Tuple):
                    env.update(zip((e.id for e in t.elts), value))
    return env


def all_repo_paths():
    out = set()
    for dirpath, dirnames, files in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d != ".git"]
        rel = Path(dirpath).relative_to(ROOT).as_posix()
        out.add("" if rel == "." else rel)
        out.update((Path(rel) / f).as_posix() if rel != "." else f for f in files)
    return out


class Facts(unittest.TestCase):
    """The numbers every source repeats must be the same everywhere."""

    @classmethod
    def setUpClass(cls):
        cls.facts = gen.c_facts()
        cls.k = cls.facts["consts"]

    def test_the_signing_tool_uses_the_constants_of_the_c_header(self):
        """tools/mkimage.py and bootloader/image.h agree on the magic, header size, slot addresses and sizes, and the RAM range."""
        py = python_constants("tools/mkimage.py")
        names = ["IMG_MAGIC", "IMG_HDR_VERSION", "IMG_HDR_SIZE", "IMG_FIXED_LEN", "SLOT_A_BASE", "SLOT_B_BASE", "SLOT_SIZE", "APP_BASE", "IMG_MAX_SIZE", "RAM_START", "RAM_END"]
        for n in names:
            self.assertIn(n, py, f"mkimage.py no longer defines {n}")
            self.assertEqual(py[n], self.k[n], f"{n}: mkimage.py has {py[n]:#x}, image.h has {self.k[n]:#x}")
        sig = {n: off for n, off, _ in self.facts["fields"]}["signature"]
        self.assertEqual(py["SIG_OFFSET"], sig, "mkimage.py writes the signature at a different offset from the header structure")

    def test_the_eeprom_reset_tool_and_the_header_use_the_same_address(self):
        """tools/make_eeprom_reset.py clears the address that bootloader/nv_eeprom.h says holds the anti-rollback record."""
        self.assertEqual(python_constants("tools/make_eeprom_reset.py")["ROLLBACK_EEPROM_ADDR"], self.k["ROLLBACK_EEPROM_ADDR"])
        m = re.search(r"(0x[0-9A-Fa-f]+)\s+16 B\s+bootloader", read("bootloader/nv_eeprom.h"))
        self.assertIsNotNone(m, "the data EEPROM map in nv_eeprom.h's comment has changed shape")
        self.assertEqual(int(m.group(1), 16), self.k["ROLLBACK_EEPROM_ADDR"])
        self.assertEqual(self.k["ROLLBACK_WORDS"] * 4, 16, "the anti-rollback record is no longer 16 bytes")

    def test_the_field_table_in_image_h_matches_the_compiled_structure(self):
        """The hand-written field table in image.h's comment gives the same offsets, sizes and names as the compiled img_header structure."""
        table = gen.header_comment_table(read("bootloader/image.h"))
        fields = self.facts["fields"]
        self.assertEqual(sorted((o, s) for o, s, _, _ in table), sorted((o, s) for _, o, s in fields))
        by_off = {o: n for n, o, _ in fields}
        for off, size, name, _ in table:
            if name != "reserved":
                self.assertEqual(by_off[off], name, f"image.h's comment calls the field at offset {off} '{name}'")
        self.assertEqual(self.facts["sizeof"], self.k["IMG_HDR_SIZE"], "sizeof(img_header) is not IMG_HDR_SIZE")

    def test_an_image_made_by_the_signing_tool_has_the_documented_layout(self):
        """An image built by tools/mkimage.py with a throw-away key has every field where the compiled structure says, and its signature verifies in Python."""
        from cryptography.hazmat.primitives import hashes, serialization
        from cryptography.hazmat.primitives.asymmetric import ec, utils
        off = {n: (o, s) for n, o, s in self.facts["fields"]}
        with tempfile.TemporaryDirectory() as d:
            d = Path(d)
            key = d / "k.pem"
            r = run([sys.executable, "tools/gen_signing_key.py", "--key", str(key), "--header", str(d / "pub.h")])
            self.assertEqual(r.returncode, 0, r.stderr)
            app = struct.pack("<II", 0x20005000, 0x08006201) + bytes(range(256)) * 4
            (d / "app.bin").write_bytes(app)
            r = run([sys.executable, "tools/mkimage.py", str(d / "app.bin"), "--version", "1.2.3", "--key", str(key), "-o", str(d / "img")])
            self.assertEqual(r.returncode, 0, r.stderr)
            img = (d / "img.bin").read_bytes()
            u32 = lambda name: struct.unpack_from("<I", img, off[name][0])[0]
            hdr = self.k["IMG_HDR_SIZE"]
            self.assertEqual(len(img), hdr + len(app))
            self.assertEqual((u32("magic"), u32("hdr_version"), u32("hdr_size"), u32("img_size")), (self.k["IMG_MAGIC"], self.k["IMG_HDR_VERSION"], hdr, len(app)))
            self.assertEqual(u32("fw_version"), (1 << 24) | (2 << 16) | 3)
            digest = hashlib.sha256(img[:self.k["IMG_FIXED_LEN"]] + app).digest()
            o, s = off["sha256"]
            self.assertEqual(img[o:o + s], digest, "the hash is not SHA-256 over the fixed header bytes followed by the application")
            o, s = off["signature"]
            sig = img[o:o + s]
            pub = serialization.load_pem_private_key(key.read_bytes(), None).public_key()
            pub.verify(utils.encode_dss_signature(int.from_bytes(sig[:32], "big"), int.from_bytes(sig[32:], "big")), digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))
            for name in ("reserved0", "reserved1"):
                o, s = off[name]
                self.assertEqual(img[o:o + s], bytes(s), f"{name} is not zero")
            self.assertEqual(img[hdr:], app)

    def test_the_linker_scripts_agree_with_the_header(self):
        """The bootloader's linker script ends where slot A starts; both application linker scripts start at the application and fit the slot; RAM is 20 KB in all three."""
        k = self.k
        boot = gen.linker_regions(gen.LD_BOOTLOADER)
        self.assertEqual(boot["FLASH"], (0x08000000, k["SLOT_A_BASE"] - 0x08000000))
        for ld in gen.LD_APP:
            app = gen.linker_regions(ld)
            self.assertEqual(app["FLASH"], (k["APP_BASE"], k["IMG_MAX_SIZE"]), f"{ld}: flash does not start at the application or is not the slot minus the header")
            self.assertEqual(app["RAM"], (k["RAM_START"], k["RAM_END"] - k["RAM_START"]), f"{ld}: RAM differs from image.h")
        self.assertEqual(boot["RAM"], (k["RAM_START"], k["RAM_END"] - k["RAM_START"]))

    def test_the_slots_follow_each_other_and_are_aligned_to_whole_flash_pages(self):
        """Slot B starts where slot A ends, every slot boundary is a whole 128-byte page, and the write-protect mask covers exactly the bootloader's 4 KB sectors."""
        k = self.k
        self.assertEqual(k["SLOT_A_BASE"] + k["SLOT_SIZE"], k["SLOT_B_BASE"])
        for n in ("SLOT_A_BASE", "SLOT_B_BASE", "SLOT_SIZE", "IMG_HDR_SIZE"):
            self.assertEqual(k[n] % k["FLASH_PAGE_SIZE"], 0, f"{n} is not a whole number of flash pages")
        boot_len = gen.linker_regions(gen.LD_BOOTLOADER)["FLASH"][1]
        self.assertEqual(bin(k["OB_BOOTLOADER_WRP_MASK"]).count("1") * 4096, boot_len, "the write-protect mask does not cover exactly the bootloader")

    def test_the_flash_table_in_the_readme_matches_the_headers_and_linker_scripts(self):
        """The README's flash-layout table (bootloader, slot A, slot B, reserve) gives the same start addresses and sizes as image.h and the linker script, and adds up to the stated total."""
        text = read("README.md")
        rows = {m.group(1): (int(m.group(2), 16), int(m.group(3)) * 1024)
                for m in re.finditer(r"^\| (Bootloader|Slot A|Slot B|Reserve) \| `(0x[0-9A-Fa-f]+)` \| (\d+) KB \|", text, re.M)}
        self.assertEqual(set(rows), {"Bootloader", "Slot A", "Slot B", "Reserve"}, "the README's flash table has changed shape")
        k = self.k
        boot = gen.linker_regions(gen.LD_BOOTLOADER)["FLASH"]
        self.assertEqual(rows["Bootloader"], boot)
        self.assertEqual(rows["Slot A"], (k["SLOT_A_BASE"], k["SLOT_SIZE"]))
        self.assertEqual(rows["Slot B"], (k["SLOT_B_BASE"], k["SLOT_SIZE"]))
        self.assertEqual(rows["Reserve"][0], k["SLOT_B_BASE"] + k["SLOT_SIZE"])
        total = int(re.search(r"Flash layout \((\d+) KB", text).group(1)) * 1024
        self.assertEqual(sum(s for _, s in rows.values()), total, "the sizes in the README's flash table do not add up to its stated total")
        self.assertEqual(rows["Reserve"][0] + rows["Reserve"][1], 0x08000000 + total, "the reserve does not end at the end of flash")


class Documentation(unittest.TestCase):
    """docs/API.md and the documents that mention the code."""

    def test_the_generated_parts_of_api_md_are_current(self):
        """The tables in docs/API.md are exactly what tools/make_api_docs.py writes from the code today."""
        r = run([sys.executable, "tools/make_api_docs.py", "--check"])
        self.assertEqual(r.returncode, 0, r.stderr + r.stdout)

    def test_every_header_is_in_the_reference_or_deliberately_left_out(self):
        """Every .h file in bootloader/ and secure_link/ is either documented in API.md or listed as left out, with a reason."""
        on_disk = {p.relative_to(ROOT).as_posix() for d in ("bootloader", "secure_link") for p in (ROOT / d).glob("*.h")}
        self.assertEqual(on_disk - set(gen.SKIPPED_HEADERS), set(gen.HEADERS), "a header was added or removed without updating tools/make_api_docs.py")
        for rel in gen.HEADERS:
            self.assertIn(f"#### `{rel}`", read("docs/API.md"))

    def test_every_declaration_in_the_headers_has_a_comment(self):
        """Every function and extern declaration in a documented header has a comment directly above it or on its line, and every header has a one-line summary."""
        for rel in gen.HEADERS:
            info = gen.parse_header(read(rel))
            self.assertTrue(info["summary"], f"{rel}: the first line of the file should read '/* name.h - summary'")
            for name, decl, comment in info["functions"]:
                self.assertTrue(comment, f"{rel}: '{name}' has no comment: {decl}")

    def test_every_function_and_constant_in_the_headers_appears_in_the_reference(self):
        """A second, simpler reading of each header finds the same functions, extern declarations and #define constants as appear in the generated tables."""
        api = read("docs/API.md")
        for rel in gen.HEADERS:
            text = read(rel)
            consts = [m.group(1) for m in re.finditer(r"^#define[ \t]+(\w+)[ \t]+\S", text, re.M)]
            funcs = [m.group(1) for m in re.finditer(r"^(?!typedef|#|/\*|\*|\}|extern \"C\")[A-Za-z_][\w \t*]*?\b(\w+)\s*\(", text, re.M) if not m.group(0).startswith("extern")]
            externs = [re.sub(r"\s+", " ", ln.strip().rstrip(";")) for ln in text.split("\n") if re.match(r'^extern\s+(?!"C")', ln)]
            self.assertTrue(consts or funcs or externs, rel)
            for name in consts + funcs:
                self.assertIn(f"`{name}`", api, f"{rel}: {name} is missing from docs/API.md")
            for decl in externs:
                self.assertIn(decl, api, f"{rel}: '{decl}' is missing from docs/API.md")

    def test_every_tool_and_every_test_program_is_listed(self):
        """Every script in tools/, the two in secure_link/, and every test_*.c program has an entry in API.md."""
        api = read("docs/API.md")
        scripts = {p.relative_to(ROOT).as_posix() for p in (ROOT / "tools").iterdir() if p.suffix in (".py", ".sh")} | set(gen.SECURE_LINK_TOOLS)
        for rel in scripts:
            self.assertIn(f"#### `{rel}`", api)
        for d in gen.TEST_DIRS:
            for name in gen.test_programs(d):
                self.assertIn(f"`{d}/{name}`", api)

    def test_every_file_path_mentioned_in_the_documents_exists(self):
        """Every file or folder named in backticks in the README, SECURITY.md and docs/ exists in the repository (build outputs, the private key and the generated key headers excepted)."""
        paths = all_repo_paths()
        missing = []
        for doc in DOCS:
            for tok in set(re.findall(r"`([^`\n]+)`", read(doc))):
                tok = tok.strip()
                if not (re.fullmatch(r"[\w./-]+\.(c|h|py|sh|md|ld|txt|ino|json|yml)", tok) or re.fullmatch(r"[\w-]+/[\w./-]*", tok)):
                    continue
                if re.search(r"\.(elf|hex|bin|map|pem)$", tok) or tok.startswith(("images/", "build/", "keys/")) or "<" in tok or tok in gen.SKIPPED_HEADERS:
                    continue
                t = tok.rstrip("/")
                if not (t in paths or any(p.endswith("/" + t) for p in paths) or any(p.startswith(t + "/") for p in paths)):
                    missing.append(f"{doc}: {tok}")
        self.assertEqual(missing, [])

    def test_the_power_cut_numbers_in_the_documents_are_what_the_update_test_prints(self):
        """The 'power cut at N points' figure in the README, the CRA mapping, the IEC 62443 assessment and the threat model equals what the update test reports when it runs."""
        d = ROOT / "bootloader" / "test"
        try:
            r = run(["make", "test_update"], cwd=d)
            self.assertEqual(r.returncode, 0, r.stderr[-800:])
            out = run(["./test_update"], cwd=d)
            self.assertEqual(out.returncode, 0, out.stdout[-800:])
        finally:
            run(["make", "clean"], cwd=d)
        m = re.search(r"power cut at (\d+) points of a (\d+)-operation install", out.stdout)
        self.assertIsNotNone(m, "the update test no longer prints its number of cut points")
        points = int(m.group(1))
        for doc in NUMBER_DOCS:
            found = [int(x.replace(",", "")) for x in re.findall(r"(\d[\d,]*) +(?:simulated +)?(?:cut +)?points", read(doc))]
            self.assertTrue(found, f"{doc} no longer states the number of power-cut points")
            self.assertEqual(set(found), {points}, f"{doc} says {found} power-cut points; the test ran {points}")

    def test_every_option_of_the_python_tools_has_help_text(self):
        """Every command-line option of the Python tools has help text, so the generated tool reference is never a row of empty descriptions."""
        for rel in gen.tool_files() + gen.SECURE_LINK_TOOLS:
            if rel.endswith(".py"):
                for names, help_text, _ in gen.argparse_rows(ast.parse(read(rel))):
                    self.assertTrue(help_text, f"{rel}: option {names} has no help text")

    def test_the_examples_in_api_md_compile_and_the_secure_link_one_runs(self):
        """Each C example in API.md compiles with warnings as errors against the real headers, and the secure-link example round-trips a packet when linked with the real code."""
        api = read("docs/API.md")
        block = api[api.index("<!-- examples:start -->"):api.index("<!-- examples:end -->")]
        examples = re.findall(r"```c\n(.*?)```", block, re.S)
        self.assertEqual(len(examples), 3)
        flags = ["gcc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-I", "bootloader", "-I", "secure_link"]
        with tempfile.TemporaryDirectory() as d:
            for i, code in enumerate(examples):
                src = Path(d) / f"ex{i}.c"
                src.write_text(code, encoding="utf-8")
                r = run(flags + ["-fsyntax-only", str(src)])
                self.assertEqual(r.returncode, 0, f"example {i + 1}:\n{r.stderr}")
            link = next(c for c in examples if "round_trip" in c)
            (Path(d) / "link.c").write_text(link + "\n#include <stdio.h>\nint main(void) { uint8_t e[16] = {1}, m[16] = {2}; int n = round_trip(e, m); printf(\"%d\\n\", n); return n == 2 ? 0 : 1; }\n")
            r = run(flags + ["-o", str(Path(d) / "link"), str(Path(d) / "link.c"), "secure_link/aes128.c", "secure_link/aes_cmac.c", "secure_link/secure_link.c"])
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertEqual(run([str(Path(d) / "link")]).returncode, 0, "the secure-link example did not return the payload length")


class Wiring(unittest.TestCase):
    """The tests, the build files and CI must actually run what exists."""

    def test_every_test_program_is_built_and_run_by_its_makefile(self):
        """Every test_*.c has a build rule in its Makefile, is a prerequisite of the default test target, and is run by it: no test can exist without being run."""
        for d in gen.TEST_DIRS:
            mk = read(f"{d}/Makefile")
            default = set()
            for m in re.finditer(r"^(?:test|all):(.*)$", mk, re.M):
                default |= set(m.group(1).split())
            for name in gen.test_programs(d):
                stem = name[:-2]
                self.assertRegex(mk, rf"(?m)^{stem}:", f"{d}/Makefile has no rule that builds {stem}")
                self.assertIn(stem, default, f"{d}/Makefile: {stem} is not built by the default target (the test exists but never runs)")
                self.assertRegex(mk, rf"(?m)^\t\./{stem}\b", f"{d}/Makefile never runs ./{stem}")
            self.assertRegex(mk, r"(?m)^test:", f"{d}/Makefile has no 'test' target")

    def test_ci_runs_every_test_suite_on_push_and_on_pull_requests(self):
        """The CI workflow runs the bootloader C tests, the secure-link C tests and these documentation tests, installs the cryptography package, and triggers on push to main and on pull requests."""
        ci = read(".github/workflows/ci.yml")
        for cmd in ("make -C bootloader/test test", "make -C secure_link/test test", "python -m unittest discover -s tests"):
            self.assertIn(cmd, ci)
        self.assertIn("pip install cryptography", ci)
        self.assertRegex(ci, r"(?m)^  pull_request:")
        self.assertRegex(ci, r"(?m)^  push:")

    def test_python_bytecode_is_ignored_and_not_committed(self):
        """.gitignore ignores __pycache__ and *.pyc, and no bytecode file is tracked by git."""
        ign = read(".gitignore")
        self.assertIn("__pycache__/", ign)
        self.assertIn("*.pyc", ign)
        r = run(["git", "ls-files"])
        if r.returncode == 0:
            tracked = [f for f in r.stdout.split("\n") if f and (ROOT / f).exists()]       # a file deleted from disk but not yet from the index is on its way out
            self.assertEqual([f for f in tracked if f.endswith(".pyc") or "__pycache__" in f], [])


if __name__ == "__main__":
    unittest.main()
