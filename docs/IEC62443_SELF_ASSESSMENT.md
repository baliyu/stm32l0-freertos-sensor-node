# IEC 62443 self-assessment: STM32L0 sensor node

Self-assessment by the author, 8 October 2026. **It is not a certification, not an independent assessment, and it does not claim conformity or any security level.**

## How to read this

- **IEC 62443 is a paid standard and I have not read the licensed text.** I used public summaries of its structure and requirement titles (listed at the end). The ratings are my engineering judgement of what each requirement is aimed at and must be checked against the standard itself before any claim is made.
- **Component type:** the node is a small device with firmware, which fits IEC 62443-4-2's *embedded device* category (the requirements numbered EDR) on top of the common component requirements (CR). The Feather receiver is out of scope.
- **Two parts are covered:** 4-2 (technical requirements for the component, grouped under seven foundational requirements) and 4-1 (the eight secure product development practices). A component can only be certified to 4-2 if it was built under a 4-1 compliant lifecycle.
- **Security levels:** the standard defines levels 1 to 4 for increasingly capable attackers, and the requirements are incremental. I did not assign a level. Requirements that are enhancements are shown as `RE`.
- **Status:** **Met** / **Partly** (something required is missing) / **Not met** / **Not assessed** (I have not checked) / **Not applicable** (with the reason).
- Evidence points to this repository's README, `THREAT_MODEL.md` and `docs/secure_boot/`; the CRA view of the same project is in `CRA_MAPPING.md`.

## Result

**4-2 component requirements** (54 rows): 8 met, 23 partly, 16 not met, 1 not assessed, 6 not applicable.
**4-1 practices** (8): 0 met, 8 partly, 0 not met.

The strongest area is **system integrity** (FR3): signed boot, authenticated updates, anti-rollback and authenticated radio frames. The weakest is **logging and monitoring** (the audit requirements in FR2, FR3 and FR6), where nothing is recorded.

## Part 1: IEC 62443-4-2 component requirements (embedded device)

### FR1 Identification and authentication control (IAC)

| Requirement | Title | Status | Evidence or gap |
|---|---|---|---|
| CR 1.1, 1.3, 1.7, 1.10, 1.11, 1.12 | Human-user items (user identification, accounts, passwords, authenticator feedback, login attempts, use notification) | **Not applicable** | The node has no human users, accounts or login interface. Its inputs are a button and the radio. |
| CR 1.2 | Software process and device identification and authentication | **Partly** | The node is identified by its device id and authenticated by an AES-CMAC under its link key; the bootloader authenticates the firmware image by ECDSA signature. The node does not authenticate the receiver (transmit-only). |
| CR 1.4 | Identifier management | **Partly** | The device id is one byte assigned at build time. No process manages identifiers. |
| CR 1.5 | Authenticator management | **Partly** | Link keys come from `gen_keys.py`, are kept out of Git and have an encrypted backup; the signing key stays off the device. Keys are compiled into the app, so changing one means a re-flash. |
| CR 1.5 RE1 | Hardware security for authenticators | **Not met** | The link keys sit in internal flash behind read-out protection Level 1, not in a hardware key store. The follow-up project (MKR WAN with ATECC608) does this. |
| CR 1.8 | Public key infrastructure certificates | **Not applicable** | No certificates are used; the bootloader holds a bare ECDSA public key. |
| CR 1.9 | Strength of public key-based authentication | **Partly** | ECDSA P-256 verification against a public key built into the write-protected bootloader, tested with RFC 6979 vectors. No certificate chain, revocation or key rollover. |
| CR 1.9 RE1 | Hardware security for public key-based authentication | **Partly** | A public key needs integrity, not secrecy: it sits in write-protected flash. It is not in a hardware store. |
| CR 1.14 | Strength of symmetric key-based authentication | **Partly** | AES-128-CMAC with separate encryption and integrity keys; 4-byte tag (2^-32 guess chance). Known-answer tests against RFC 4493. |
| CR 1.14 RE1 | Hardware security for symmetric key-based authentication | **Not met** | Same as CR 1.5 RE1: keys in flash behind RDP Level 1. |

### FR2 Use control (UC)

