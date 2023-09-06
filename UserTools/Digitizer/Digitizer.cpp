#include <unordered_map>

#include "Digitizer.h"

void Digitizer::connect() {
  std::stringstream ss;
  std::string string;
  std::unordered_map<int, ThreadArgs*> thread_partition;
  for (int i = 0; ; ++i) {
    ss.str({});
    ss << "digitizer_" << i << "_link";
    if (!m_variables.Get(ss.str(), string)) break;

    CAEN_DGTZ_ConnectionType link;
    if (string == "usb")
      link = CAEN_DGTZ_USB;
    else if (string == "optical")
      link = CAEN_DGTZ_OpticalLink;
    else if (string == "usb_a4818_v2718")
      link = CAEN_DGTZ_USB_A4818_V2718;
    else if (string == "usb_a4818_v3718")
      link = CAEN_DGTZ_USB_A4818_V3718;
    else if (string == "usb_a4818_v4718")
      link = CAEN_DGTZ_USB_A4818_V4718;
    else if (string == "usb_a4818")
      link = CAEN_DGTZ_USB_A4818;
    else if (string == "usb_v4718")
      link = CAEN_DGTZ_USB_V4718;
// currently not supported
//    else if (string == "eth_v4718")
//      link = CAEN_DGTZ_ETH_V4718;
    else {
      ss << ": unknown link type: " << string;
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

    info() << "connecting to digitizer " << i << "... ";
    digitizers.emplace_back(
        Board {
          static_cast<uint8_t>(i),
          caen::Digitizer(link, arg, conet, vme)
        }
    );
    info() << "success" << std::endl;

    auto thread = thread_partition.find(arg);
    if (thread == thread_partition.end()) {
      threads.emplace_back(*this);
      thread = thread_partition.emplace(arg, &threads.back()).first;
    };
    thread->second->digitizers.push_back(&digitizers.back());

    if (m_verbose > 2) {
      auto& i = digitizers.back().digitizer.info();
      info()
        << "model name: " << i.ModelName << '\n'
        << "model: " << i.Model << '\n'
        << "number of channels: " << i.Channels << '\n'
        << "ROC firmware: " << i.ROC_FirmwareRel << '\n'
        << "AMC firmware: " << i.AMC_FirmwareRel << '\n'
        << "serial number: " << i.SerialNumber << '\n'
        << "license: " << i.License << std::endl;
    };
  };
}

void Digitizer::configure() {
  CAEN_DGTZ_DPP_PSD_Params_t params;
  params.trgho    = 0;
  params.thr[0]   = 20;
  params.selft[0] = 0;
  params.csens[0] = 0;
  params.sgate[0] = 50;
  params.lgate[0] = 80;
  params.pgate[0] = 0;
  params.tvaw[0]  = 0;
  params.nsbl[0]  = 1;
  params.discr[0] = 0;
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
    params.selft[i]  = params.selft[0];
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

  int pre_trigger_size = 0;
  m_variables.Get("pre_trigger_size", pre_trigger_size);

  std::string string;
  int i = 0;
  for (auto& board : digitizers) {
    info() << "configuring digitizer " << i << "... ";

    auto& digitizer = board.digitizer;

    std::stringstream ss;
    ss << "digitizer_" << i << "_channels";
    uint8_t channels = 0xf;
    if (m_variables.Get(ss.str(), string)) {
      ss.str({});
      ss << string;
      ss >> std::hex >> channels;
    };

    digitizer.reset();

    digitizer.setDPPAcquisitionMode(
        waveforms ? CAEN_DGTZ_DPP_ACQ_MODE_Mixed : CAEN_DGTZ_DPP_ACQ_MODE_List,
        CAEN_DGTZ_DPP_SAVE_PARAM_EnergyAndTime
    );

    digitizer.disableEventAlignedReadout();

    if (baseline)
      digitizer.setDPPVirtualProbe(
          ANALOG_TRACE_2, CAEN_DGTZ_DPP_VIRTUALPROBE_Baseline
      );

    digitizer.setChannelEnableMask(channels);
    if (waveforms)
      for (uint32_t channel = 0; channel < 16; channel += 2)
        if (channels & 3 << channel)
          digitizer.setRecordLength(channel, nsamples);

    digitizer.setDPPEventAggregation();

    digitizer.setDPPParameters(channels, params);
    
    for (uint32_t channel = 0; channel < 16; ++channel)
      if (channels & 1 << channel) {
        digitizer.setDPPPreTriggerSize(channel, pre_trigger_size);

        // enable fine timestamp
        digitizer.writeRegister(0x1084 | channel << 8, 2, 8, 10);
      };

    board.buffer.allocate(digitizer);
    board.events.allocate(digitizer);
    if (waveforms) board.waveforms.allocate(digitizer);

    info() << "success" << std::endl;
  };
}

void Digitizer::run_threads() {
  std::stringstream ss;
  for (size_t i = 0; i < threads.size(); ++i) {
    ss.str({});
    ss << "Digitizer " << i;
    util.CreateThread(ss.str(), &thread, &threads[i]);
  };
}

// Read data from the board and put it into m_data.raw_readout_queue
void Digitizer::readout(Board& board) {
  board.digitizer.readData(CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, board.buffer);
  if (board.digitizer.getNumEvents(board.buffer) == 0) return;

  board.digitizer.getEvents(board.buffer, board.events);
  uint32_t nhits = 0;
  for (uint32_t channel = 0;
       channel < board.digitizer.info().Channels;
       ++channel)
    nhits += board.events.nevents(channel);

  auto hits = std::unique_ptr<std::vector<CAENEvent>>(
      new std::vector<CAENEvent>(nhits)
  );
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
      hit->TimeTag     = event->TimeTag;
      hit->Extras      = event->Extras;
      hit->ChargeShort = event->ChargeShort;
      hit->ChargeLong  = event->ChargeLong;
      hit->Baseline    = event->Baseline;
      hit->channel     = id;
      if (nsamples) {
        board.events.decode(event, board.waveforms);
        auto waveform = board.waveforms.waveforms()->Trace1;
        hit->waveform.insert(
            hit->waveform.end(),
            waveform,
            waveform + nsamples * sizeof(*waveform)
        );
      };
      ++hit;
    };
  };

  std::lock_guard<std::mutex> lock(m_data->raw_readout_mutex);
  m_data->raw_readout_queue.push(std::move(hits));
}

