#ifndef TIME_SLICE_H
#define TIME_SLIDE_H

#include <vector>
#include <mutex>

class TimeSlice{

 public:
 
  std::vector<Hit> hits;
  std::mutex mutex; 
  std::vector<std::pair<int,long> > positive_trggers




};

#endif
