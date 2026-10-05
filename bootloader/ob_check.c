/* bootloader/ob_check.c - see ob_check.h */
#include "ob_check.h"

unsigned ob_rdp_level(uint32_t optr)
{
  uint32_t rdp = optr & OB_OPTR_RDP_MASK;

  if (rdp == OB_RDP_LEVEL0_VALUE) return 0U;
  if (rdp == OB_RDP_LEVEL2_VALUE) return 2U;
  return 1U;                      /* every other value, by design of the chip */
}

uint32_t ob_check(uint32_t optr, uint32_t wrprot1)
{
  uint32_t flags = OB_OK;

  if ((wrprot1 & OB_BOOTLOADER_WRP_MASK) != OB_BOOTLOADER_WRP_MASK) flags |= OB_ERR_WRP;
  if (optr & OB_OPTR_WPRMOD)                                       flags |= OB_ERR_WPRMOD;
  if (optr & OB_OPTR_BFB2)                                         flags |= OB_ERR_BFB2;
  if (ob_rdp_level(optr) == 0U)                                    flags |= OB_WARN_RDP0;

  return flags;
}

int ob_is_fatal(uint32_t flags, int require_rdp1)
{
  uint32_t fatal = OB_ERR_WRP | OB_ERR_WPRMOD | OB_ERR_BFB2;

  if (require_rdp1) fatal |= OB_WARN_RDP0;
  return (flags & fatal) != 0U;
}
