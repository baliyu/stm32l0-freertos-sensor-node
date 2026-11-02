"""Traceability from the threat model to the evidence behind it.

Every threat ST-nn in docs/THREAT_MODEL.md has one row in docs/TRACEABILITY.csv: what the node must do about it (a requirement),
how the repository shows it (named checks in the host tests, and hardware log files), and its status:

  mitigated   at least one named check in a host test (the check's text must really occur in that file)
  procedural  a control that exists outside the code (for example, how a key is kept) and cannot be tested here; the note says what
  accepted    a risk accepted on purpose; the note says why
  gap         known to be missing; the note says what is missing

Built with AI assistance (Claude).  Run `python3 tests/traceability.py` to print the matrix; it exits 1 if the matrix and the repository disagree.
"""
from __future__ import annotations

import csv
import io
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COLUMNS = ["id", "status", "requirement", "tests", "hardware_logs", "hardware_claimed", "note"]
STATUSES = ("mitigated", "procedural", "accepted", "gap")


def threat_ids(text):
    """The threat ids in the first column of the threat table, in order."""
    return re.findall(r"^\| (ST-\d+) \|", text, re.M)


def load(text):
    return list(csv.DictReader(io.StringIO(text)))


def split(cell):
    return [x.strip() for x in (cell or "").split(";") if x.strip()]


def test_programs(root=ROOT):
    """Every host test program, as a path relative to the repository root."""
    return sorted(p.relative_to(root).as_posix() for p in root.glob("*/test/test_*.c"))


def problems(rows, ids, root=ROOT):
    """Everything wrong with the matrix: an empty list means it agrees with the threat model and with the files."""
    out = []
    if not rows:
        return ["the matrix is empty"]
    missing = [c for c in COLUMNS if c not in rows[0]]
    if missing:
        return [f"missing columns: {', '.join(missing)}"]
    seen = [r["id"] for r in rows]
    for dup in sorted({i for i in seen if seen.count(i) > 1}):
        out.append(f"{dup}: listed more than once")
    for i in ids:
        if i not in seen:
            out.append(f"{i}: in the threat model but not in the matrix")
    for i in seen:
        if i not in ids:
            out.append(f"{i}: in the matrix but not in the threat model")
    referenced = set()
    for r in rows:
        name = r["id"]
        if r["status"] not in STATUSES:
            out.append(f"{name}: status {r['status']!r} is not one of {', '.join(STATUSES)}")
        if r["hardware_claimed"] not in ("yes", "no"):
            out.append(f"{name}: hardware_claimed must be yes or no")
        if not r["requirement"].strip():
            out.append(f"{name}: no requirement written")
        tests = split(r["tests"])
        if r["status"] == "mitigated" and not tests:
            out.append(f"{name}: mitigated but no test is named")
        if r["status"] != "mitigated" and not r["note"].strip():
            out.append(f"{name}: {r['status']} needs a note saying why")
        for ref in tests:
            path, _, needle = ref.partition("::")
            f = root / path
            if not needle:
                out.append(f"{name}: test reference {ref!r} has no '::text' part")
            elif not f.is_file():
                out.append(f"{name}: {path} does not exist")
            elif needle not in f.read_text(encoding="utf-8", errors="replace"):
                out.append(f"{name}: {path} has no check or text '{needle}'")
            else:
                referenced.add(path)
        for log in split(r["hardware_logs"]):
            if not (root / log).is_file():
                out.append(f"{name}: hardware log {log} does not exist")
    for prog in test_programs(root):
        if prog not in referenced:
            out.append(f"{prog}: a host test that no threat refers to")
    return out


def unlogged_claims(rows):
    """Threats whose evidence in the threat model includes a hardware result, but with no log file in the repository."""
    return [r["id"] for r in rows if r["hardware_claimed"] == "yes" and not split(r["hardware_logs"])]


def counts(rows):
    return {s: sum(1 for r in rows if r["status"] == s) for s in STATUSES}


def report(rows):
    lines = [f"{len(rows)} threats: " + ", ".join(f"{n} {s}" for s, n in counts(rows).items())]
    for r in rows:
        lines.append(f"  {r['id']}  {r['status']:10s} tests {len(split(r['tests'])):2d}  hardware logs {len(split(r['hardware_logs'])):2d}  {r['requirement']}")
    un = unlogged_claims(rows)
    lines.append("hardware results claimed in the threat model with no log file in the repository: " + (", ".join(un) if un else "none"))
    return "\n".join(lines)


def main(root=ROOT):
    ids = threat_ids((root / "docs" / "THREAT_MODEL.md").read_text(encoding="utf-8"))
    rows = load((root / "docs" / "TRACEABILITY.csv").read_text(encoding="utf-8"))
    found = problems(rows, ids, root)
    if found:
        print("\n".join(found))
        return 1
    print(report(rows))
    return 0


if __name__ == "__main__":
    sys.exit(main())
