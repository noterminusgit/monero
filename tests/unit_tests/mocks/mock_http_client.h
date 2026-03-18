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
#include <vector>
#include <functional>
#include <mutex>
#include "net/abstract_http_client.h"
#include "storages/portable_storage_template_helper.h"

namespace test
{
  // Enhanced mock HTTP client with per-URI canned responses, JSON-RPC dispatch,
  // invocation tracking, and programmable error injection. Designed for comprehensive
  // wallet2 and RPC proxy testing.
  class programmable_http_client : public epee::net_utils::http::abstract_http_client
  {
  public:
    struct invocation_record {
      std::string uri;
      std::string method;
      std::string body;
    };

    programmable_http_client() : m_connected(false), m_should_connect(true),
      m_default_result(true), m_bytes_sent(0), m_bytes_received(0),
      m_connect_count(0), m_invoke_count(0) {}

    void set_server(std::string host, std::string port,
                    boost::optional<epee::net_utils::http::login> user,
                    epee::net_utils::ssl_options_t ssl_options = epee::net_utils::ssl_support_t::e_ssl_support_autodetect) override
    {
      m_host = std::move(host);
      m_port = std::move(port);
    }

    bool set_proxy(const std::string& address) override { return true; }

    void set_auto_connect(bool auto_connect) override {}

    bool connect(std::chrono::milliseconds timeout) override
    {
      ++m_connect_count;
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

      ++m_invoke_count;
      m_bytes_sent += body.size();

      std::string uri_str = uri.to_string();
      std::string body_str = body.to_string();

      // Record invocation
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_invocations.push_back({uri_str, method.to_string(), body_str});
      }

      // Check for URI-specific handler
      auto handler_it = m_uri_handlers.find(uri_str);
      if (handler_it != m_uri_handlers.end())
      {
        m_current_response.m_response_code = 200;
        m_current_response.m_response_comment = "OK";
        m_current_response.m_body = handler_it->second(body_str);
        m_bytes_received += m_current_response.m_body.size();
        if (ppresponse_info) *ppresponse_info = &m_current_response;
        return true;
      }

      // Check for canned URI response
      auto resp_it = m_canned_responses.find(uri_str);
      if (resp_it != m_canned_responses.end())
      {
        m_current_response.m_response_code = 200;
        m_current_response.m_response_comment = "OK";
        m_current_response.m_body = resp_it->second;
        m_bytes_received += m_current_response.m_body.size();
        if (ppresponse_info) *ppresponse_info = &m_current_response;
        return true;
      }

      // Check for JSON-RPC dispatch
      if (uri_str == "/json_rpc" && !body_str.empty())
      {
        std::string rpc_method = extract_json_rpc_method(body_str);
        auto jrpc_it = m_json_rpc_handlers.find(rpc_method);
        if (jrpc_it != m_json_rpc_handlers.end())
        {
          m_current_response.m_response_code = 200;
          m_current_response.m_response_comment = "OK";
          m_current_response.m_body = jrpc_it->second(body_str);
          m_bytes_received += m_current_response.m_body.size();
          if (ppresponse_info) *ppresponse_info = &m_current_response;
          return true;
        }
        auto jrpc_canned_it = m_json_rpc_responses.find(rpc_method);
        if (jrpc_canned_it != m_json_rpc_responses.end())
        {
          m_current_response.m_response_code = 200;
          m_current_response.m_response_comment = "OK";
          m_current_response.m_body = jrpc_canned_it->second;
          m_bytes_received += m_current_response.m_body.size();
          if (ppresponse_info) *ppresponse_info = &m_current_response;
          return true;
        }
      }

      // Default: return canned default or error
      if (ppresponse_info)
      {
        m_current_response.m_response_code = m_default_response_code;
        m_current_response.m_response_comment = "OK";
        m_current_response.m_body = m_default_response_body;
        m_bytes_received += m_current_response.m_body.size();
        *ppresponse_info = &m_current_response;
      }
      return m_default_result;
    }

    bool invoke_get(const boost::string_ref uri,
                    std::chrono::milliseconds timeout,
                    const std::string& body = std::string(),
                    const epee::net_utils::http::http_response_info** ppresponse_info = nullptr,
                    const epee::net_utils::http::fields_list& additional_params = epee::net_utils::http::fields_list()) override
    {
      return invoke(uri, "GET", body, timeout, ppresponse_info, additional_params);
    }

    uint64_t get_bytes_sent() const override { return m_bytes_sent; }
    uint64_t get_bytes_received() const override { return m_bytes_received; }

    // --- Configuration API ---

    void set_should_connect(bool v) { m_should_connect = v; }
    void set_default_result(bool v) { m_default_result = v; }
    void set_default_response_code(int code) { m_default_response_code = code; }

    // Set a canned response body for a specific URI
    void set_response(const std::string& uri, const std::string& body)
    {
      m_canned_responses[uri] = body;
    }

    // Set a handler function for a specific URI
    void set_handler(const std::string& uri, std::function<std::string(const std::string&)> handler)
    {
      m_uri_handlers[uri] = std::move(handler);
    }

    // Set a canned JSON-RPC response for a method name
    void set_json_rpc_response(const std::string& method, const std::string& result_json)
    {
      m_json_rpc_responses[method] = wrap_json_rpc_response(result_json);
    }

    // Set a handler for a JSON-RPC method
    void set_json_rpc_handler(const std::string& method, std::function<std::string(const std::string&)> handler)
    {
      m_json_rpc_handlers[method] = std::move(handler);
    }