| Requirement | Title | Status | Evidence or gap |
|---|---|---|---|
| CR 2.1 | Authorization enforcement | **Partly** | Updates are authorised by signature; the debug port is locked. The application runs with full privileges (no roles, no memory protection on the Cortex-M0+). |
| CR 2.2 | Wireless use control | **Partly** | The radio is transmit-only and every packet is authenticated and encrypted. No monitoring or explicit usage policy; the EU duty cycle is respected. |
| CR 2.3, 2.5, 2.6, 2.7, EDR 2.4 | Portable devices, session lock, remote session termination, concurrent sessions, mobile code | **Not applicable** | No portable-device use, no sessions and no mobile code. |
| CR 2.8 | Auditable events | **Not met** | Rejected updates and failed checks are printed on the UART; nothing is recorded. |
| CR 2.9, 2.10 | Audit storage capacity; response to audit processing failures | **Not met** | There is no audit storage. |
| CR 2.11 | Timestamps | **Not met** | No real-time clock or time source. |
| CR 2.12 | Non-repudiation | **Not met** | No signed or recorded actions. |
| EDR 2.13 | Use of physical diagnostic and test interfaces | **Partly** | Read-out protection Level 1: debugger reads of flash and EEPROM refused (hardware-verified); bootloader sectors write-protected. Level 1 is not designed to resist fault injection. |
| EDR 2.13 RE1 | Active monitoring (of diagnostic interfaces) | **Not met** | Debug access attempts are not detected or reported. |

### FR3 System integrity (SI)

| Requirement | Title | Status | Evidence or gap |
|---|---|---|---|
| CR 3.1 | Communication integrity | **Met** | AES-CMAC over device id, counter and ciphertext, checked before anything else; tamper and replay tests plus RFC 4493 vectors. Caveat: 4-byte tag. |
| CR 3.1 RE1 | Communication authentication | **Met** | The same MIC authenticates the sender, who holds the integrity key. |
| EDR 3.2 | Protection from malicious code | **Partly** | Only signed firmware runs after boot. Nothing protects the system once running (no memory protection). |
| CR 3.3 | Security functionality verification | **Partly** | The bootloader checks its protection settings (option bytes) and every image on each boot; host tests cover the logic. No on-demand self-test. |
| CR 3.3 RE1 | Security functionality verification during normal operation | **Not met** | Checks happen at boot only. |
| CR 3.4 | Software and information integrity | **Met** | SHA-256 over header fields and app on every boot; anti-rollback record and frame counter kept in redundant copies and repaired at boot. |
| CR 3.4 RE1 | Authenticity of software and information | **Met** | ECDSA P-256 signature checked on every boot and before every update; unsigned and re-hashed images refused on hardware. |
| CR 3.4 RE2 | Automated notification of integrity violations | **Not met** | A message on the UART only; nothing is reported to another system. |
| CR 3.5 | Input validation | **Partly** | Packets are validated by MIC and counter; image header, vector table and version are validated. No fuzzing. |
| CR 3.6, 3.8 | Deterministic output; session integrity | **Not applicable** | The node controls no process and has no sessions. |
| CR 3.7 | Error handling | **Partly** | Failed boot checks halt with a message; update failures leave slot A untouched. Not reviewed for information leaks. |
| CR 3.9 | Protection of audit information | **Not applicable** | There is no audit information yet; it applies once a log exists. |
| EDR 3.10 | Support for updates | **Partly** | The bootloader installs signed updates safely (power-fail-safe, anti-rollback). At RDP Level 1 the board cannot receive one in the field. |
| EDR 3.10 RE1 | Update authenticity and integrity | **Met** | A slot B image must pass hash, signature, vector-table and version checks before slot A is touched. |
| EDR 3.11 | Physical tamper resistance and detection | **Not met** | No tamper detection or enclosure. Write protection and RDP Level 1 resist but do not detect. |
| EDR 3.11 RE1 | Notification of a tampering attempt | **Not met** | Nothing detects an attempt. |
| EDR 3.12 | Provisioning product supplier roots of trust | **Partly** | The public key is compiled into the write-protected bootloader. Custody of the signing key is a laptop. |
| EDR 3.13 | Provisioning asset owner roots of trust | **Not met** | An asset owner cannot install their own key; it is fixed at build time. |
| EDR 3.14 | Integrity of the boot process | **Met** | The bootloader verifies the firmware (hash, signature) before running it and checks the protection settings first. Caveat: the first stage is protected by flash write protection, not by an immutable ROM. |
| EDR 3.14 RE1 | Authenticity of the boot process | **Met** | ECDSA signature verification before every boot. Same caveat about the root of trust, and the BFB2 question in the threat model (ST-09). |

### FR4 Data confidentiality (DC)

| Requirement | Title | Status | Evidence or gap |
|---|---|---|---|
| CR 4.1 | Information confidentiality | **Partly** | Radio payloads use AES-128-CTR. Metadata (device id, counter, length, timing) is visible. Keys at rest are protected by RDP Level 1 only. |
| CR 4.2 (+RE1, RE2) | Information persistence (erasing shared memory resources, with verification) | **Not assessed** | I have not checked whether RAM holding keys or plaintext is cleared after use. |
| CR 4.3 | Use of cryptography | **Partly** | Standard algorithms (AES-128, AES-CMAC, ECDSA P-256, SHA-256) with known-answer tests. No documented key lifecycle (rotation, revocation) and no side-channel evaluation. |

### FR5 Restricted data flow (RDF)

