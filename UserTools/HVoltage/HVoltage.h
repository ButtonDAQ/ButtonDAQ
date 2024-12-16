#ifndef HVoltage_H
#define HVoltage_H

#include <string>
#include <iostream>
#include <chrono>

#include "caen++/v6534.hpp"

#include "Tool.h"

class HVoltage: public ToolFramework::Tool {
  public:
    HVoltage();

    bool Initialise(std::string configfile, DataModel& data);
    bool Execute();
    bool Finalise();

    void connect();
    void configure();
 private:
    struct Monitor : public ToolFramework::Thread_args {
      HVoltage& tool;
      std::chrono::seconds interval;

      Monitor(HVoltage& tool): tool(tool) {};
    };

    Monitor* monitor = nullptr;

    std::vector<caen::V6534> boards;

    ToolFramework::Utilities util;

    static void monitor_thread(ToolFramework::Thread_args*);

    ToolFramework::Logging& log(int level) {
      return *m_log << ToolFramework::MsgL(level, m_verbose);
    };

    ToolFramework::Logging& error() { return log(0); };
    ToolFramework::Logging& warn()  { return log(1); };
    ToolFramework::Logging& info()  { return log(2); };
};

#endif
