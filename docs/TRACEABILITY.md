# From threat to evidence

Built with AI assistance (Claude). This page links every threat in [THREAT_MODEL.md](THREAT_MODEL.md) to what the repository offers as evidence, so that "the threat model says X is stopped" can be followed to a named check or a log file.
The data is in `docs/TRACEABILITY.csv`, one row per threat; `tests/traceability.py` checks it and `python3 tests/traceability.py` prints it. It runs with the other tests.

## What a row says

| Status | Meaning |
|---|---|
| mitigated | A requirement the node must meet, with at least one named check in a host test. The check's text must occur in that file, so a renamed or deleted check breaks the build |
| procedural | A control that exists outside the code (how the signing key is kept); nothing here can test it, and the note says what it is |
| accepted | A risk accepted on purpose, with the reason |
| gap | Known to be missing, with what is missing |

Counts: 11 mitigated, 1 procedural, 3 accepted, 2 gap. That is 17 threats.

## What the matrix found

- **Hardware results with no log.** The threat model cites hardware results for the refusal of tampered images (ST-06), the refusal of an older image (ST-07) and a real power cut during an update (ST-08). The repository holds hardware logs only for the lockdown stage (write protection and read-out protection, ST-09 and ST-10).
  No log file in the repository for: ST-06, ST-07, ST-08.
- **Every host test program is used.** Each of the eight test programs is evidence for at least one threat; one that supported no threat would be reported.
- **Gaps are visible.** ST-14 (no persistent security log) and ST-15 (brown-out reset off) are marked as gaps, the same two the CRA and IEC 62443 self-assessments report.

## What it does not do

It checks that a named check exists in a test file, not that the check is a good one; the CI run shows that the tests pass. It does not judge whether the requirement wording is complete.
Requirements are written per threat; this is not a full requirements specification. Statuses are my judgement and come with the threat model's own residual risks.

## Adding or changing a threat

Add the row to `docs/THREAT_MODEL.md`, then one row to `docs/TRACEABILITY.csv` with the same id. A reference to a check is written `path::text`, where the text occurs in that file. The tests fail if an id is missing, a file or text is gone, or a mitigated row names no check.
