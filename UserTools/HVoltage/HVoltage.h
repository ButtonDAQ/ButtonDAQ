#ifndef HVoltage_H
#define HVoltage_H

#include <string>
#include <iostream>
#include <chrono>

#include "caen++/v6534.hpp"

#include "Tool.h"

class HVoltage: public Tool {
  public:
    HVoltage();

    bool Initialise(std::string configfile, DataModel& data);
    bool Execute();
    bool Finalise();

    void connect();
    void configure();
 private:
    struct Monitor : public Thread_args {
      HVoltage& tool;
      std::chrono::seconds interval;

      Monitor(HVoltage& tool): tool(tool) {};
    };

    Monitor* monitor = nullptr;

    std::vector<caen::V6534> boards;

    Utilities util;

    static void monitor_thread(Thread_args*);

    Logging& log(int level) { return *m_log << MsgL(level, m_verbose); };

    Logging& error() { return log(0); };
    Logging& warn()  { return log(1); };
    Logging& info()  { return log(2); };
};

#endif
