#include "DataModel.h"
#include "TimeSlice.h"

#include "Reformatter.h"

Reformatter::Reformatter(): Tool() {}

// See Table 2.3 in UM2580_DPSD_UserManual_rev9
static uint64_t time_from_seconds(long double seconds) {
  seconds /= 2e-9; // Tsampl
  uint64_t time = seconds; // Tcoarse
  seconds *= 1024;
  return time << 10
       | static_cast<uint64_t>(seconds) & 0x3ff; // Tfine
}

static long double time_to_seconds(uint64_t time) {
  return (
        static_cast<long double>(time >> 10)
      + static_cast<long double>(time & 0x3ff) / 1024.0L
  ) * 2e-9L;
}

inline static uint64_t decode_time(uint64_t time) {
  uint32_t tag    = time >> 32;
  uint32_t extras = time   & 0xffffffff;
  uint64_t result = extras & 0xffff0000; // bits 16 to 31
  result <<= 31 - 16;
  result |= tag;
  result <<= 10;
  result |= extras & 0x3ff; // bits 0 to 9
  return result;
}

// CAENDigitizer 2.17.3 coupled with DPP-PSD firmware version 136.137 (AMC)
// 04.25 (ROC) has a bug when the baseline (times 4) is returned as an int16_t
// rather than uint16_t, with the sign depending on the channel pulse polarity.
// This function decodes the proper baseline value.
inline static uint16_t decode_baseline(uint16_t baseline) {
  // XXX: currently assuming negative pulse polarity
#if 0
  // positive pulse polarity
  return (uint16_t)-baseline / 4;
#else
  // negative pulse polarity
  return baseline / 4;
#endif
}

void Reformatter::ThreadArgs::send(const std::vector<Hit>& hits) {
  std::unique_ptr<TimeSlice> timeslice(new TimeSlice);
  timeslice->hits = hits;
  std::lock_guard<std::mutex> lock(tool.m_data->readout_mutex);
  tool.m_data->readout.push(std::move(timeslice));
}

void Reformatter::ThreadArgs::execute() {
  if (!tool.m_data->raw_readout) return;

  /* We wait until the time of the earlist hit available for processing across
   * all channels (`time_min`) plus the desired timeslice length (`interval`)
   * is less than the time of the latest hit in the channel for all active
   * channels. A channel is active if we have seen data from it and its
   * digitizer is active (see DataModel::active_digitizers and the Digitizer
   * tool; basically a digitizer is active if it is responding). When we have
   * all the hits we need, they are extracted from `readouts` and separated
   * into `current` and `next` with `current` storing the hits fitting the time
   * window, and `next` storing the hits to be send next. `current` data is
   * then sent for processing.
   */

  {
    std::lock_guard<std::mutex> lock(tool.m_data->raw_readout_mutex);
    readouts.push_back(std::move(tool.m_data->raw_readout));
  };

  for (auto& board : *readouts.back())
    for (auto& hit : *board) {
      // Decode CAEN data format
      hit.time     = decode_time(hit.time);
      hit.baseline = decode_baseline(hit.baseline);

      if (hit.channel >= channels.size()) {
        // A new channel is seen. Initialize the `digitizer_active` fields
        auto i = channels.size();
        channels.resize(hit.channel + 1);
        for (; i < channels.size(); ++i)
          channels[i].digitizer_active
            = &tool.m_data->active_digitizers[Hit::get_digitizer_id(i)];
      };

      Channel& channel = channels[hit.channel];
      if (channel.active) {
        if (hit.time < channel.min)
          channel.min = hit.time;
        else if (hit.time > channel.max)
          channel.max = hit.time;
      } else {
        channel.active = true;
        channel.min = channel.max = hit.time;
      };
    };

  for (auto& channel : channels)
    if (channel.active) {
      if (channel.min < time_min)
        time_min = channel.min;
      else if (channel.max > time_max)
        time_max = channel.max;
    };

  // check the time window
  uint64_t end = time_min + tool.interval;
  for (auto& channel : channels)
    if (channel.active && channel.max < end)
      if (*channel.digitizer_active)
        // some channel may yet provide data fitting the current time window
        return;
      else
        // channel's digitizer went inactive
        channel.active = false;

  // No more hits to expect. Form the timeslice and send it down the toolchain

  // Reset the channels min times; they will be recalculated in a moment
  for (auto& channel : channels) channel.min = channel.max;

  // Prepare the timeslice hits. Store events hitting the time window in
  // current, the rest in next.
  for (auto& readout : readouts)
    for (auto& board : *readout)
      for (auto& hit : *board)
        if (hit.time < end)
          current->push_back(std::move(hit));
        else {
          next->push_back(std::move(hit));
          channels[hit.channel].min = std::min(
              channels[hit.channel].min, hit.time
          );
        };
  readouts.clear();

  // Copy the hits and send the timeslice
  send(*current);
  current->clear();
  std::swap(current, next);

  time_min = std::numeric_limits<uint64_t>().max();
  for (auto& channel : channels)
    if (channel.active)
      time_min = std::min(time_min, channel.min);
}

Reformatter::ThreadArgs::~ThreadArgs() {
  // Send the last hits for processing
  if (!next->empty()) send(*next);
}

void Reformatter::Thread(Thread_args* args) {
  static_cast<ThreadArgs*>(args)->execute();
}

bool Reformatter::Initialise(std::string configfile, DataModel& data) {
  InitialiseTool(data);
  InitialiseConfiguration(configfile);

  if (!m_variables.Get("verbose", m_verbose)) m_verbose = 1;

  long double time = 0.1;
  m_variables.Get("interval", time);
  interval = time_from_seconds(time);

  thread = new ThreadArgs(*this);
  util.CreateThread("Reformatter", &Thread, thread);

  ExportConfiguration();
  return true;
}

bool Reformatter::Execute() {
  return true;
}

bool Reformatter::Finalise() {
  util.KillThread(thread);
  delete thread;
  return true;
}
