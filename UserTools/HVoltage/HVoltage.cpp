#include <thread>

#include <caen++/vme.hpp>

#include "HVoltage.h"
#include "DataModel.h"

static bool feql(float x, float y, float eps) {
  return fabs(x - y) <= fabs(eps * x);
};

static SlowControlElement* ui_add(
  SlowControlCollection& ui,
  const std::string& name,
  SlowControlElementType type,
  const std::function<std::string (std::string)>& callback
) {
  if (!ui.Add(name, type, callback))
    throw std::runtime_error(
        "failed to add slow control element `" + name + '\''
    );
  return ui[name];
};

HVoltage::HVoltage(): Tool() {}

void HVoltage::connect() {
  uint32_t link = ~0;

  std::stringstream ss;
  std::string string;
  caen::Connection connection;
  for (int i = 0; ; ++i) {
    ss.str({});
    ss << "hv_" << i << "_vme";
    if (!m_variables.Get(ss.str(), string)) break;
    ss.str({});
    ss << string;
    ss >> std::hex >> connection.address >> std::dec;

    ss.clear();
    ss.str({});
    connection.link = 0;
    ss << "hv_" << i << "_usb";
    if (!m_variables.Get(ss.str(), connection.link)) {
      if (link == ~0) link = m_data->vme_links.hvoltage();
      connection.link = link;
    };

    info()
      << "connecting to high voltage board V6534 "
      << i
      << " at "
      << connection
      << std::flush;
    boards.emplace_back(caen::V6534(connection));
    info() << "success" << std::endl;

    if (m_verbose >= 3) {
      auto& hv = boards.back().hv;
      info()
        << "model: " << hv.model() << ' ' << hv.description() << '\n'
        << "serial number: " << hv.serial_number() << '\n'
        << "firmware version: " << hv.vme_fwrel()
        << std::endl;
    };
  };
};

void HVoltage::disconnect() {
  for (auto& element : controls) m_data->sc_vars.Remove(element->GetName());
  controls.clear();
  boards.clear();
};

void HVoltage::configure() {
  auto& ui = m_data->sc_vars;

  controls.reserve(boards.size() * 6);

  std::stringstream ss;
  auto iboard = boards.begin();
  for (unsigned i = 0; i < boards.size(); ++i, ++iboard) {
    Board* board = &*iboard; // this pointer is captured below
    for (int channel = 0; channel < 6; ++channel) {
      board->hv.set_pwdown(channel, caen::V6534::PowerDownMode::ramp);
      board->hv.set_ramp_up(channel, 5);
      board->hv.set_ramp_down(channel, 10);
      board->hv.set_current(channel, 300e-6);

      ss.str({});
      ss << "hv_" << i << "_ch_" << channel << "_power";
      std::string var = ss.str();
      auto element = ui_add(
          ui, var, OPTIONS,
          [&ui, board, channel](std::string name) -> std::string {
            auto value = ui.GetValue<std::string>(name);
            std::lock_guard<std::mutex> lock(board->mutex);
            board->hv.set_power(channel, value == "on");
            return "ok";
          }
      );
      element->AddOption("on");
      element->AddOption("off");
      element->SetValue(board->hv.power(channel) ? "on" : "off");

      ss.str({});
      ss << "hv_" << i << "_ch_" << channel << "_voltage";
      var = ss.str();
      element = ui_add(
          ui, var, VARIABLE,
          [&ui, board, channel](std::string name) -> std::string {
            auto value = ui.GetValue<float>(name);
            std::lock_guard<std::mutex> lock(board->mutex);
            board->hv.set_voltage(channel, value);
            return "ok";
          }
      );
      element->SetMin(0);
      element->SetMax(2e3);
      element->SetStep(0.1);
      element->SetValue(board->hv.voltage_setting(channel));

      controls.push_back(element);
    };
  };

  controls.push_back(
    ui_add(
        ui, "Shutdown_all", BUTTON,
        [this](std::string name) -> std::string {
          for (auto& board : boards) {
            std::lock_guard<std::mutex> lock(board.mutex);
            for (int channel = 0; channel < 6; ++channel)
              board.hv.set_power(channel, false);
          };

          std::stringstream ss;
          for (size_t i = 0; i < boards.size(); ++i)
            for (int channel = 0; channel < 6; ++channel) {
              ss.str({});
              ss << "hv_" << i << "_ch_" << channel << "_power";
              m_data->sc_vars[ss.str()]->SetValue("off");
            }
          return "ok";
        }
    )
  );
};

bool json_encode(
    std::ostream& output,
    const HVoltage::Monitor::Channel& channel
) {
  return json_encode_object(
      output,
      "voltage",         channel.voltage,
      "current",         channel.current,
      "voltage_setting", channel.voltage_setting,
      "current_setting", channel.current_setting,
      "power",           channel.power,
      "status",          static_cast<int>(channel.status),
      "temperature",     channel.temperature
  );
};

