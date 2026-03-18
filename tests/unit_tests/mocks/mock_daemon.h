// Copyright (c) 2014-2024, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <string>
#include <functional>
#include "net/http_client.h"

namespace test
{
  // A minimal mock HTTP client for testing RPC handlers without network I/O.
  // Implements the abstract_http_client interface that NodeRPCProxy and other
  // RPC consumers depend on.
  class mock_http_client : public epee::net_utils::http::abstract_http_client
  {
  public:
    using response_handler_t = std::function<bool(const epee::net_utils::http::http_response_info**)>;
    using json_rpc_handler_t = std::function<std::string(const std::string& method, const std::string& params)>;

    mock_http_client() : m_connected(false) {}

    void set_server(std::string host, std::string port, boost::optional<epee::net_utils::http::login> user, epee::net_utils::ssl_options_t ssl_options = epee::net_utils::ssl_support_t::e_ssl_support_autodetect) override
    {
      m_address = host + ":" + port;
    }

    void set_auto_connect(bool auto_connect) override {}

    bool connect(std::chrono::milliseconds timeout) override
    {
      m_connected = m_should_connect;
      return m_connected;
    }

    bool disconnect() override
    {
      m_connected = false;
      return true;
    }

    bool is_connected(bool *ssl = nullptr) override
    {
      if (ssl) *ssl = false;
      return m_connected;
    }

    bool invoke(const boost::string_ref uri, const boost::string_ref method,
                const boost::string_ref body,
                std::chrono::milliseconds timeout,
                const epee::net_utils::http::http_response_info** ppresponse_info = nullptr,
                const epee::net_utils::http::fields_list& additional_params = epee::net_utils::http::fields_list()) override
    {
      if (!m_connected && !connect(timeout))
        return false;

      m_last_uri = uri.to_string();
      m_last_method = method.to_string();
      m_last_body = body.to_string();

      if (m_invoke_handler)
        return m_invoke_handler(ppresponse_info);

      if (ppresponse_info && m_response_info)
        *ppresponse_info = m_response_info;

      return m_invoke_result;
    }

    bool invoke_get(const boost::string_ref uri,
                    std::chrono::milliseconds timeout,
                    const std::string& body = std::string(),
                    const epee::net_utils::http::http_response_info** ppresponse_info = nullptr,
                    const epee::net_utils::http::fields_list& additional_params = epee::net_utils::http::fields_list()) override
    {
      return invoke(uri, "GET", body, timeout, ppresponse_info, additional_params);
    }

    bool invoke_post(const boost::string_ref uri,
                     const std::string& body,
                     std::chrono::milliseconds timeout,
                     const epee::net_utils::http::http_response_info** ppresponse_info = nullptr,
                     const epee::net_utils::http::fields_list& additional_params = epee::net_utils::http::fields_list())
    {
      return invoke(uri, "POST", body, timeout, ppresponse_info, additional_params);
    }

    uint64_t get_bytes_sent() const override { return 0; }
    uint64_t get_bytes_received() const override { return 0; }

    // Test configuration methods
    void set_should_connect(bool v) { m_should_connect = v; }
    void set_invoke_result(bool v) { m_invoke_result = v; }
    void set_response_info(epee::net_utils::http::http_response_info* info) { m_response_info = info; }
    void set_invoke_handler(response_handler_t handler) { m_invoke_handler = handler; }

    const std::string& get_last_uri() const { return m_last_uri; }
    const std::string& get_last_method() const { return m_last_method; }
    const std::string& get_last_body() const { return m_last_body; }

  private:
    bool m_connected;
    bool m_should_connect = true;
    bool m_invoke_result = true;
    std::string m_address;
    std::string m_last_uri;
    std::string m_last_method;
    std::string m_last_body;
    epee::net_utils::http::http_response_info* m_response_info = nullptr;
    response_handler_t m_invoke_handler;
  };

} // namespace test