    // Set default response body for unmatched URIs
    void set_default_response(const std::string& body)
    {
      m_default_response_body = body;
    }

    // --- Template helpers for epee-serializable responses ---

    template<typename T>
    void set_serialized_response(const std::string& uri, const T& response)
    {
      std::string body;
      epee::serialization::store_t_to_json(response, body);
      set_response(uri, body);
    }

    template<typename T>
    void set_serialized_json_rpc_response(const std::string& method, const T& result)
    {
      std::string result_json;
      epee::serialization::store_t_to_json(result, result_json);
      set_json_rpc_response(method, result_json);
    }

    // --- Inspection API ---

    size_t get_connect_count() const { return m_connect_count; }
    size_t get_invoke_count() const { return m_invoke_count; }

    std::vector<invocation_record> get_invocations() const
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      return m_invocations;
    }

    invocation_record get_last_invocation() const
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (m_invocations.empty()) return {"", "", ""};
      return m_invocations.back();
    }

    void clear_invocations()
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_invocations.clear();
      m_invoke_count = 0;
    }

    void reset()
    {
      clear_invocations();
      m_canned_responses.clear();
      m_uri_handlers.clear();
      m_json_rpc_responses.clear();
      m_json_rpc_handlers.clear();
      m_default_response_body.clear();
      m_default_response_code = 200;
      m_default_result = true;
      m_should_connect = true;
      m_connected = false;
      m_bytes_sent = 0;
      m_bytes_received = 0;
      m_connect_count = 0;
    }

  private:
    static std::string extract_json_rpc_method(const std::string& body)
    {
      // Simple extraction of "method":"..." from JSON body
      auto pos = body.find("\"method\"");
      if (pos == std::string::npos) return "";
      pos = body.find("\"", pos + 8);
      if (pos == std::string::npos) return "";
      auto end = body.find("\"", pos + 1);
      if (end == std::string::npos) return "";
      return body.substr(pos + 1, end - pos - 1);
    }

    static std::string wrap_json_rpc_response(const std::string& result)
    {
      return "{\"jsonrpc\":\"2.0\",\"id\":\"0\",\"result\":" + result + "}";
    }

    bool m_connected;
    bool m_should_connect;
    bool m_default_result;
    int m_default_response_code = 200;
    std::string m_host;
    std::string m_port;
    uint64_t m_bytes_sent;
    uint64_t m_bytes_received;
    size_t m_connect_count;
    size_t m_invoke_count;

    std::string m_default_response_body;
    std::map<std::string, std::string> m_canned_responses;
    std::map<std::string, std::function<std::string(const std::string&)>> m_uri_handlers;
    std::map<std::string, std::string> m_json_rpc_responses;
    std::map<std::string, std::function<std::string(const std::string&)>> m_json_rpc_handlers;

    epee::net_utils::http::http_response_info m_current_response;
    mutable std::mutex m_mutex;
    std::vector<invocation_record> m_invocations;
  };

  // Factory that produces programmable_http_client instances, for wallet2 constructor injection
  class mock_http_client_factory : public epee::net_utils::http::http_client_factory
  {
  public:
    mock_http_client_factory() : m_client(std::make_shared<programmable_http_client>()) {}

    std::unique_ptr<epee::net_utils::http::abstract_http_client> create() override
    {
      // We return a wrapper that delegates to our shared client so we retain access
      return std::make_unique<delegating_client>(m_client);
    }

    std::shared_ptr<programmable_http_client> get_client() const { return m_client; }

  private:
    // Thin delegating wrapper so the factory can hand out unique_ptrs while
    // test code retains shared access to the underlying programmable_http_client
    class delegating_client : public epee::net_utils::http::abstract_http_client
    {
    public:
      explicit delegating_client(std::shared_ptr<programmable_http_client> impl) : m_impl(std::move(impl)) {}

      bool set_proxy(const std::string& address) override
      { return m_impl->set_proxy(address); }

      void set_server(std::string host, std::string port,
                      boost::optional<epee::net_utils::http::login> user,
                      epee::net_utils::ssl_options_t ssl_options) override
      { m_impl->set_server(std::move(host), std::move(port), std::move(user), std::move(ssl_options)); }

      void set_auto_connect(bool auto_connect) override
      { m_impl->set_auto_connect(auto_connect); }

      bool connect(std::chrono::milliseconds timeout) override
      { return m_impl->connect(timeout); }

      bool disconnect() override
      { return m_impl->disconnect(); }

      bool is_connected(bool *ssl) override
      { return m_impl->is_connected(ssl); }

      bool invoke(const boost::string_ref uri, const boost::string_ref method,
                  const boost::string_ref body, std::chrono::milliseconds timeout,
                  const epee::net_utils::http::http_response_info** ppresponse_info,
                  const epee::net_utils::http::fields_list& additional_params) override
      { return m_impl->invoke(uri, method, body, timeout, ppresponse_info, additional_params); }

      bool invoke_get(const boost::string_ref uri, std::chrono::milliseconds timeout,
                      const std::string& body,
                      const epee::net_utils::http::http_response_info** ppresponse_info,
                      const epee::net_utils::http::fields_list& additional_params) override
      { return m_impl->invoke_get(uri, timeout, body, ppresponse_info, additional_params); }

      uint64_t get_bytes_sent() const override { return m_impl->get_bytes_sent(); }
      uint64_t get_bytes_received() const override { return m_impl->get_bytes_received(); }

    private:
      std::shared_ptr<programmable_http_client> m_impl;
    };

    std::shared_ptr<programmable_http_client> m_client;
  };

} // namespace test