// An ugly hack to work around broken JSON parser in middleman
std::string& json_esc(std::string& string) {
  static const char* from = "{}\"";
  static const char* to   = "@#$";
  for (char& c : string) {
    const char* x = strchr(from, c);
    if (x) c = to[x - from];
  };
  return string;
};

HVoltage::Monitor::Monitor(
    DataModel&              data,
    std::list<Board>&       boards,
    std::chrono::seconds    interval,
    ToolFramework::Logging& log
): services(*data.services),
   ui(data.sc_vars),
   boards(boards),
   interval(interval),
   log(log)
{
  std::chrono::steady_clock::time_point update;

  std::string state;
  get_state = ui_add(
      ui, "hv_get_state", BUTTON,
      [this, update, state](std::string name) mutable -> std::string {
        auto now = std::chrono::steady_clock::now();
        if (now - update > std::chrono::seconds(1)) {
          readout();
          update = now;
          bool result = ToolFramework::json_encode(state, channels);
          json_esc(state);
        };
        return state;
      }
  );

  start();
};

HVoltage::Monitor::~Monitor() {
  stop();
  ui.Remove(get_state->GetName());
};

void HVoltage::Monitor::set_interval(std::chrono::seconds interval) {
  if (interval == this->interval) return;
  stop();
  this->interval = interval;
  start();
};

void HVoltage::Monitor::start() {
  monitor_mutex.lock();
  thread = std::thread(&HVoltage::Monitor::monitor, this);
};

void HVoltage::Monitor::stop() {
  monitor_mutex.unlock();
  thread.join();
};

void HVoltage::Monitor::readout() {
  std::lock_guard<std::mutex> lock(readout_mutex);

  if (channels.size() != boards.size()) channels.resize(boards.size());

  auto board = boards.begin();
  for (size_t b = 0; b < boards.size(); ++b, ++board) {
    auto& channels = this->channels[b];
    std::lock_guard<std::mutex> lock(board->mutex);
    for (uint8_t c = 0; c < 6; ++c) {
      auto& channel = channels[c];
      channel.voltage         = board->hv.voltage(c);
      channel.current         = board->hv.current(c);
      channel.voltage_setting = board->hv.voltage_setting(c);
      channel.current_setting = board->hv.current_setting(c);
      channel.power           = board->hv.power(c);
      channel.temperature     = board->hv.temperature(c);
      channel.status          = board->hv.status(c);
    };
  };

  readout_time = std::chrono::steady_clock::now();
};

void HVoltage::Monitor::monitor() {
  try {
    do {
      readout();

      Store data;
      for (size_t b = 0; b < boards.size(); ++b) {
        auto bprefix = "hv_" + std::to_string(b);
        for (uint8_t c = 0; c < 6; ++c) {
          auto& channel = channels[b][c];
          auto cprefix = bprefix + "_channel_" + std::to_string(c);
          data.Set(cprefix + "_power",       channel.power ? "on" : "off");
          data.Set(cprefix + "_voltage",     channel.voltage);
          data.Set(cprefix + "_current",     channel.current);
          data.Set(cprefix + "_temperature", channel.temperature);
          data.Set(cprefix + "_status",      static_cast<int>(channel.status));
        };
      };

      std::string json;
      data >> json;
      services.SendMonitoringData(json, "HVoltage");

      // interruptable sleep
    } while (!monitor_mutex.try_lock_for(interval));
   } catch (std::exception& e) {
     log
       << ToolFramework::MsgL(0, 0)
       << "HVoltage::Monitor: "
       << e.what()
       << std::endl;
   };
};

bool HVoltage::Initialise(std::string configfile, DataModel& data) {
  if (terminator) terminator.erase();

  InitialiseTool(data);
  InitialiseConfiguration(configfile);

  terminator = m_data->terminators.add([this]() { Finalise(); });

  if (!m_variables.Get("verbose", m_verbose)) m_verbose = 1;

  connect();
  configure();

  if (data.services) {
    int interval = 5;
    m_variables.Get("monitor_interval", interval);

    monitor.reset(
        new Monitor(*m_data, boards, std::chrono::seconds(interval), *m_log)
    );

    auto element = ui_add(
        m_data->sc_vars,
        "hv_interval",
        VARIABLE,
        [this](std::string name) -> std::string {
          auto& ui = m_data->sc_vars;
          monitor->set_interval(
              std::chrono::seconds(ui.GetValue<unsigned>(name))
          );
          return name + ' ' + ui.GetValue<std::string>(name);
        }
    );
    element->SetMin(0);
    element->SetStep(1);
    element->SetValue(interval);
  };

  ExportConfiguration();
  return true;
};

bool HVoltage::Execute() {
  return true;
};

bool HVoltage::Finalise() {
  if (monitor) delete monitor.release();
  disconnect();

  if (terminator) terminator.erase();

  return true;
};
