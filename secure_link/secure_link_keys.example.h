/* secure_link_keys.example.h - shows the format only. These are NOT real keys.
 * Generate your own with:  python3 gen_keys.py
 * which writes secure_link_keys.h (git-ignored). */
#ifndef SECURE_LINK_KEYS_H
#define SECURE_LINK_KEYS_H

#include <stdint.h>

#define SL_DEV_ID 0x01U

static const uint8_t SL_ENC_KEY[16] = { 0 };
static const uint8_t SL_MIC_KEY[16] = { 0 };

#endif
