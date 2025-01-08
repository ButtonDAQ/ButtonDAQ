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
    ~Digitizer() { Finalise(); }

    bool Initialise(std::string configfile, DataModel&);
    bool Execute();
    bool Finalise();

  private:
    struct Board {
      uint8_t                                                      id;
      bool                                                         active;
      caen::Digitizer                                              digitizer;
      caen::Digitizer::ReadoutBuffer                               buffer;
      caen::Digitizer::DPPEvents<CAEN_DGTZ_DPP_PSD_Event_t>        events;
      caen::Digitizer::DPPWaveforms<CAEN_DGTZ_DPP_PSD_Waveforms_t> waveforms;
    };

    struct ReadoutThread {
      std::vector<Board*> boards;
      std::thread thread;
    };

    class Monitor {
      public:
        Monitor(
            ToolFramework::Services&  services,
            const std::vector<Board>& boards,
            std::chrono::seconds      interval
        );
        ~Monitor();

        void set_interval(std::chrono::seconds);

      private:
        ToolFramework::Services&  services;
        const std::vector<Board>& boards;
        std::chrono::seconds      interval;
        std::thread               thread;
        std::timed_mutex          mutex;

        void start();
        void stop();

        void monitor();
    };

    std::vector<Board> digitizers;
    uint16_t nsamples; // number of samples in waveforms

    bool acquiring = false;
    std::vector<ReadoutThread> readout_threads;

    std::unique_ptr<Monitor> monitor;

    void connect();
    void disconnect();
    void configure();

    void start_acquisition();
    void stop_acquisition();

    void readout(Board&);
    void readout(const std::vector<Board*>&);

    ToolFramework::Logging& log(int level) {
      return *m_log << ToolFramework::MsgL(level, m_verbose);

    };

    ToolFramework::Logging& error() { return log(0); };
    ToolFramework::Logging& warn()  { return log(1); };
    ToolFramework::Logging& info()  { return log(2); };
};

#endif
