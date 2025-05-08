#ifndef HVoltage_H
#define HVoltage_H

#include <string>
#include <iostream>
#include <chrono>

#include "caen++/v6534.hpp"

#include "Tool.h"
#include "Services.h"

class HVoltage: public ToolFramework::Tool {
  public:
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
            DataModel&                      data,
            const std::vector<caen::V6534>& boards,
            std::chrono::seconds            interval
        );
        ~Monitor();

        void set_interval(std::chrono::seconds interval);

      private:
        ToolFramework::Services&              services;
        ToolFramework::SlowControlCollection& ui;
        const std::vector<caen::V6534>&       boards;
        std::vector<std::array<Channel, 6>>   channels;
        int8_t                                status;
        std::chrono::seconds                  interval;
        std::thread                           thread;
        std::mutex                            readout_mutex;
        std::timed_mutex                      monitor_mutex;
        std::chrono::steady_clock::time_point readout_time;
        ToolFramework::SlowControlElement*    get_state = nullptr;

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

    std::vector<caen::V6534> boards;
    std::vector<ToolFramework::SlowControlElement*> controls;

    ToolFramework::Logging& log(int level) {
      return *m_log << ToolFramework::MsgL(level, m_verbose);
    };

    ToolFramework::Logging& error() { return log(0); };
    ToolFramework::Logging& warn()  { return log(1); };
    ToolFramework::Logging& info()  { return log(2); };
};

#endif
