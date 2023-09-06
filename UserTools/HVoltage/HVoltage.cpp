#include "HVoltage.h"

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
  for (int i = 0; ; ++i) {
    uint32_t vme;
    ss.str({});
    ss << "hv_" << i << "_vme";
    if (!m_variables.Get(ss.str(), string)) break;
    ss.str({});
    ss << string;
    ss >> std::hex >> vme;

    uint32_t usb = 0;
    ss.str({});
    ss << "hv_" << i << "_usb";
    m_variables.Get(ss.str(), usb);

    info()
      << "connecting to high voltage board V6534 "
      << i
      << " (vme = "
      << std::hex << vme << std::dec
      << ", usb = "
      << usb
      << ")..."
      << std::flush;
    boards.emplace_back(caen::V6534(vme, usb));
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

void HVoltage::configure() {
  auto& ui = m_data->SC_vars;

  std::stringstream ss;
  for (unsigned i = 0; i < boards.size(); ++i)
    for (int channel = 0; channel < 6; ++channel) {
      float voltage = 0;
      ss.str({});
      ss << "hv_" << i << "_ch_" << channel;
      bool power = m_variables.Get(ss.str(), voltage) && voltage > 0;
      boards[i].set_voltage(channel, voltage);
      boards[i].set_power(channel, power);

      auto set_voltage = [&vars = ui, &board = boards[i], channel](std::string name)
        -> std::string
      {
        auto value = vars.GetValue<float>(name);
        board.set_voltage(channel, value);
        return std::move(name) + ' ' + std::to_string(value);
      };

      ss.str({});
      ss << "hv_" << i << "_channel_" << channel << "_voltage";
      auto name = ss.str();
      auto element = ui_add(ui, name, VARIABLE, set_voltage);
      element->SetMin(0);
      element->SetMax(boards[i].voltage_max(channel));
      element->SetStep(0.1);
      element->SetValue(voltage);

      auto set_power = [&vars = ui, &board = boards[i], channel](std::string name)
        -> std::string
      {
        auto value = vars.GetValue<std::string>(name);
        board.set_power(channel, value == "on");
        return std::move(name) + ' ' + value;
      };

      ss.str({});
      ss << "hv_" << i << "_channel_" << channel << "_power";
      name = ss.str();
      element = ui_add(ui, name, OPTIONS, set_power);
      element->AddOption("on");
      element->AddOption("off");
      element->SetValue(power ? "on" : "off");
    };
};

void HVoltage::monitor_thread(Thread_args* args) {
  auto mon = static_cast<Monitor*>(args);

  Store data;
  int i = 0;
  for (auto& board : mon->tool.boards) {
    auto bprefix = "hv_" + std::to_string(i++);
    for (uint8_t c = 0; c < board.nchannels(); ++c) {
      auto cprefix = bprefix + "_channel_" + std::to_string(c);
      data.Set(cprefix + "_power",       board.power(c) ? "on" : "off");
      data.Set(cprefix + "_voltage",     board.voltage(c));
      data.Set(cprefix + "_current",     board.current(c));
      data.Set(cprefix + "_temperature", board.temperature(c));
    };
  };

  std::string json;
  data >> json;
  mon->tool.m_data->SQL.SendMonitoringData(json, "HVoltage");

  std::this_thread::sleep_for(mon->interval);
};

bool HVoltage::Initialise(std::string configfile, DataModel& data) {
  std::string json;
  if (data.SQL.GetConfig(json, 0, "HVoltage"))
    m_variables.JsonParser(std::move(json));

  if (configfile != "") m_variables.Initialise(configfile);
  //m_variables.Print();

  m_data = &data;
  m_log  = m_data->Log;

  if (!m_variables.Get("verbose", m_verbose)) m_verbose = 1;

  m_data->SC_vars.InitThreadedReceiver(m_data->context, 5555);

  try {
    connect();
    configure();

    Monitor* monitor = new Monitor(*this);
    int interval = 5;
    m_variables.Get("monitor_interval", interval);
    monitor->interval = std::chrono::seconds(interval);
    util.CreateThread("HVoltage monitor", &monitor_thread, monitor);

    auto element = ui_add(
        m_data->SC_vars,
        "hv_interval",
        VARIABLE,
        [&ui = m_data->SC_vars, monitor](std::string name) -> std::string {
          monitor->interval = std::chrono::seconds(ui.GetValue<unsigned>(name));
          return name + ' ' + ui.GetValue<std::string>(name);
        }
    );
    element->SetMin(0);
    element->SetStep(1);
    element->SetValue(interval);
  } catch (std::exception& e) {
    error() << e.what() << std::endl;
    return false;
  };

  return true;
};

bool HVoltage::Execute() {
  return true;
};

bool HVoltage::Finalise() {
  for (auto& board : boards)
    for (int channel = 0; channel < 6; ++channel)
      board.set_power(channel, false);

  if (monitor) {
    util.KillThread(monitor);
    delete monitor;
  };

  return true;
};
