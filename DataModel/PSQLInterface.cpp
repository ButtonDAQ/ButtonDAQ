#include "PSQLInterface.h"

bool PSQLInterface::Initialise(
    zmq::context_t* context,
    std::string device_name,
    std::string config_file,
    int timeout
) {
  m_context = context;
  m_name = std::move(device_name);
  m_dbname = "daq";
  m_timeout = timeout;

  m_pgclient.SetUp(m_context);
  if (!m_pgclient.Initialise(std::move(config_file))) {
    std::cout << "error initialising pgclient" << std::endl;
    return false;
  };

  // after Initialising the pgclient needs ~15 seconds for the middleman to connect
  std::this_thread::sleep_for(std::chrono::seconds(15));
  // hopefully the middleman has found us by now
  
  return true;
};

bool PSQLInterface::Finalise() {
  return m_pgclient.Finalise();
};

bool PSQLInterface::Query(
    std::string dbname,
    std::string query,
    std::string* result,
    std::string* error,
    int timeout
) {
  if (timeout < 0) timeout = m_timeout;
  return m_pgclient.SendQuery(
      std::move(dbname),
      std::move(query),
      result,
      timeout ? &timeout : nullptr,
      error
  );
};

bool PSQLInterface::Query(
    const char* subsystem,
    std::string query,
    std::string* result,
    int timeout
) {
  std::string error;
  if (Query(m_dbname, std::move(query), result, &error, timeout))
    return true;
  std::cerr << subsystem << " error: " << error << std::endl;
  return false;
};

bool PSQLInterface::SQLQuery(
    std::string dbname,
    std::string query,
    Store& result,
    std::string* error,
    int timeout
) {
  std::string json;
  if (!SQLQuery(std::move(dbname), std::move(query), json, error, timeout))
    return false;
  result.JsonParser(json);
  return true;
};

bool PSQLInterface::SendLog(
    std::string message,
    int severity,
    const std::string& device,
    int timeout
) {
  std::stringstream query;
  query
    << "insert into logging (time, source, severity, message) values (now(), '"
    << Quote(device.empty() ? m_name : device)
    << "', "
    << severity
    << ", '"
    << Quote(std::move(message))
    << "')";
  return Query("log", query.str(), nullptr, timeout);
};

bool PSQLInterface::SendAlarm(
    std::string type,
    std::string message,
    const std::string& device,
    int timeout
) {
  std::stringstream query;
  query
    << "insert into alarms (time, source, type, alarm) values (now(), '"
    << Quote(device.empty() ? m_name : device)
    << "', '"
    << Quote(std::move(type))
    << "', '"
    << Quote(std::move(message))
    << "')";
  return Query("SendAlarm", query.str(), nullptr, timeout);
};

bool PSQLInterface::SendMonitoringData(
    std::string json,
    const std::string& device,
    int timeout
) {
  std::stringstream query;
  query
    << "insert into monitoring (time, source, data) values (now(), '"
    << Quote(device.empty() ? m_name : device)
    << "', '"
    << Quote(std::move(json))
    << "')";
  return Query("SendMonitoringData", query.str(), nullptr, timeout);
};

bool PSQLInterface::SendMonitoringData(
    Store& data,
    const std::string& device,
    int timeout
) {
  std::string json;
  data >> json;
  return SendMonitoringData(json, device, timeout);
};

bool PSQLInterface::SendConfig(
    std::string json,
    std::string author,
    int version,
    const std::string& device,
    int timeout
) {
  std::stringstream query;
  query
    << "insert into device_config (time, device, version, author, data) "
       "values (now(), '"
    << Quote(device.empty() ? m_name : device)
    << "', "
    << version
    << ", '"
    << Quote(std::move(author))
    << "', '"
    << Quote(std::move(json))
    << "')";
  return Query("SendConfig", query.str(), nullptr, timeout);
};

bool PSQLInterface::SendConfig(
    Store& config,
    const std::string& author,
    int version,
    const std::string& device,
    int timeout
) {
  std::string json;
  config >> json;
  return SendConfig(json, author, version, device, timeout);
};

bool PSQLInterface::GetConfig(
    std::string& json,
    int version,
    const std::string& device,
    int timeout
) {
  std::stringstream query;
  query
    << "select data from device_config where device = '"
    << Quote(device.empty() ? m_name : device)
    << "' and version = "
    << version;
  std::string data;
  if (!Query("GetConfig", query.str(), &data, timeout))
    return false;

  // The database reply is "{\"data\": \"{\"key\":\"value\"}, ...\"}".
#if 0
  // The following doesn't work because the unescaped quotes in the value for
  // "data" fool the JSON parser. 
  Store config;
  config.JsonParser(data);
  return config.Get("data", json);
#else
  // Strip the returning string.
  // XXX: revise this if the "data" column is renamed
  data.replace(0, 9, "");
  data.replace(data.end() - 2, data.end(), "");
  json = std::move(data);
  return true;
#endif
};

bool PSQLInterface::GetConfig(
    Store& config,
    int version,
    const std::string& device,
    int timeout
) {
  std::string json;
  if (!GetConfig(json, version, device, timeout)) return false;
  config.JsonParser(json);
  return true;
};

std::string PSQLInterface::Quote(const std::string& string) {
  size_t dsize = 0;
  for (auto& c : string) if (c == '\'') ++dsize;
  if (dsize == 0) return string;

  std::string result;
  result.reserve(string.size() + dsize);
  for (auto& c : string) {
    result.push_back(c);
    if (c == '\'') result.push_back(c);
  };
  return result;
};

std::string&& PSQLInterface::Quote(std::string&& string) {
  size_t dsize = 0;
  for (auto& c : string) if (c == '\'') ++dsize;

  if (dsize > 0) {
    string.insert(0, dsize, ' ');
    auto dst = string.begin();
    auto src = dst + dsize;
    do {
      if (*src == '\'') *dst++ = *src;
      *dst++ = *src++;
    } while (dst != src);
  };

  return std::move(string);
};
