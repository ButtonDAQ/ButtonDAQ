#include "DataModel.h"
#include "TimeSlice.h"

#include "Reformatter.h"

Reformatter::Reformatter(): Tool() {}

void Reformatter::send_timeslice(Time time, std::vector<Hit>& hits) {
  if (hits.empty()) return;

  std::unique_ptr<TimeSlice> timeslice(new TimeSlice);
  timeslice->time = time;
  timeslice->hits.insert(
      timeslice->hits.begin(),
      std::make_move_iterator(hits.begin()),
      std::make_move_iterator(hits.end())
  );
  hits.clear();

  std::lock_guard<std::mutex> lock(m_data->readout_mutex);
  m_data->readout.push(std::move(timeslice));
};

template <typename Container>
static void remove_if(
    Container& c,
    const std::function<bool (typename Container::value_type&)>& predicate
) {
  c.erase(std::remove_if(c.begin(), c.end(), predicate), c.end());
};

void Reformatter::configure() {
  long double time = 0.1;
  m_variables.Get("interval", time);
  if (time <= 0) {
    *m_data->Log
      << ML(0) << "Reformatter: invalid time interval: " << time
      << ", using 0.1 s" << std::endl;
    time = 0.1;
  };
  interval = Time(time);

  dead_time = 10 * interval;
  if (m_variables.Get("dead_time", time)) {
    if (time <= 0) {
      *m_data->Log << ML(0) << "Reformatter: invalid dead time: " << time;
      time = 10 * interval.seconds();
      *m_data->Log << ", using " << time << " s" << std::endl;
    };
    dead_time = Time(time) + interval;
  };

  channels.resize(m_data->enabled_digitizer_channels.size() * 16);
  {
    int i = 0;
    for (uint16_t mask : m_data->enabled_digitizer_channels)
      for (int j = 0; j < 16; ++j)
        channels[i++].active = mask & 1 << j;
  };
};

void Reformatter::reformat() {
  /* We wait until the time of the earlist hit available for processing across
   * all channels (`start`) plus the desired timeslice length (`interval`) is
   * less than the time of the latest hit in the channel for all active
   * channels. A channel is active if we have seen data from it not too long
   * ago. When we have all the hits we need, they are extracted from `readouts`
   * and separated into `current` and `next` with `current` storing the hits
   * fitting the time window, and `next` storing the hits to be send next.
   * `current` data is then sent for processing.
   */

  Time start;
  Time time;

  while (reformatting) {
    if (m_data->raw_readout) {
      {
        std::lock_guard<std::mutex> lock(m_data->raw_readout_mutex);
        readouts.push_back(std::move(m_data->raw_readout));
      };

      for (auto& board : *readouts.back())
        for (auto& hit : *board) {
          if (hit.channel >= channels.size()) {
            std::stringstream ss;
            ss << "Unexpected hit channel " << static_cast<int>(hit.channel);
            throw std::runtime_error(ss.str());
          };

          Channel& channel = channels[hit.channel];
          if (channel.active) {
            if (hit.time > channel.time) channel.time = hit.time;
          } else {
            channel.active = true;
            channel.time = hit.time;
          };
        };
    } else {
      usleep(interval.seconds() * 0.5e6);
      continue;
    };

    for (auto& channel : channels)
      if (channel.time > time)
        time = channel.time;

    while (true) {
      Time end = start + interval;
      if (time < end) break;

      bool done = false;
      for (auto& channel : channels)
        if (channel.active && channel.time < end)
          if (channel.time + dead_time > time) {
            // some channel may yet provide data fitting the current time
            // window
            done = true;
            break;
          } else {
            channel.active = false;
            done = done && channel.time >= start;
          };
      if (done) break;

      remove_if(
          readouts,
          [this, end](
            std::unique_ptr<
              std::list<std::unique_ptr<std::vector<Hit>>>
            >& readout
          ) -> bool {
            remove_if(
                *readout,
                [this, end](std::unique_ptr<std::vector<Hit>>& board) -> bool {
                  remove_if(
                      *board,
                      [this, end](Hit& hit) -> bool {
                        if (hit.time >= end) return false;
                        buffer.push_back(std::move(hit));
                        return true;
                      }
                  );
                  return board->empty();
                }
            );
            return readout->empty();
          }
      );

      send_timeslice(start, buffer);

      start = end;
    };
  };
};

void Reformatter::start_reformatting() {
  reformatting = true;
  thread = std::thread(&Reformatter::reformat, this);
};

void Reformatter::stop_reformatting() {
  reformatting = false;
  thread.join();
};

bool Reformatter::Initialise(std::string configfile, DataModel& data) {
  InitialiseTool(data);
  InitialiseConfiguration(configfile);

  if (!m_variables.Get("verbose", m_verbose)) m_verbose = 1;

  configure();

  ExportConfiguration();
  return true;
}

bool Reformatter::Execute() {
  if (m_data->run_stop && reformatting) stop_reformatting();

  if (m_data->change_config) {
    bool ref = reformatting;
    if (ref) stop_reformatting();

    InitialiseConfiguration();

    configure();

    if (ref) start_reformatting();
  };

  if (m_data->run_start && !reformatting) start_reformatting();

  return true;
}

bool Reformatter::Finalise() {
  if (reformatting) stop_reformatting();
  return true;
}