void Digitizer::thread(Thread_args* arg) {
  ThreadArgs* args = static_cast<ThreadArgs*>(arg);
  try {
    for (auto digitizer : args->digitizers) args->tool.readout(*digitizer);
  } catch (std::exception& e) {
    args->tool.error() << e.what() << std::endl;
  };
}

bool Digitizer::Initialise(std::string configfile, DataModel &data) {
  try {
    if (configfile != "") m_variables.Initialise(configfile);
    //m_variables.Print();

    m_data = &data;
    m_log  = m_data->Log;

    if (!m_variables.Get("verbose", m_verbose)) m_verbose = 1;

    connect();
    configure();
    run_threads();
    return true;

  } catch (std::exception& e) {
    if (m_log) error() << e.what() << std::endl;
    return false;
  };
}

bool Digitizer::Execute() {
  static bool acquire = false;
  if (!acquire) {
    acquire = true;
    for (auto& board : digitizers) board.digitizer.SWStartAcquisition();
  };

  sleep(1);
  std::lock_guard<std::mutex>(m_data->raw_readout_mutex);
  while (!m_data->raw_readout_queue.empty()) m_data->raw_readout_queue.pop();
  return true;
}

bool Digitizer::Finalise() {
  for (auto& thread : threads) util.KillThread(&thread);
  threads.clear();
  for (auto& board : digitizers) board.digitizer.SWStopAcquisition();
  digitizers.clear(); // disconnect from the digitizers
  return true;
}
