#ifndef Digitizer_H
#define Digitizer_H

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <caen++/digitizer.hpp>

#include "Tool.h"

class Digitizer: public ToolFramework::Tool {
  public:
    bool Initialise(std::string configfile, DataModel&);
    bool Execute();
    bool Finalise();

  private:
    struct Board {
      uint8_t                                                      id;
      caen::Digitizer                                              digitizer;
      caen::Digitizer::ReadoutBuffer                               buffer;
      caen::Digitizer::DPPEvents<CAEN_DGTZ_DPP_PSD_Event_t>        events;
      caen::Digitizer::DPPWaveforms<CAEN_DGTZ_DPP_PSD_Waveforms_t> waveforms;
    };

    struct ReadoutThread : ToolFramework::Thread_args {
      Digitizer& tool;
      std::vector<Board*> digitizers;

      ReadoutThread(Digitizer& tool): tool(tool) {};
    };

    struct MonitorThread : ToolFramework::Thread_args {
      Digitizer& tool;
      std::chrono::seconds interval;

      MonitorThread(Digitizer& tool): tool(tool) {};
    };

    ToolFramework::Utilities util;
    std::vector<ReadoutThread> threads;

    std::vector<Board> digitizers;
    uint16_t nsamples; // number of samples in waveforms

    bool acquiring = false;

    MonitorThread* monitor = nullptr;

    void connect();
    void configure();
    void run_readout();
    void run_monitor();
    void readout(Board&);

    static void readout_thread(ToolFramework::Thread_args*);
    static void monitor_thread(ToolFramework::Thread_args*);

    ToolFramework::Logging& log(int level) {
      return *m_log << ToolFramework::MsgL(level, m_verbose);

    };

    ToolFramework::Logging& error() { return log(0); };
    ToolFramework::Logging& warn()  { return log(1); };
    ToolFramework::Logging& info()  { return log(2); };
};

#endif
