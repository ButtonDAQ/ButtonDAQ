#ifndef PSQL_INTERFACE_H
#define PSQL_INTERFACE_H

#include <iostream>

#include <zmq.hpp>

#include "PGClient.h"

class PSQLInterface {
  private:
    zmq::context_t* m_context;
    PGClient m_pgclient;
    std::string m_dbname;
    std::string m_name;
    int m_timeout;

  public:
    bool Initialise(
        zmq::context_t*,
        std::string device_name,
        std::string config_file,
        int timeout = 300 /* ms */
    );

    bool Finalise();

    // For the methods of this class, negative timeout means the default
    // timeout, zero timeout means no timeout

    bool SQLQuery(
        std::string dbname,
        std::string query,
        std::string* error = nullptr,
        int timeout = -1
    ) {
      return Query(
          std::move(dbname), std::move(query), nullptr, error, timeout
      );
    };

    bool SQLQuery(
        std::string query,
        std::string* error = nullptr,
        int timeout = -1
    ) {
      return SQLQuery(m_dbname, std::move(query), error, timeout);
    };

    bool SQLQuery(
        std::string dbname,
        std::string query,
        std::string& result,
        std::string* error = nullptr,
        int timeout = -1
    ) {
      return Query(
          std::move(dbname), std::move(query), &result, error, timeout
      );
    };

    bool SQLQuery(
        std::string query,
        std::string& result,
        std::string* error = nullptr,
        int timeout = -1
    ) {
      return SQLQuery(m_dbname, std::move(query), result, error, timeout);
    };

    bool SQLQuery(
        std::string dbname,
        std::string query,
        Store& result,
        std::string* error = nullptr,
        int timeout = -1
    );

    bool SQLQuery(
        std::string query,
        Store& result,
        std::string* error = nullptr,
        int timeout = -1
    ) {
      return SQLQuery(m_dbname, std::move(query), result, error, timeout);
    };

    bool SendLog(
        std::string message,
        int severity = 2,
        const std::string& device = "",
        int timeout = 0
    );

    bool SendAlarm(
        std::string type,
        std::string message,
        const std::string& device = "",
        int timeout = 0
    );

    bool SendMonitoringData(
        std::string json,
        const std::string& device = "",
        int timeout = 0
    );

    bool SendMonitoringData(
        Store& data,
        const std::string& device = "",
        int timeout = 0
    );

    bool SendConfig(
        std::string json,
        std::string author,
        int version,
        const std::string& device = "",
        int timeout = -1
    );

    bool SendConfig(
        Store& config,
        const std::string& author,
        int version,
        const std::string& device = "",
        int timeout = -1
    );

    bool GetConfig(
        std::string& json,
        int version,
        const std::string& device = "",
        int timeout = -1
    );

    bool GetConfig(
        Store& config,
        int version,
        const std::string& device = "",
        int timeout = -1
    );

    // quote the string so that it can be used as SQL literal
    static std::string   Quote(const std::string&);
    static std::string&& Quote(std::string&&);

  private:
    bool Query(
        std::string dbname,
        std::string query,
        std::string* result,
        std::string* error,
        int timeout
    );

    bool Query(
        const char* subsystem,
        std::string query,
        std::string* result,
        int timeout
    );
};

#endif
