#include <unordered_map>

#include "DataModel.h"

#include "Digitizer.h"

Digitizer::Monitor::Monitor(
    ToolFramework::Services& services,
    const std::vector<Board>& boards,
    std::chrono::seconds     interval
): services(services), boards(boards), interval(interval)
{
  start();
};

Digitizer::Monitor::~Monitor() {
  stop();
};

void Digitizer::Monitor::monitor() {
  do {
    Store data;
    int b = 0;
    for (auto& board : boards) {
      auto prefix = "digitizer_" + std::to_string(b++);
      for (unsigned c = 0; c < 16; ++c)
        data.Set(
            prefix + "_channel_" + std::to_string(c) + "_temperature",
            board.digitizer.readTemperature(c)
        );
    };

    std::string json;
    data >> json;
    services.SendMonitoringData(std::move(json), "Digitizer");

    // Locking a mutex seems to be the simplest way to implement an
    // interruptible sleep in C++11
  } while (!mutex.try_lock_for(interval));
  mutex.unlock();
};

void Digitizer::Monitor::start() {
  mutex.lock();
  thread = std::thread(&Digitizer::Monitor::monitor, this);
};

void Digitizer::Monitor::stop() {
  mutex.unlock();
  thread.join();
};

void Digitizer::Monitor::set_interval(std::chrono::seconds interval) {
  if (interval != this->interval) return;
  stop();
  this->interval = interval;
  start();
};

void Digitizer::connect() {
  std::stringstream ss;
  std::string string;
  std::string link_string;

  // link_arg -> indices of digitizers to be read out in the same thread
  std::unordered_map<uint32_t, std::list<int>> threads_partition;

  for (int i = 0; ; ++i) {
    ss.str({});
    ss << "digitizer_" << i << "_link";
    if (!m_variables.Get(ss.str(), link_string)) break;

    CAEN_DGTZ_ConnectionType link;
    if (link_string == "usb")
      link = CAEN_DGTZ_USB;
    else if (link_string == "optical")
      link = CAEN_DGTZ_OpticalLink;
    else if (link_string == "usb_a4818_v2718")
      link = CAEN_DGTZ_USB_A4818_V2718;
    else if (link_string == "usb_a4818_v3718")
      link = CAEN_DGTZ_USB_A4818_V3718;
    else if (link_string == "usb_a4818_v4718")
      link = CAEN_DGTZ_USB_A4818_V4718;
    else if (link_string == "usb_a4818")
      link = CAEN_DGTZ_USB_A4818;
    else if (link_string == "usb_v4718")
      link = CAEN_DGTZ_USB_V4718;
// currently not supported
//    else if (string == "eth_v4718")
//      link = CAEN_DGTZ_ETH_V4718;
    else {
      ss << ": unknown link type: " << link_string;
      throw std::runtime_error(ss.str());
    };

    ss.str({});
    ss << "digitizer_" << i << "_link_arg";
    uint32_t arg;
    if (!m_variables.Get(ss.str(), arg)) {
      ss << " is not found in the configuration file";
      throw std::runtime_error(ss.str());
    };

    ss.str({});
    ss << "digitizer_" << i << "_conet";
    int conet = 0;
    m_variables.Get(ss.str(), conet);

    ss.str({});
    ss << "digitizer_" << i << "_vme";
    uint32_t vme = 0;
    if (m_variables.Get(ss.str(), string)) {
      ss.str({});
      ss << string;
      ss >> std::hex >> vme;
    };

    info()
      << "connecting to digitizer " << i
      << " (link = " << link_string
      << ", arg = " << arg
      << ", conet = " << conet
      << ", vme = " << std::hex << vme << std::dec
      << ")..."
      << std::flush;
    digitizers.emplace_back(
        Board {
          static_cast<uint8_t>(i),
          false,
          caen::Digitizer(link, arg, conet, vme),
          caen::Digitizer::ReadoutBuffer(),
          caen::Digitizer::DPPEvents<CAEN_DGTZ_DPP_PSD_Event_t>(),
          caen::Digitizer::DPPWaveforms<CAEN_DGTZ_DPP_PSD_Waveforms_t>()
        }
    );
    info() << "success" << std::endl;

    {
      auto partition = threads_partition.find(arg);
      if (partition == threads_partition.end())
        threads_partition.emplace(std::pair<uint32_t, std::list<int>>(arg, { i }));
      else
        partition->second.push_back(i);
    };

    if (m_verbose > 2) {
      auto& i = digitizers.back().digitizer.info();
      log(3)
        << "model name: " << i.ModelName << '\n'
        << "model: " << i.Model << '\n'
        << "number of channels: " << i.Channels << '\n'
        << "ROC firmware: " << i.ROC_FirmwareRel << '\n'
        << "AMC firmware: " << i.AMC_FirmwareRel << '\n'
        << "serial number: " << i.SerialNumber << '\n'
        << "license: " << i.License << std::endl;
    };
  };

  readout_threads.reserve(threads_partition.size());
  for (auto& partitions : threads_partition) {
    std::vector<Board*> boards;
    boards.reserve(partitions.second.size());
    for (int i : partitions.second) boards.push_back(&digitizers[i]);
    ReadoutThread thread;
    thread.boards = std::move(boards);
    readout_threads.push_back(std::move(thread));
  };
}

