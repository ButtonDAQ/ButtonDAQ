#ifndef TIME_SLICE_H
#define TIME_SLICE_H

#include <vector>
#include <map>
#include <mutex>
#include <type_traits>

#include <cstddef>

#include "Hit.h"

enum class trigger_type {nhits, calib, zero_bais};  

struct TimeSlice {
  Time time;
  std::vector<Hit> hits;
  std::mutex mutex;
  std::vector<std::pair<trigger_type, unsigned long>> positive_trggers;
  std::map<trigger_type, bool> trigger_flags;
};

#endif
