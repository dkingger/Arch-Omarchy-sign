#pragma once
#include <stdint.h>

// Unsigned subtraction remains valid across the 32-bit millis() rollover.
constexpr bool timerExpired(uint32_t now, uint32_t since, uint32_t interval) {
  return static_cast<uint32_t>(now - since) >= interval;
}
