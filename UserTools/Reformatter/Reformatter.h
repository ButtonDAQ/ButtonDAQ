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
      Time time; // time of the last hit in the channel
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
    Time interval;

    // max time to wait for data from a channel
    Time dead_time;

    bool reformatting = false;
    std::thread thread;

    void configure();

    void start_reformatting();
    void stop_reformatting();

    void send_timeslice(Time time, std::vector<Hit>& hits);
    void reformat();
};

#endif