void Digitizer::disconnect() {
  readout_threads.clear();
  digitizers.clear();
};

void Digitizer::configure() {
  CAEN_DGTZ_DPP_PSD_Params_t params;
  params.trgho    = 0;
  params.thr[0]   = 20;
  params.selft[0] = 1;
  params.csens[0] = 0;
  params.sgate[0] = 50;
  params.lgate[0] = 80;

  // should be in agreement with CFD delay (cfdd), see the note after fig. 2.5
  // in UM2580_DPSD_UserManual_rev9
  params.pgate[0] = 4;

  params.tvaw[0]  = 0;
  params.nsbl[0]  = 1;
  params.discr[0] = 1;
  params.cfdf[0]  = 0;
  params.cfdd[0]  = 4;
  params.trgc[0]  = CAEN_DGTZ_DPP_TriggerConfig_Threshold;
  params.purh     = CAEN_DGTZ_DPP_PSD_PUR_DetectOnly;
  params.purgap   = 100;

  m_variables.Get("trigger_hold_off",    params.trgho);
  m_variables.Get("trigger_threshold",   params.thr[0]);
  m_variables.Get("self_trigger",        params.selft[0]);
  m_variables.Get("short_gate",          params.sgate[0]);
  m_variables.Get("long_gate",           params.lgate[0]);
  m_variables.Get("gate_offset",         params.pgate[0]);
  m_variables.Get("trigger_window",      params.tvaw[0]);
  m_variables.Get("baseline_samples",    params.nsbl[0]);
  m_variables.Get("discrimination_mode", params.discr[0]);
  m_variables.Get("CFD_fraction",        params.cfdf[0]);
  m_variables.Get("CFD_delay",           params.cfdd[0]);

  for (int i = 1; i < MAX_DPP_PSD_CHANNEL_SIZE; ++i) {
    params.thr[i]   = params.thr[0];
    params.selft[i] = params.selft[0];
    params.sgate[i] = params.sgate[0];
    params.lgate[i] = params.lgate[0];
    params.pgate[i] = params.pgate[0];
    params.tvaw[i]  = params.tvaw[0];
    params.nsbl[i]  = params.nsbl[0];
    params.discr[i] = params.discr[0];
    params.cfdf[i]  = params.cfdf[0];
    params.cfdd[i]  = params.cfdd[0];
    params.trgc[i]  = params.trgc[0];
  };

  bool waveforms = false;
  m_variables.Get("waveforms_enabled", waveforms);

  nsamples = 0;
  if (waveforms) {
    m_variables.Get("waveforms_nsamples", nsamples);
    if (nsamples == 0) waveforms = false;
  };

  bool baseline = false;
  if (waveforms) m_variables.Get("waveforms_baseline", baseline);

  auto polarity = CAEN_DGTZ_PulsePolarityPositive;
  {
    int p;
    if (m_variables.Get("pulse_polarity", p) && p < 0)
      polarity = CAEN_DGTZ_PulsePolarityNegative;
  };

  int pre_trigger_size = 0;
  m_variables.Get("pre_trigger_size", pre_trigger_size);

  m_data->enabled_digitizer_channels.resize(digitizers.size());

  std::string string;
  int i = 0;
  for (auto& board : digitizers) {
    info() << "configuring digitizer " << i << "... " << std::flush;

    auto& digitizer = board.digitizer;

    std::stringstream ss;
    ss << "digitizer_" << i << "_channels";
    uint16_t channels = 0xFFFF;
    if (m_variables.Get(ss.str(), string)) {
      ss.str({});
      ss << string;
      int mask;
      ss >> std::hex >> mask;
      channels = mask;
    };

    digitizer.reset();

    digitizer.setDPPAcquisitionMode(
        waveforms ? CAEN_DGTZ_DPP_ACQ_MODE_Mixed : CAEN_DGTZ_DPP_ACQ_MODE_List,
        CAEN_DGTZ_DPP_SAVE_PARAM_EnergyAndTime
    );

    if (baseline)
      digitizer.setDPPVirtualProbe(
          ANALOG_TRACE_2, CAEN_DGTZ_DPP_VIRTUALPROBE_Baseline
      );

    digitizer.setChannelEnableMask(channels);
    m_data->enabled_digitizer_channels[i] = channels;

    if (waveforms)
      for (uint32_t channel = 0; channel < 16; channel += 2)
        if (channels & 3 << channel)
          digitizer.setRecordLength(channel, nsamples);

    digitizer.setDPPEventAggregation();

    digitizer.setDPPParameters(channels, params);

    // enable the extras word with extended and fine timestamps
    digitizer.writeRegister(0x8000, 1, 17, 17);

    for (uint32_t channel = 0; channel < 16; ++channel)
      if (channels & 1 << channel) {
        digitizer.setDPPPreTriggerSize(channel, pre_trigger_size);

        // enable constant fraction discriminator (CFD)
//        digitizer.writeRegister(0x1080 | channel << 8, 1, 6, 6);
        // enable fine timestamp
        digitizer.writeRegister(0x1084 | channel << 8, 2, 8, 10);

        digitizer.setChannelPulsePolarity(channel, polarity);
      };

    board.buffer.allocate(digitizer);
    board.events.allocate(digitizer);
    if (waveforms) board.waveforms.allocate(digitizer);

    info() << "success" << std::endl;

    ++i;
  };

  i = 60;
  m_variables.Get("monitor_interval", i);
  auto interval = std::chrono::seconds(i);
  if (monitor)
    monitor->set_interval(interval);
  else if (m_data->services)
    monitor.reset(new Monitor(*m_data->services, digitizers, interval));
}

