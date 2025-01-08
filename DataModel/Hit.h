#ifndef HIT_H
#define HIT_H

#include <cstdint>

// Stores time in 64 bits as a fixed point value with 1/512 ns precision.
// Bits 10 to 64 store an integer number of ticks, each tick is 2 ns.
// Bits 0 to 9 store sub-ns fraction.
// CAEN digitizer time range is 57 bits in this format.
class Time {
  public:
    Time(): time(0) {};
    Time(const Time& t): time(t.time) {};

    // See Table 2.3 in UM2580_DPSD_UserManual_rev9
    explicit Time(long double seconds) {
      seconds /= 2e-9; // Tsampl
      time = seconds; // Tcoarse
      seconds *= 1024;
      time = time << 10 | static_cast<uint64_t>(seconds) & 0x3ff; // Tfine
    };

    // Construct Time from CAEN digitizer output data.
    // tag is the trigger time tag, extras provides the most and the least significant bits.
    // See "Channel aggregate data format" in DPP-PSD documentation.
    Time(uint32_t tag, uint32_t extras) {
      time = extras & 0xffff0000; // bits 16 to 31
      time <<= 31 - 16;
      time |= tag;
      time <<= 10;
      time |= extras & 0x3ff; // bits 0 to 9
    };

    double seconds() const {
      return (
            static_cast<long double>(time >> 10)
          + static_cast<long double>(time & 0x3ff) / 1024.0L
      ) * 2e-9L;
    };

    uint64_t bits() const {
      return time;
    };

    Time& operator=(Time t) {
      time = t.time;
      return *this;
    };

    bool operator==(Time t) const {
      return time == t.time;
    };

    bool operator!=(Time t) const {
      return time != t.time;
    };

    bool operator>(Time t) const {
      return time > t.time;
    };

    bool operator>=(Time t) const {
      return time >= t.time;
    };

    bool operator<(Time t) const {
      return time < t.time;
    };

    bool operator<=(Time t) const {
      return time <= t.time;
    };

    Time operator+(Time t) const {
      return Time(time + t.time);
    };

    Time& operator+=(Time t) {
      time += t.time;
      return *this;
    };

    Time operator-(Time t) const {
      return Time(time - t.time);
    };

    Time& operator-=(Time t) {
      time -= t.time;
      return *this;
    };

    template <typename Number>
    typename std::enable_if<std::is_arithmetic<Number>::value, Time>::type
    operator*(Number x) const {
      return Time(time * x);
    };

    template <typename Number>
    typename std::enable_if<std::is_arithmetic<Number>::value, Time&>::type
    operator*=(Number x) {
      time *= x;
      return *this;
    };

    template <typename Number>
    typename std::enable_if<std::is_arithmetic<Number>::value, Time>::type
    operator/(Number x) const {
      return Time(time / x);
    };

    template <typename Number>
    typename std::enable_if<std::is_arithmetic<Number>::value, Time&>::type
    operator/=(Number x) {
      time /= x;
      return *this;
    };

  private:
    uint64_t time;

    explicit Time(uint64_t time): time(time) {};
};

template <typename Number>
typename std::enable_if<std::is_arithmetic<Number>::value, Time>::type
operator*(Number x, Time t) {
  return t * x;
};

struct Hit {
  Time     time;
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
