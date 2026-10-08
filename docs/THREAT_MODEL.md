# Threat model: STM32L0 FreeRTOS sensor node with secure bootloader

Design-level analysis written by the author on 8 October 2026, using STRIDE (Spoofing, Tampering, Repudiation, Information disclosure, Denial of service, Elevation of privilege) on each trust boundary. It is a self-assessment, not an independent review. "Evidence" points to tests and hardware results already in this repository; items marked *analysis* are reasoning I have not tested.

## Scope and attackers
**In scope:** the sensor node firmware, the bootloader, the encrypted LoRa link to the Feather receiver, and the update path.
**Out of scope:** the Feather receiver's own security (software keys, flash replay floor), side-channel attacks (timing, power) on the AES and ECDSA code, and the supply chain of the toolchain.

| Attacker | Capability |
|---|---|
| A1 Radio attacker | Receives everything on 868.1 MHz, can transmit, replay and jam. No physical access. |
| A2 Physical attacker | Holds the board: SWD debugger, USB power, can cut power at any moment. |
| A3 Build-environment attacker | Gets onto the development laptop that holds the signing key. |

## Assets
Link keys (encryption and integrity) · firmware signing private key · bootloader integrity · anti-rollback record (data EEPROM) · LoRa frame counter · authenticity and secrecy of the readings · availability of the node.

## Data flow and trust boundaries
```
 DS18B20 --(1-Wire, wire in reach of A2)--> [ STM32L072: app | bootloader ]
                                                |        ^
        ===== boundary B1: radio (A1) ======    |        | SWD / USB (A2), boundary B2
        v                                       |        |
 Feather M0 receiver (separate trust)           |   signed images made on the laptop (A3), boundary B3
```
Inside the chip the application and the bootloader share one privilege level (boundary B4 does not exist in hardware on this Cortex-M0+).

## Threats and mitigations
| ID | STRIDE | Threat | What stops it (evidence) | Residual risk |
|---|---|---|---|---|
| ST-01 | S, T | A1 forges or alters a packet | AES-CMAC MIC under a separate integrity key; the receiver checks the MIC first. Evidence: `secure_link/test` (FIPS-197 and the four RFC 4493 vectors, tamper tests). | The tag is 4 bytes, so a blind guess succeeds with probability 2^-32 per attempt (same trade-off as LoRaWAN). |
| ST-02 | S, T | A1 replays a captured packet | Strictly increasing frame counter; sender persists it (reserve-before-use, two power-fail-safe copies); receiver persists its replay floor before delivering anything. Evidence: counter-storage tests with simulated reboots and power cuts. | The receiver floor moves in steps of 100: after a receiver reboot up to 100 genuine packets can be rejected (documented availability trade-off). |
| ST-03 | I | A1 reads the readings | AES-128-CTR encryption. The receiver prints the raw bytes as an eavesdropper sees them. | Device id, counter, length and timing stay visible (metadata). |
| ST-04 | I | Keystream reuse after a counter reset | Found during development (identical packets after a reset); fixed by persisting the counter. | If counter state is lost (for example data EEPROM erased with a debugger), keystreams can repeat: rotate the link keys. The debug port has been locked since the lockdown stage. |
| ST-05 | D | A1 jams the channel or floods forged frames | Not mitigated (physical layer). The EU 1 % duty cycle keeps the node's own transmissions low. | Accepted. Each forged frame costs the receiver one MIC check. |
| ST-06 | T | A2 modifies the application in flash | Every boot verifies SHA-256 and an ECDSA P-256 signature (public key in the bootloader). Evidence: tampered image and a modified-and-re-hashed image refused on hardware; host tests use images from the real signing tool. | Boot time cost (about 2.2 s at 16 MHz). |
| ST-07 | T, E | A2 installs an older signed, vulnerable image | Anti-rollback minimum version in EEPROM, raised only after the signature check, repaired on every boot. Evidence: a correctly signed older image refused on hardware; power-cut tests found and fixed a stale-copy bug. | It compares version numbers, not contents: two different builds both labelled 1.1.0 would both be accepted. Needs release discipline. |
| ST-08 | D | A2 cuts power during an update | Slot B is kept until slot A is complete and compared. Evidence: 1,682 simulated cut points; a real USB power cut recovered on the next boot. | Overwrite updates cannot fall back if a correctly signed image is functionally broken (no swap-with-confirm). |
| ST-09 | T | A2 replaces or weakens the bootloader | Bootloader sectors write-protected (debugger erase refused at the first page; a signed update still installed); every boot checks the option bytes and halts if protection is weakened. | On this chip software can rewrite the option bytes, so removal is only detected at the next reset. With BFB2=1 the chip may boot bank 2 without running the bootloader (as I read the reference manual; not tested). |
| ST-10 | I | A2 reads the link keys with a debugger | Read-out protection Level 1: debugger reads of flash and EEPROM refused (hardware). Level 2 is never set. | Level 1 is not designed to resist fault injection and bypasses have been published. Keys sit in internal flash. The follow-up project moves them into a secure element. |
| ST-11 | E | A compromised application writes the EEPROM records or the option bytes | Not mitigated: no privilege separation on this Cortex-M0+. | Accepted; protection removal is caught at the next boot. |
| ST-12 | S, T | A3 steals the signing key and signs malicious images | Private key kept in a git-ignored folder; AES-256 gpg backup, restore-tested. | A compromised laptop means full signing power. A product needs an HSM or an offline signing machine. |
| ST-13 | T | A2 manipulates the sensor or its wiring so a false reading is sent and correctly authenticated | Not mitigated: the link protects the packet, not the truth of the measurement. | Accepted; would need tamper detection or sensor redundancy. |
| ST-14 | R | Nobody can later show what happened (rejected updates, failed boots) | Not implemented: messages go to the UART only, nothing is stored. | Gap. A product needs a persistent security event log. |
| ST-15 | D | Slow brown-out during flash writes | Not mitigated: brown-out reset is off (factory setting). | Production should enable it (one option-byte change). |
| ST-16 | D | Anything that weakens the protection stops the device from booting | Deliberate: the bootloader fails closed instead of self-repairing, because option-byte writing code is one bug from an unprogrammable chip. | Availability cost accepted. |
| ST-17 | T | Flaws in vendored or hand-written crypto | SHA-256 from FIPS 180-4 with FIPS vectors, cross-checked with Python; ECDSA verification from micro-ecc (pinned commit) with RFC 6979 vectors. | No independent review and no side-channel evaluation. |

## Top residual risks (what I would fix first for a product)
1. **Link keys in internal flash** behind RDP Level 1 (ST-10): move to a secure element.
2. **No in-field update path and no event log** (ST-14): receive images over UART or LoRa into slot B and record security events.
3. **Signing key on a laptop** (ST-12): HSM or offline signer.
