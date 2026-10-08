# CRA gap analysis: STM32L0 FreeRTOS sensor node with secure bootloader

Self-assessment by the author, 8 October 2026, against Annex I of the EU Cyber Resilience Act (Regulation (EU) 2024/2847). **This repository is a demonstrator, not a product placed on the EU market, so the Act does not apply to it.** The exercise asks how it would measure up if it were a product. It is not legal advice and not a conformity assessment. Requirement wording is paraphrased; the exact text is in the Official Journal.

**Dates:** the reporting obligations (Article 14) apply from 11 September 2026; the rest of the Act, including all Annex I requirements and CE marking, applies from 11 December 2027.

**Reporting (Article 14), for a real product:** a manufacturer must notify actively exploited vulnerabilities and severe incidents through ENISA's single reporting platform: an early warning within 24 hours, a notification within 72 hours, and a final report within 14 days after a fix is available (vulnerabilities). The contact point for this repository is in `SECURITY.md`.

Status: **Met** / **Partly** (something required is missing) / **Not met** / **Not assessed** (I have not checked).

**Result:** 4 met, 16 partly, 1 not met, 1 not assessed, out of 22 requirements. Threat model: [THREAT_MODEL.md](THREAT_MODEL.md).

## Part I: properties of the product

| Annex I | Requirement (paraphrased) | Status | Evidence or gap |
|---|---|---|---|
| Part I, 1 | Appropriate cybersecurity level, based on the risks | **Partly** | A design-level threat model exists (`THREAT_MODEL.md`), self-assessed. |
| Part I, 2(a) | No known exploitable vulnerabilities when released | **Not assessed** | Vendored micro-ecc is pinned to a commit and the crypto has known-answer tests, but I have not checked the dependencies against published advisories. |
| Part I, 2(b) | Secure by default; the product can be reset to its original state | **Partly** | Secure defaults: signed images only, anti-rollback, protected bootloader, read-out protection Level 1 (a production build refuses Level 0). Reset to original state is not possible without a mass erase that destroys the keys. |
| Part I, 2(c) | Vulnerabilities can be fixed by security updates (automatic by default with opt-out, update notices, postponement) | **Partly** | The bootloader installs signed updates safely (power-fail-safe, anti-rollback). At RDP Level 1 there is no in-field delivery path, so the board cannot actually receive one; no notices or automatic updates. |
| Part I, 2(d) | Protection from unauthorised access (authentication, access management) and reporting of possible unauthorised access | **Partly** | Radio packets are authenticated by AES-CMAC and the debug port is locked (RDP Level 1). Attempts are reported only on the UART. |
| Part I, 2(e) | Confidentiality of stored and transmitted data (state-of-the-art encryption) | **Partly** | AES-128-CTR on the radio link. Link keys sit in internal flash, protected by RDP Level 1 but not encrypted. |
| Part I, 2(f) | Integrity of data, commands, programs and configuration; reporting of corruption | **Partly** | Strongest area: CMAC on packets, ECDSA-signed firmware, anti-rollback, write-protected bootloader, boot-time option-byte check. Corruption is reported on the UART only, not recorded. |
| Part I, 2(g) | Data minimisation | **Met** | Only temperature readings are processed; no personal data. |
| Part I, 2(h) | Availability of essential functions, also after an incident; resilience against denial of service | **Partly** | Heartbeat watchdog verified by fault injection; power-fail-safe updates. Radio jamming is not addressed and the fail-closed boot trades availability for safety. |
| Part I, 2(i) | Limited negative impact on the availability of other devices and networks | **Met** | A 16-byte packet about every 11 s stays well under the EU 1 % duty cycle. |
| Part I, 2(j) | Limited attack surface, including external interfaces | **Partly** | LoRa, UART, a button and the locked debug port. The 1-Wire sensor wire is reachable by a physical attacker. |
| Part I, 2(k) | Exploitation mitigation to reduce the impact of an incident | **Partly** | Write protection, boot-time checks, fail-closed behaviour. No privilege separation on the Cortex-M0+ and no memory protection. |
| Part I, 2(l) | Security logging and monitoring of relevant internal activity (with user opt-out) | **Not met** | Rejected updates and failed checks are printed on the UART; nothing is stored. |
| Part I, 2(m) | Users can securely and easily remove all data and settings permanently | **Partly** | Returning from RDP Level 1 to Level 0 mass-erases flash and EEPROM, which removes the keys, but it needs a debugger and also erases the firmware. |

## Part II: vulnerability handling

| Annex I | Requirement (paraphrased) | Status | Evidence or gap |
|---|---|---|---|
| Part II, (1) | Identify and document vulnerabilities and components, including a machine-readable SBOM (at least top-level dependencies) | **Partly** | Components are named in the README (FreeRTOS, ST HAL, micro-ecc at a pinned commit). No machine-readable SBOM. |
| Part II, (2) | Fix vulnerabilities without delay, including by security updates | **Partly** | `SECURITY.md` commits to fixing reported issues; a signed update could be built, but cannot be delivered in the field yet. |
| Part II, (3) | Effective and regular security tests and reviews | **Partly** | Known-answer tests (FIPS-197, RFC 4493, RFC 6979), power-cut tests at 1,682 points, hardware tests of every lockdown step, a threat model. No fuzzing, static analysis or independent review. |
| Part II, (4) | Publicly disclose fixed vulnerabilities once an update is available | **Partly** | `SECURITY.md` promises a GitHub security advisory once a fix exists; none has been needed yet. |
| Part II, (5) | A coordinated vulnerability disclosure policy | **Met** | `SECURITY.md` states the coordinated disclosure policy. |
| Part II, (6) | Facilitate reporting, including a contact address, for the product and its third-party components | **Met** | `SECURITY.md` gives a private reporting route and an email address. |
| Part II, (7) | Mechanisms to distribute updates securely (automatic where applicable) | **Partly** | The mechanism is built: signature check, anti-rollback, safe install. The distribution channel (UART or LoRa into slot B) is missing. |
| Part II, (8) | Disseminate updates without delay, free of charge, with advisory messages | **Partly** | Free (MIT) and advisories would go through GitHub; no delivery to devices. |

## The three gaps I would close first

1. **In-field update path** (I.2c, II.2, II.7, II.8): receive images over UART or LoRa into slot B; the bootloader side already exists.
1. **Security event log** (I.2l): record rejected updates, failed checks and counter events in non-volatile storage.
1. **Keys in flash and no privilege separation** (I.2e, I.2k): move keys into a secure element (done in the follow-up project) or use a part with memory protection.