void Digitizer::start_acquisition() {
  acquiring = true;
  for (auto& rt : readout_threads) {
    for (auto board : rt.boards) {
      info()
        << "starting acquisition on digitizer "
        << static_cast<int>(board->id)
        << std::endl;
      board->digitizer.SWStartAcquisition();
      board->active = true;
    };
    rt.thread = std::thread(
        static_cast<void (Digitizer::*)(const std::vector<Board*>&)>(
          &Digitizer::readout
        ),
        this,
        rt.boards
    );
  };
};

void Digitizer::stop_acquisition() {
  acquiring = false;
  for (auto& rt : readout_threads) {
    rt.thread.join();
    for (auto board : rt.boards) {
      info()
        << "stopping acquisition on digitizer "
        << static_cast<int>(board->id)
        << std::endl;
      board->digitizer.SWStopAcquisition();
      board->active = false;
    };
  };
};

// Read data from the board and put it into m_data.raw_readout
void Digitizer::readout(Board& board) {
  board.digitizer.readData(CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, board.buffer);
  if (board.digitizer.getNumEvents(board.buffer) == 0) return;

  board.digitizer.getEvents(board.buffer, board.events);
  uint32_t nhits = 0;
  for (uint32_t channel = 0;
       channel < board.digitizer.info().Channels;
       ++channel)
    nhits += board.events.nevents(channel);

  std::unique_ptr<std::vector<Hit>> hits(new std::vector<Hit>(nhits));
  auto hit = hits->begin();
  for (uint32_t channel = 0;
       channel < board.digitizer.info().Channels;
       ++channel)
  {
    uint8_t id = channel | board.id << 4;
    for (auto event = board.events.begin(channel);
         event != board.events.end(channel);
         ++event)
    {
      hit->time         = Time(event->TimeTag, event->Extras);
      hit->charge_short = event->ChargeShort;
      hit->charge_long  = event->ChargeLong;
#if 0
      hit->baseline     = event->Baseline;
#else
      // Fine timestamps are incompatible with baselines.
      // See UM4380_725-730_DPP_PSD_Registers_rev7.pdf, DPP Algorithm Control
      // 2, description of the Extras word options (bits [10:8]) at page 30.
      hit->baseline     = 0;
#endif
      hit->channel      = id;
      if (nsamples) {
        board.events.decode(event, board.waveforms);
        uint16_t* waveform = board.waveforms.waveforms()->Trace1;
        hit->waveform.insert(
            hit->waveform.end(), waveform, waveform + nsamples
        );
      };
      ++hit;
    };
  };

  std::lock_guard<std::mutex> lock(m_data->raw_readout_mutex);
  if (!m_data->raw_readout)
    m_data->raw_readout.reset(new std::list<std::unique_ptr<std::vector<Hit>>>());
  m_data->raw_readout->push_back(std::move(hits));
}

void Digitizer::readout(const std::vector<Board*>& boards) {
  int active = 0;
  for (auto board : boards) if (board->active) ++active;

  while (acquiring && active > 0)
    for (auto board : boards)
      if (board->active)
        try {
          readout(*board);
        } catch (caen::Digitizer::Error& error) {
          this->error()
            << "digitizer "
            << static_cast<int>(board->id)
            << ": "
            << error.what()
            << std::endl;
          board->active = false;
          --active;
        };
};

bool Digitizer::Initialise(std::string configfile, DataModel &data) {
  InitialiseTool(data);
  InitialiseConfiguration(configfile);

  if (!m_variables.Get("verbose", m_verbose)) m_verbose = 1;

  connect();
  configure();

  ExportConfiguration();
  return true;
};

bool Digitizer::Execute() {
  if (digitizers.empty()) return true;

  if (m_data->run_stop && acquiring) stop_acquisition();

  if (m_data->change_config) {
    bool acq = acquiring;
    if (acq) stop_acquisition();

    InitialiseConfiguration();

    disconnect();

    connect();
    configure();

    if (acq) start_acquisition();
  };

  if (m_data->run_start && !acquiring) start_acquisition();

  return true;
};

bool Digitizer::Finalise() {
  if (monitor) {
    delete monitor.release();
    monitor = nullptr;
  };

  if (acquiring) stop_acquisition();
  disconnect();

  return true;
}
