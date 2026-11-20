# Sequence diagrams: boot and secure link

Two diagrams, drawn from the README and `docs/API.md`. They are separate on purpose: a new image reaches slot B by debugger flash, not over LoRa (the in-field update path over UART or LoRa is on the not-done list in the README). Each message carries a tag: **hardware** (the README says it ran on the board), **host test** (checked by the PC tests) or **README** (stated there, and I have not separately checked it here).

## 1. Boot, every reset

```mermaid
sequenceDiagram
    participant B as Bootloader
    participant OB as Option bytes
    participant SB as Slot B
    participant SA as Slot A
    participant EE as Data EEPROM
    participant App
    B->>OB: read protection settings (README)
    OB-->>B: sectors 0-5 protected, WPRMOD=0, BFB2=0
    alt a setting is weak
        B->>B: print which one and halt before touching any image (README)
    end
    B->>SB: does slot B hold an image?
    alt image present
        B->>SB: check hash, signature, vector table (host test)
        B->>EE: read minimum version
        alt all checks pass
            B->>SA: copy into slot A, page by page (hardware)
            B->>SA: compare with slot B (README)
            B->>SB: clear slot B (README)
        else a check fails
            B->>SB: erase slot B, slot A untouched (README)
        end
    end
    B->>SA: check header, SHA-256, ECDSA P-256 signature (host test)
    B->>EE: raise minimum version after the signature passes (host test)
    Note over B,EE: a correctly signed older image was refused on hardware
    B->>App: reset peripherals, restore clock, set VTOR, jump (README)
```

## 2. Secure link, one reading

```mermaid
sequenceDiagram
    participant S as STM32L0 sender
    participant E as Data EEPROM
    participant F as Feather M0 receiver
    participant FL as Receiver flash
    S->>S: read the DS18B20 temperature
    S->>E: reserve a block of 100 frame counters before using any (README)
    S->>S: AES-128-CTR encrypt, AES-CMAC MIC of 4 bytes (host test)
    S->>F: LoRa 868.1 MHz, dev_id, fcnt, ciphertext, MIC (README)
    F->>F: check the MIC first (host test)
    alt MIC invalid
        F->>F: reject and print the reason
    else MIC valid
        F->>F: is the frame counter newer than the replay floor? (host test)
        alt not newer
            F->>F: reject as replay
        else newer
            F->>FL: store the new floor, two copies, before delivery (host test)
            F->>F: deliver the reading
        end
    end
```

## Where the tags come from

| Tag | Evidence in the README |
|---|---|
| hardware, slot B copy | a signed update v1.3.0 to v1.3.1 still installed through slot B with the bootloader sectors write-protected |
| hardware, older image | a correctly signed older image was refused on hardware (stage 4) |
| host test, boot checks | SHA-256, ECDSA (RFC 6979 vectors), image checks with images made by the real signing tool, anti-rollback with simulated power cuts |
| host test, link | AES-128 against FIPS-197, AES-CMAC against the four RFC 4493 vectors, packet tamper and replay tests, counter-storage tests with simulated reboots and power cuts |

## Not drawn

- An in-field update over UART or LoRa into slot B. Not built.
- Swap-with-confirm updates. Not built, so a correctly signed but broken image cannot fall back.
- What happens when slot A fails its check. Not read for this diagram.
- The BFB2 bypass: with BFB2=1 the chip can boot bank 2 and skip the bootloader. The README describes it as a gap in the boot-time check.
- The receiver replay floor is stored in steps of 100, so after a receiver reboot up to 100 genuine packets may be rejected.

## Predictions made while drawing these diagrams

I wrote each prediction as a chance before reading the answer. All 15 were hits. Several were easy checks, so this says little about skill.

| Step | Prediction | Chance | Outcome |
|---|---|---|---|
| Find the repo | The repo is at /mnt/c/stm32/l072_blinky | 85% | Hit |
| Read the README | The docs name the cipher mode | 60% | Hit (AES-128-CTR, AES-CMAC MIC) |
| Read the README | The docs describe the receiver rejecting a replayed frame | 75% | Hit |
| Read the README | The docs describe how a new image gets into slot B | 55% | Hit (debugger flash; the UART and LoRa path is not built) |
| Read the boot order | README and API.md list the same steps in the same order | 70% | Hit |
| Read the boot order | The option-byte check comes before the slot B check | 80% | Hit |
| Read the link rules | The receiver stores the replay floor before delivering | 85% | Hit |
| Read the link rules | The sender sends only after its counter block is stored | 60% | Hit, as I read "reserve-before-use" |
| Write the file | The file is created and git status shows only that file | 93% | Hit |
| Write the file | No em dash in the file | 97% | Hit |
| Render | Both diagrams render on GitHub first time | 70% | Hit |
| Commit | The README anchor is found exactly once | 80% | Hit |
| Commit | git status shows README modified and the new file before the commit | 93% | Hit |
| Commit | The commit succeeds with one new commit on top of 81821ce | 97% | Hit |
| Push | The push succeeds and main is level with origin/main | 97% | Hit |
