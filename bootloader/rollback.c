/* rollback.c - see rollback.h */
#include "rollback.h"

static int copy_read(const nv_backend *be, uint32_t k, uint32_t *v)
{
  uint32_t a = be->read(2U * k), b = be->read(2U * k + 1U);
  if (b != ~a) return 0;                /* torn, corrupted or never written (erased = 0, 0) */
  *v = a;
  return 1;
}

static int copy_write(const nv_backend *be, uint32_t k, uint32_t v)
{
  uint32_t check;
  if (be->write(2U * k, v) != 0) return -1;
  if (be->write(2U * k + 1U, ~v) != 0) return -1;
  if (!copy_read(be, k, &check) || check != v) return -1;   /* read back */
  return 0;
}

uint32_t rollback_min_version(const nv_backend *be)
{
  uint32_t a = 0, b = 0;
  int va = copy_read(be, 0, &a), vb = copy_read(be, 1, &b);
  if (va && vb) return (a > b) ? a : b;
  if (va) return a;
  if (vb) return b;
  return 0;
}

int rollback_raise(const nv_backend *be, uint32_t version)
{
  uint32_t a = 0, b = 0, target;
  int va = copy_read(be, 0, &a), vb = copy_read(be, 1, &b);

  target = rollback_min_version(be);
  if (version > target) target = version;                  /* never lowered */

  /* Nothing to do only if BOTH copies already hold the target. This also
   * repairs a copy left stale or damaged by an earlier power cut, so a later
   * corruption of the other copy cannot lower the minimum. */
  if (va && vb && a == target && b == target) return 0;

  if (copy_write(be, 0, target) != 0) return -1;
  if (copy_write(be, 1, target) != 0) return -1;
  return 0;
}
