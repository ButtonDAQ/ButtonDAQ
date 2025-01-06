#ifndef Reformatter_H
#define Reformatter_H

#include <string>
#include <iostream>

#include "Tool.h"

class Reformatter: public ToolFramework::Tool {
  public:
    Reformatter();
    ~Reformatter() { Finalise(); };

    bool Initialise(std::string configfile, DataModel& data);
    bool Execute();
    bool Finalise();

  private:
    struct Channel {
      uint64_t time;  // time of the last hit in the channel
      bool active; // whether we expect hits in the channel
    };

    std::vector<Channel> channels;

    std::vector<Hit> buffer;

    std::vector<
      std::unique_ptr<
        std::list<
          std::unique_ptr<
            std::vector<Hit>
          >
        >
      >
    > readouts;

    // target timeslice length
    uint64_t interval;

    // max time to wait for data from a channel
    uint64_t dead_time;

    bool stop = true;
    std::thread thread;

    void send_timeslice(std::vector<Hit>& hits);
    void reformat();
};

#endif
