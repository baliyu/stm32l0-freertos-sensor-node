/* bootloader/ob_check.h - secure boot, stage 6: boot-time check of the chip's
 * protection settings (option bytes).
 *
 * Why: on the STM32L0 the option bytes can be rewritten by software. A buggy or
 * compromised application could remove the bootloader's write protection and
 * reset. The bootloader therefore re-checks the settings on every boot and
 * refuses to continue (fail closed) if they are weaker than expected.
 *
 * The logic here is pure (register values in, verdict out) so it can be tested
 * on the PC. main.c reads the two registers and acts on the result.
 */
#ifndef OB_CHECK_H
#define OB_CHECK_H

#include <stdint.h>

/* Loaded option-byte registers (RM0376, flash interface at 0x40022000).
 * Verified on the board in stage 6.1/6.2: OPTR=0x807000AA, WRPROT1=0x0000003F. */
#define OB_FLASH_OPTR_ADDR      0x4002201CUL
#define OB_FLASH_WRPROT1_ADDR   0x40022020UL

/* Bootloader region: sectors 0-5 = 0x08000000-0x08005FFF (24 KB, 4 KB each) */
#define OB_BOOTLOADER_WRP_MASK  0x0000003FUL

/* FLASH_OPTR fields */
#define OB_OPTR_RDP_MASK        0x000000FFUL   /* AA = level 0, CC = level 2, else level 1 */
#define OB_OPTR_WPRMOD          (1UL << 8)     /* 1 turns the WRPROT bits into PCROP */
#define OB_OPTR_BFB2            (1UL << 23)    /* 1 = boot from bank 2 (= slot B area) */

#define OB_RDP_LEVEL0_VALUE     0xAAU
#define OB_RDP_LEVEL2_VALUE     0xCCU

/* Result flags (a bit set means "this is wrong") */
#define OB_OK                   0x0U
#define OB_ERR_WRP              (1U << 0)  /* one or more bootloader sectors not write-protected */
#define OB_ERR_WPRMOD           (1U << 1)  /* WRPROT bits mean PCROP, not write protection */
#define OB_ERR_BFB2             (1U << 2)  /* boot path can skip this bootloader */
#define OB_WARN_RDP0            (1U << 3)  /* read-out protection off (debugger has full access) */

/* 0, 1 or 2 */
unsigned ob_rdp_level(uint32_t optr);

/* Returns a combination of the OB_ERR_* / OB_WARN_* flags, OB_OK if all is well.
 * Extra protected sectors beyond 0-5 are allowed. */
uint32_t ob_check(uint32_t optr, uint32_t wrprot1);

/* Non-zero if the bootloader must refuse to continue.
 * require_rdp1 = 0 during development (level 0 only warns),
 *              = 1 for production builds (level 0 is fatal). */
int ob_is_fatal(uint32_t flags, int require_rdp1);

#endif
