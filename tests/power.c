#include "power.h"
#include <assert.h>
#include <string.h>

int main(void) {
  screen_power p;
  memset(&p, 0, sizeof(p));

  /* Rear-key short press: provision only after release. */
  assert(power_tick(&p, false, true, true, false) == 0);
  assert(power_tick(&p, false, false, true, false) == POWER_PROVISION);

  /* Rear-key long press: MCU reset at the 50th 33 ms sample (~1.65 s).
   * Releasing after the reset threshold must not also request provisioning. */
  memset(&p, 0, sizeof(p));
  unsigned event = 0;
  for (unsigned i = 0; i < 49; ++i)
    event |= power_tick(&p, false, true, true, false);
  assert((event & (POWER_RESET | POWER_PROVISION)) == 0);
  event = power_tick(&p, false, true, true, false);
  assert(event & POWER_RESET);
  assert((event & POWER_PROVISION) == 0);
  event = power_tick(&p, false, false, true, false);
  assert((event & POWER_PROVISION) == 0);

  /* A long press during an OTA transfer is ignored; it cannot interrupt the
   * UART maintenance owner or reset the MCU mid-image. */
  memset(&p, 0, sizeof(p));
  p.ota = true;
  for (unsigned i = 0; i < 80; ++i) {
    event = power_tick(&p, false, true, true, false);
    assert((event & (POWER_RESET | POWER_PROVISION)) == 0);
  }
  return 0;
}
