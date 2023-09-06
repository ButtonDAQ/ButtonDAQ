#ifndef Digitizer_H
#define Digitizer_H

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <caen++/digitizer.hpp>

#include "Tool.h"

class Digitizer: public Tool {
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

    struct ReadoutThread : Thread_args {
      Digitizer& tool;
      std::vector<Board*> digitizers;

      ReadoutThread(Digitizer& tool): tool(tool) {};
    };

    struct MonitorThread : Thread_args {
      Digitizer& tool;
      std::chrono::seconds interval;

      MonitorThread(Digitizer& tool): tool(tool) {};
    };

    Utilities util;
    std::vector<ReadoutThread> threads;
    MonitorThread* monitor = nullptr;

    std::vector<Board> digitizers;
    uint16_t nsamples; // number of samples in waveforms

    bool acquiring = false;

    void connect();
    void configure();
    void run_readout();
    void run_monitor();
    void readout(Board&);

    static void readout_thread(Thread_args*);
    static void monitor_thread(Thread_args*);

    Logging& log(int level) { return *m_log << MsgL(level, m_verbose); };

    Logging& error() { return log(0); };
    Logging& warn()  { return log(1); };
    Logging& info()  { return log(2); };
};

#endif
