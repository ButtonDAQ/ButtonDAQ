#ifndef HVoltage_H
#define HVoltage_H

#include <string>
#include <iostream>
#include <chrono>

#include "caen++/v6534.hpp"

#include "Tool.h"
#include "Services.h"
#include "HandleList.h"

class HVoltage: public ToolFramework::Tool {
  public:
    struct Board {
      caen::V6534 hv;
      std::mutex  mutex;

      Board(caen::V6534 hv): hv(std::move(hv)) {};

      Board(Board&&) = default;
    };

    class Monitor {
      public:
        struct Channel {
          float   voltage;
          float   current;
          float   voltage_setting;
          float   current_setting;
          bool    power;
          int16_t temperature;
          int8_t  status;
        };

        Monitor(
            DataModel&              data,
            std::list<Board>&       boards,
            std::chrono::seconds    interval,
            ToolFramework::Logging& log
        );
        ~Monitor();

        void set_interval(std::chrono::seconds interval);

      private:
        ToolFramework::Services&              services;
        ToolFramework::SlowControlCollection& ui;
        std::list<Board>&                     boards;
        std::vector<std::array<Channel, 6>>   channels;
        int8_t                                status;
        std::chrono::seconds                  interval;
        std::thread                           thread;
        std::mutex                            readout_mutex;
        std::timed_mutex                      monitor_mutex;
        std::chrono::steady_clock::time_point readout_time;
        ToolFramework::SlowControlElement*    get_state = nullptr;
        ToolFramework::Logging&               log;

        void stop();
        void start();

        void monitor();
        void readout();
    };

    HVoltage();

    bool Initialise(std::string configfile, DataModel& data);
    bool Execute();
    bool Finalise();

    void connect();
    void disconnect();
    void configure();

  private:

    std::unique_ptr<Monitor> monitor;

    std::list<Board> boards;
    std::vector<ToolFramework::SlowControlElement*> controls;

    HandleList<std::function<void ()>>::Handle terminator;

    ToolFramework::Logging& log(int level) {
      return *m_log << ToolFramework::MsgL(level, m_verbose);
    };

    ToolFramework::Logging& error() { return log(0); };
    ToolFramework::Logging& warn()  { return log(1); };
    ToolFramework::Logging& info()  { return log(2); };
};

#endif
