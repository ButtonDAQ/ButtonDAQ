#include <thread>

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
  std::stringstream ss;
  std::string string;
  caen::Connection connection;
  for (int i = 0; ; ++i) {
    ss.str({});
    ss << "hv_" << i << "_vme";
    if (!m_variables.Get(ss.str(), string)) break;
    ss.str({});
    ss << string;
    ss >> std::hex >> connection.address;

    ss.clear();
    ss.str({});
    connection.link = 0;
    ss << "hv_" << i << "_usb";
    m_variables.Get(ss.str(), connection.link);

    info()
      << "connecting to high voltage board V6534 "
      << i
      << " at "
      << connection
      << std::flush;
    boards.emplace_back(caen::V6534(connection));
    info() << "success" << std::endl;

    if (m_verbose >= 3) {
      auto& hv = boards.back();
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
  for (unsigned i = 0; i < boards.size(); ++i) {
    caen::V6534* board = &boards[i];
    for (int channel = 0; channel < 6; ++channel) {
      board->set_pwdown(channel, caen::V6534::PowerDownMode::ramp);
      board->set_ramp_up(0, 5);
      board->set_ramp_down(0, 10);

      ss.str({});
      ss << "hv_" << i << "_ch_" << channel << "_power";
      std::string var = ss.str();
      bool power = false;
      m_variables.Get(var, power);
      if (board->power(channel) != power)
        board->set_power(channel, power);

      auto element = ui_add(
          ui, var, OPTIONS,
          [&ui, board, channel](std::string name) -> std::string {
            auto value = ui.GetValue<std::string>(name);
            board->set_power(channel, value == "on");
            return "ok";
          }
      );
      element->AddOption("on");
      element->AddOption("off");
      element->SetValue(power ? "on" : "off");

      ss.str({});
      ss << "hv_" << i << "_ch_" << channel << "_voltage";
      var = ss.str();
      float voltage = 0;
      m_variables.Get(var, voltage);
      if (board->voltage_setting(channel) != voltage)
        board->set_voltage(channel, voltage);

      element = ui_add(
          ui, var, VARIABLE,
          [&ui, board, channel](std::string name) -> std::string {
            auto value = ui.GetValue<float>(name);
            board->set_voltage(channel, value);
            return "ok";
          }
      );
      element->SetMin(0);
      element->SetMax(2e3);
      element->SetStep(0.1);
      element->SetValue(voltage);

      controls.push_back(element);
    };
  };

  controls.push_back(
    ui_add(
        ui, "Shutdown_all", BUTTON,
        [this](std::string name) -> std::string {
          for (auto& board : boards)
            for (int channel = 0; channel < 6; ++channel)
              board.set_power(channel, false);

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

HVoltage::Monitor::Monitor(
    ToolFramework::Services& services,
    const std::vector<caen::V6534>& boards,
    std::chrono::seconds interval
): services(services), boards(boards), interval(interval) {
  start();
};

HVoltage::Monitor::~Monitor() {
  stop();
};

void HVoltage::Monitor::set_interval(std::chrono::seconds interval) {
  if (interval == this->interval) return;
  stop();
  this->interval = interval;
  start();
};

void HVoltage::Monitor::start() {
  mutex.lock();
  thread = std::thread(&HVoltage::Monitor::monitor, this);
};

void HVoltage::Monitor::stop() {
  mutex.unlock();
  thread.join();
};

void HVoltage::Monitor::monitor() {
  struct State {
    bool    power;
    float   voltage;
    float   current;
    int16_t temperature;
  };
  std::vector<std::array<State, 6>> state, last;
  std::chrono::steady_clock::time_point last_update;

  do {
    if (state.size() != boards.size()) {
      state.resize(boards.size());
      last.resize(boards.size());
    };

    for (size_t b = 0; b < boards.size(); ++b) {
      auto& board_state = state[b];
      auto& board = boards[b];
      for (uint8_t c = 0; c < 6; ++c) {
        auto& channel_state       = board_state[c];
        channel_state.power       = board.power(c);
        channel_state.voltage     = board.voltage(c);
        channel_state.current     = board.current(c);
        channel_state.temperature = board.temperature(c);
      };
    };

    bool update = false;
    for (size_t b = 0; b < boards.size() && !update; ++b) {
      auto& state_board = state[b];
      auto& last_board  = last[b];
      for (uint8_t c = 0; c < 6 && !update; ++c) {
        auto& state_channel = state_board[c];
        auto& last_channel  = last_board[c];
        update
          =  state_channel.power != last_channel.power
          || !feql(state_channel.voltage,     last_channel.voltage,     1e-3)
          || !feql(state_channel.current,     last_channel.current,     1e-2)
          || !feql(state_channel.temperature, last_channel.temperature, 1e-1);
      };
    };

    auto now = std::chrono::steady_clock::now();
    if (update || last_update - now > std::chrono::seconds(300)) {
      Store data;
      for (size_t b = 0; b < boards.size(); ++b) {
        auto bprefix = "hv_" + std::to_string(b);
        for (uint8_t c = 0; c < 6; ++c) {
          auto& channel = state[b][c];
          auto cprefix = bprefix + "_channel_" + std::to_string(c);
          data.Set(cprefix + "_power",       channel.power ? "on" : "off");
          data.Set(cprefix + "_voltage",     channel.voltage);
          data.Set(cprefix + "_current",     channel.current);
          data.Set(cprefix + "_temperature", channel.temperature);
        };
      };

      std::string json;
      data >> json;
      services.SendMonitoringData(json, "HVoltage");

      last = state;
      last_update = now;
    };

    // interruptable sleep
  } while (!mutex.try_lock_for(interval));
};

bool HVoltage::Initialise(std::string configfile, DataModel& data) {
  InitialiseTool(data);
  InitialiseConfiguration(configfile);

  if (!m_variables.Get("verbose", m_verbose)) m_verbose = 1;

  connect();
  configure();

  if (data.services) {
    int interval = 5;
    m_variables.Get("monitor_interval", interval);

    monitor.reset(
        new Monitor(
          *m_data->services,
          boards,
          std::chrono::seconds(interval)
        )
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
  if (m_data->change_config) {
    disconnect();
    InitialiseConfiguration();
    if (!m_variables.Get("verbose", m_verbose)) m_verbose = 1;
    connect();
    configure();
  };

  return true;
};

bool HVoltage::Finalise() {
  if (monitor) delete monitor.release();
  disconnect();
  return true;
};
