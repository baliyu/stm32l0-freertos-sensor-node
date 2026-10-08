# Security policy

`stm32l0-freertos-sensor-node` is a personal engineering project (a demonstrator), not a commercial product. I take security reports seriously and handle them on a best-effort basis.

## Supported versions
Only the latest commit on `main` is maintained.

## Reporting a vulnerability
Please do **not** open a public issue for a security problem.

1. Preferred: use GitHub's private vulnerability reporting (Security tab, then "Report a vulnerability").
2. Or email baliyu70@gmail.com with the subject `SECURITY: stm32l0-freertos-sensor-node`.

Please include what you found, the affected files or commit, how to reproduce it (including any hardware or phone you used), the impact you see, and whether you plan to publish.

## What to expect
- I aim to acknowledge a report within 7 days. I work alone, so this is a target, not a guarantee.
- I will tell you whether I can reproduce it and agree a disclosure date with you (coordinated disclosure). I aim to fix or document the problem within 90 days.
- Once a fix exists I publish a GitHub security advisory with a description, the affected commits, the severity and what to do, and credit you if you wish.

## In scope
- Any way to make the bootloader run unsigned, modified or older (rolled-back) firmware.
- A flaw in the update installation that can leave the device unbootable or lose the anti-rollback state.
- Weaknesses in the `secure_link` packet format, counter handling or the receiver's replay protection.
- Mistakes in the key generation, signing or backup tools.

## Out of scope
- Debugger-based attacks on read-out protection Level 1 and physical fault injection: documented limitations.
- Side-channel attacks on the AES and ECDSA code, which I have not assessed.
- Denial of service by radio jamming.

## Known limitations
The residual risks I already know about are listed in [docs/secure_boot/THREAT_MODEL.md](docs/secure_boot/THREAT_MODEL.md). A report that only restates one of them is welcome as a discussion but is not a new vulnerability.
