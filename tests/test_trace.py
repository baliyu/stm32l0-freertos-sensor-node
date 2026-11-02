"""Tests for the threat-to-evidence matrix (tests/traceability.py) and for the committed docs/TRACEABILITY.csv.  Built with AI assistance (Claude)."""
import re
import tempfile
import unittest
from pathlib import Path

import traceability as tr

ROOT = Path(__file__).resolve().parents[1]
HEAD = ",".join(tr.COLUMNS) + "\n"


def make_repo(files):
    d = tempfile.TemporaryDirectory()
    for rel, text in files.items():
        p = Path(d.name) / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding="utf-8")
    return d


def row(i="ST-01", status="mitigated", tests="a/test/test_x.c::the check", logs="", claimed="no", note="", req="r"):
    return f"{i},{status},{req},{tests},{logs},{claimed},{note}\n"


BASE = {"a/test/test_x.c": '/* x */ check(1, "the check");\n', "docs/log.txt": "log\n"}


def run(rows, ids=("ST-01",), files=BASE):
    with make_repo(files) as _:
        pass
    d = make_repo(files)
    try:
        return tr.problems(tr.load(HEAD + "".join(rows)), list(ids), Path(d.name))
    finally:
        d.cleanup()


class Parsing(unittest.TestCase):
    def test_threat_ids_come_from_the_first_column_of_the_table(self):
        md = "| ID | S |\n|---|---|\n| ST-01 | S | x |\n| ST-12 | T | mentions ST-99 inside |\n"
        self.assertEqual(tr.threat_ids(md), ["ST-01", "ST-12"])

    def test_split_ignores_blanks_and_spaces(self):
        self.assertEqual(tr.split(" a ; b;;"), ["a", "b"])
        self.assertEqual(tr.split(""), [])
        self.assertEqual(tr.split(None), [])


class Checking(unittest.TestCase):
    def test_a_good_matrix_has_no_problems(self):
        self.assertEqual(run([row()]), [])

    def test_a_threat_missing_from_the_matrix_and_an_extra_one_are_both_reported(self):
        p = run([row("ST-01")], ids=("ST-01", "ST-02"))
        self.assertIn("ST-02: in the threat model but not in the matrix", p)
        p = run([row("ST-01"), row("ST-09")], ids=("ST-01",))
        self.assertIn("ST-09: in the matrix but not in the threat model", p)

    def test_a_threat_listed_twice_is_reported(self):
        self.assertIn("ST-01: listed more than once", run([row(), row()]))

    def test_an_unknown_status_is_reported(self):
        self.assertTrue(any("status 'done'" in x for x in run([row(status="done")])))

    def test_mitigated_needs_a_named_test(self):
        self.assertIn("ST-01: mitigated but no test is named", run([row(tests="")]))

    def test_accepted_procedural_and_gap_rows_need_a_note_but_not_a_test(self):
        for s in ("accepted", "procedural", "gap"):
            self.assertEqual(run([row(status=s, tests="", note="because")], files={"docs/log.txt": "log\n"}), [])
            self.assertTrue(any("needs a note" in x for x in run([row(status=s, tests="", note="")], files={"docs/log.txt": "log\n"})))

    def test_a_test_reference_must_name_an_existing_file_and_text_that_occurs_in_it(self):
        self.assertTrue(any("no check or text 'nope'" in x for x in run([row(tests="a/test/test_x.c::nope")])))
        self.assertTrue(any("does not exist" in x for x in run([row(tests="a/test/missing.c::x")])))
        self.assertTrue(any("no '::text' part" in x for x in run([row(tests="a/test/test_x.c")])))

    def test_hardware_logs_must_exist(self):
        self.assertEqual(run([row(logs="docs/log.txt", claimed="yes")]), [])
        self.assertTrue(any("hardware log docs/gone.txt does not exist" in x for x in run([row(logs="docs/gone.txt", claimed="yes")])))

    def test_hardware_claimed_must_be_yes_or_no(self):
        self.assertTrue(any("hardware_claimed" in x for x in run([row(claimed="maybe")])))

    def test_a_host_test_that_no_threat_refers_to_is_reported(self):
        files = dict(BASE, **{"b/test/test_orphan.c": "x\n"})
        self.assertIn("b/test/test_orphan.c: a host test that no threat refers to", run([row()], files=files))

    def test_missing_columns_and_an_empty_matrix_are_reported(self):
        self.assertTrue(tr.problems(tr.load("id,status\nST-01,gap\n"), ["ST-01"])[0].startswith("missing columns"))
        self.assertEqual(tr.problems([], ["ST-01"]), ["the matrix is empty"])


class Reporting(unittest.TestCase):
    def test_hardware_claims_without_a_log_are_listed_and_claims_with_a_log_are_not(self):
        rows = tr.load(HEAD + row("ST-01", claimed="yes") + row("ST-02", claimed="yes", logs="docs/log.txt") + row("ST-03", claimed="no"))
        self.assertEqual(tr.unlogged_claims(rows), ["ST-01"])

    def test_counts_by_status(self):
        rows = tr.load(HEAD + row("ST-01") + row("ST-02", status="gap", tests="", note="n") + row("ST-03", status="gap", tests="", note="n"))
        self.assertEqual(tr.counts(rows), {"mitigated": 1, "procedural": 0, "accepted": 0, "gap": 2})

    def test_the_report_names_the_unlogged_claims(self):
        rows = tr.load(HEAD + row("ST-01", claimed="yes"))
        self.assertIn("no log file in the repository: ST-01", tr.report(rows))


class TheCommittedMatrix(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ids = tr.threat_ids((ROOT / "docs" / "THREAT_MODEL.md").read_text(encoding="utf-8"))
        cls.rows = tr.load((ROOT / "docs" / "TRACEABILITY.csv").read_text(encoding="utf-8"))

    def test_the_threat_model_has_threats(self):
        self.assertGreaterEqual(len(self.ids), 17)

    def test_the_matrix_agrees_with_the_threat_model_and_with_every_file_it_names(self):
        self.assertEqual(tr.problems(self.rows, self.ids, ROOT), [])

    def test_every_host_test_program_is_the_evidence_for_some_threat(self):
        named = {ref.partition("::")[0] for r in self.rows for ref in tr.split(r["tests"])}
        self.assertEqual(set(tr.test_programs(ROOT)) - named, set())

    def test_the_numbers_in_the_traceability_document_are_the_matrix_counts(self):
        doc = (ROOT / "docs" / "TRACEABILITY.md").read_text(encoding="utf-8")
        m = re.search(r"Counts: (\d+) mitigated, (\d+) procedural, (\d+) accepted, (\d+) gap", doc)
        self.assertTrue(m, "docs/TRACEABILITY.md no longer states the counts")
        self.assertEqual(tuple(int(x) for x in m.groups()), tuple(tr.counts(self.rows).values()))
        un = re.search(r"No log file in the repository for: ([A-Z0-9, -]+)\.", doc)
        self.assertTrue(un, "docs/TRACEABILITY.md no longer lists the unlogged hardware claims")
        self.assertEqual([x.strip() for x in un.group(1).split(",")], tr.unlogged_claims(self.rows))


if __name__ == "__main__":
    unittest.main()
