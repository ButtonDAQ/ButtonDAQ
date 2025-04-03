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
    HVoltage();

    bool Initialise(std::string configfile, DataModel& data);
    bool Execute();
    bool Finalise();

    void connect();
    void disconnect();
    void configure();
 private:
    class Monitor {
      public:
        Monitor(
            ToolFramework::Services&        services,
            const std::vector<caen::V6534>& boards,
            std::chrono::seconds            interval
        );
        ~Monitor();

        void set_interval(std::chrono::seconds interval);

      private:
        ToolFramework::Services&        services;
        const std::vector<caen::V6534>& boards;
        std::chrono::seconds            interval;
        std::thread                     thread;
        std::timed_mutex                mutex;

        void stop();
        void start();

        void monitor();
    };

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
