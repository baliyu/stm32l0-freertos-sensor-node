/* update.h - install a new image from slot B into slot A, safely.
 *
 * Rules (overwrite update, source kept until the end):
 *   1. Slot B is checked completely (hash, signature, vector table for where
 *      it will RUN, i.e. slot A, and the anti-rollback minimum) BEFORE slot A
 *      is touched. A bad update is erased from B and slot A keeps running.
 *   2. B is copied into A page by page, then A is compared with B.
 *   3. Only then is B's header erased. Until that moment B still holds a
 *      complete, valid image, so a power cut at ANY point during the copy is
 *      recovered on the next boot simply by copying again.
 *
 * Limitation: once installed, the old version is gone. If the new firmware
 * is functionally broken (but correctly signed) there is no automatic way
 * back; that needs a swap with test-and-confirm, as MCUboot does.
 *
 * Flash access goes through flash_ops so the same logic runs on the STM32
 * and in PC tests that cut the power at every point of the copy.
 */
#ifndef UPDATE_H
#define UPDATE_H

#include <stdint.h>
#include "image.h"

#define FLASH_PAGE_SIZE 128U             /* STM32L0 program memory erase unit */

typedef struct {
  const uint8_t *(*ptr)(uint32_t addr);               /* read access to flash at addr */
  int  (*erase_page)(uint32_t addr);                   /* erase the 128-byte page at addr; 0 = ok */
  int  (*write_word)(uint32_t addr, uint32_t value);   /* program one erased word; 0 = ok */
  void (*progress)(uint32_t done, uint32_t total);     /* optional, may be NULL */
} flash_ops;

typedef enum {
  UPD_NONE = 0,       /* slot B empty: nothing to do */
  UPD_INSTALLED,      /* copied, checked and slot B cleared */
  UPD_ALREADY,        /* slot A already held this image (earlier power cut after the copy) */
  UPD_REJECTED,       /* slot B image refused (see why) and erased; slot A untouched */
  UPD_FAILED          /* flash error during the copy: slot B kept, retried next boot */
} upd_result;

/* Install slot B into slot A if slot B holds a valid, newer-than-minimum image. min_version is the lowest fw_version still accepted (see
 * rollback_min_version). *why receives the reason a slot B image was refused (IMG_OK otherwise); *version receives slot B's fw_version when it
 * has a header (0 when it is empty). */
upd_result update_from_slot_b(const flash_ops *f, const uint8_t pubkey[64], uint32_t min_version,
                              img_result *why, uint32_t *version);

/* Short text for an update result code, for the serial log. Returns a constant string. */
const char *update_result_str(upd_result r);

#endif
