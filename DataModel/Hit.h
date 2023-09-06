#ifndef HIT_H
#define HIT_H

#include <cstdint>

struct Hit {
  uint64_t time;
  uint16_t charge_short;
  uint16_t charge_long;
  uint16_t baseline;
  uint8_t  channel; // (digitizer channel) | (digitizer id) << 4
  std::vector<uint16_t> waveform;

  static uint8_t get_digitizer_id(uint8_t channel) {
    return channel >> 4;
  };
};

#endif
