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
#include <map>
#include <functional>
#include "net/http_client.h"

namespace test
{
  // A mock HTTP client that returns pre-configured JSON-RPC responses.
  // Use set_response() to configure responses for specific method names.
  class mock_wallet_rpc_client : public epee::net_utils::http::abstract_http_client
  {
  public:
    mock_wallet_rpc_client() : m_connected(false) {}

    void set_server(const std::string& address, boost::optional<epee::net_utils::http::login> user, epee::net_utils::ssl_options_t ssl_options = epee::net_utils::ssl_support_t::e_ssl_support_autodetect) override
    {
      m_address = address;
    }

    void set_auto_connect(bool auto_connect) override {}

    bool connect(std::chrono::milliseconds timeout) override
    {
      m_connected = true;
      return true;
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
      if (!m_connected)
        connect(timeout);

      m_last_body = body.to_string();

      if (ppresponse_info)
      {
        m_response.m_response_code = 200;
        m_response.m_response_comment = "OK";
        m_response.m_body = m_next_response_body;
        *ppresponse_info = &m_response;
      }
      return true;
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
                     const epee::net_utils::http::fields_list& additional_params = epee::net_utils::http::fields_list()) override
    {
      return invoke(uri, "POST", body, timeout, ppresponse_info, additional_params);
    }

    uint64_t get_bytes_sent() const override { return 0; }
    uint64_t get_bytes_received() const override { return 0; }

    // Configure what the next response body should be
    void set_next_response(const std::string& body) { m_next_response_body = body; }
    const std::string& get_last_body() const { return m_last_body; }

  private:
    bool m_connected;
    std::string m_address;
    std::string m_last_body;
    std::string m_next_response_body;
    epee::net_utils::http::http_response_info m_response;
  };

} // namespace test
