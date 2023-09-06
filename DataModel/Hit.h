#ifndef HIT_H
#define HIT_H

#include <cstdint>

struct CAENEvent {
  uint32_t TimeTag;
  uint32_t Extras;
  uint16_t ChargeShort;
  uint16_t ChargeLong;
  int16_t  Baseline;
  uint8_t  channel; // (channel number) | (digitizer number) << 4
  std::vector<uint16_t> waveform;
};


struct Hit{

  ///for you to fill out Evgenii
  long time;




};



#endif
