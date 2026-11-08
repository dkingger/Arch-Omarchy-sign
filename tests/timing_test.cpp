#include "timing.h"
#include <assert.h>
#include <initializer_list>
#include <stdint.h>

int main() {
  for (uint32_t interval : {500u, 15000u, 30000u}) {
    // Exercise the exact firmware helper, before and across millis rollover.
    for (uint32_t start : {0u, 100u, UINT32_MAX - 250u, UINT32_MAX - 10000u}) {
      assert(!timerExpired(start, start, interval));
      assert(!timerExpired(static_cast<uint32_t>(start + interval - 1), start, interval));
      assert(timerExpired(static_cast<uint32_t>(start + interval), start, interval));
      assert(timerExpired(static_cast<uint32_t>(start + interval + 1), start, interval));
    }
  }
}