| Requirement | Title | Status | Evidence or gap |
|---|---|---|---|
| CR 5.1 | Network segmentation | **Not applicable** | A transmit-only radio node has no networks to segment. (Not cross-checked against a full list of RDF requirements.) |

### FR6 Timely response to events (TRE)

| Requirement | Title | Status | Evidence or gap |
|---|---|---|---|
| CR 6.1 | Audit log accessibility | **Not met** | There is no log to access. |
| CR 6.2 | Continuous monitoring | **Not met** | Only the watchdog; no security monitoring. (Not cross-checked against a full list of TRE requirements.) |

### FR7 Resource availability (RA)

| Requirement | Title | Status | Evidence or gap |
|---|---|---|---|
| CR 7.1 | Denial of service protection | **Partly** | Heartbeat watchdog verified by fault injection; the fail-closed boot protects integrity at the cost of availability. Radio jamming is not addressed. |
| CR 7.1 RE1 | Manage communication load from component | **Met** | One 16-byte packet about every 11 s, inside the EU 1 % duty cycle. |
| CR 7.2 | Resource management | **Partly** | Fixed FreeRTOS tasks and queues, tickless idle. No analysis of exhaustion limits. |
| CR 7.3 (+RE1) | Control system backup (and backup integrity verification) | **Not met** | No device function backs up configuration. The keys have an encrypted off-device backup, which is a procedure, not a device capability. |
| CR 7.4 | Control system recovery and reconstitution | **Partly** | Power-fail-safe updates (1,682 cut points) and self-repairing anti-rollback copies. A functionally broken signed image cannot fall back (no swap-with-confirm). |
| CR 7.6 | Network and security configuration settings | **Partly** | Flash layout, option bytes and lockdown steps are documented and checked at every boot. |
| CR 7.6 RE1 | Machine-readable reporting of current security settings | **Not met** | The boot check prints human-readable text on the UART only. |
| CR 7.7 | Least functionality | **Partly** | Only the needed peripherals; no network stack; debug locked. The UART console remains. |
| CR 7.8 | Control system component inventory | **Partly** | Components are named in the README (FreeRTOS, ST HAL, micro-ecc at a pinned commit). No machine-readable inventory. |

## Part 2: IEC 62443-4-1 secure development practices

| Practice | Title | Status | Evidence or gap |
|---|---|---|---|
| SM | Security management (management of development) | **Partly** | One-person project: the process is captured in the README and lessons learned. No formal roles, training or documented lifecycle. |
| SR | Specification of security requirements | **Partly** | Security goals are written down (README, `THREAT_MODEL.md`). There is no formal requirements specification with traceability to tests. |
| SD | Secure by design | **Partly** | Threat model, defence in depth (signature, anti-rollback, write protection, read-out protection, fail-closed checks) and documented residual risks. No formal design review. |
| SI | Secure implementation | **Partly** | Crypto written from the standards and tested against known answers; third-party code pinned to a commit. No coding standard or static analysis applied. |
| SVV | Security verification and validation testing | **Partly** | Known-answer tests, simulated power-cut tests (1,682 points), hardware verification of every lockdown step with evidence in `docs/secure_boot/`. No fuzzing, penetration test or independent testers. |
| DM | Management of security-related issues | **Partly** | `SECURITY.md` gives a reporting route, response targets and a disclosure policy. No tracking workflow or severity process, and no issue has been reported yet. |
| SUM | Security update management | **Partly** | A signed, rollback-protected, power-fail-safe update mechanism exists. There is no in-field delivery path and no patch policy for deployed units. |
| SG | Security guidelines | **Partly** | README covers build, signing and lockdown with guarded scripts, and lists known limitations. No dedicated guide for integrators (secure deployment, key rotation). |

## What I would do about the gaps

1. **Security event log** (CR 2.8 to 2.12, 3.9, 6.1, 6.2): record rejected updates, failed boot checks and counter events in non-volatile storage, with a counter or time source. This is the largest single cluster of gaps.
2. **Hardware key protection** (CR 1.5 RE1, 1.9 RE1, 1.14 RE1): move the link keys into a secure element. The follow-up project does this on another board; this board keeps them in flash.
3. **In-field updates and asset-owner keys** (EDR 3.10, 3.13): receive images over UART or LoRa into slot B and let the owner provision their own verification key.
4. **Inventory and issue process** (CR 7.8, practice DM): produce a machine-readable component list and a tracked issue workflow.

## Sources for the requirement titles

- Public requirement lists for 62443-4-2 (identification and authentication, use control, system integrity, data confidentiality and resource availability groups), from a vendor's published conformity documentation.
- ISA Security Compliance Institute webinar slides on 62443-4-2 (EDR 2.13, EDR 3.10 to 3.14).
- Public overviews of the standard's structure: the four component categories (software application, embedded device, host device, network device), the seven foundational requirements, and the eight 4-1 practices.
- Not cross-checked against a full list: the restricted data flow (RDF) and timely response to events (TRE) requirement titles.
