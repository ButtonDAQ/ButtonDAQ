#ifndef Digitizer_H
#define Digitizer_H

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

    struct ThreadArgs : Thread_args {
      Digitizer& tool;
      std::vector<Board*> digitizers;

      ThreadArgs(Digitizer& tool): tool(tool) {};
    };

    Utilities util;
    std::vector<ThreadArgs> threads;

    std::vector<Board> digitizers;
    uint16_t nsamples; // number of samples in waveforms

    void connect();
    void configure();
    void run_threads();
    void readout(Board&);

    static void thread(Thread_args* arg);

    Logging& log(int level) { return *m_log << MsgL(level, m_verbose); };

    Logging& error() { return log(0); };
    Logging& warn()  { return log(1); };
    Logging& info()  { return log(2); };
};

#endif
