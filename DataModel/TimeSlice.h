#ifndef TIME_SLICE_H
#define TIME_SLIDE_H

#include <vector>
#include <mutex>
#include <Hit.h>

enum class trigger_type {nhits, calib, zero_bais};  


class TimeSlice{

 public:
 
  std::vector<Hit>* hits;
  std::mutex mutex; 
  std::vector<std::pair<trigger_type, unsigned long> > positive_trggers;
  std::map<trigger_type, bool> trigger_flags;



};

#endif
