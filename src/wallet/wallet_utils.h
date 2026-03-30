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
#include <vector>
#include <unordered_set>
#include <boost/algorithm/string.hpp>
#include "crypto/crypto.h"
#include "misc_log_ex.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "common/command_line.h"

namespace tools
{
namespace wallet_utils
{
  inline void add_reason(std::string &reasons, const char *reason)
  {
    if (!reasons.empty())
      reasons += ", ";
    reasons += reason;
  }

  inline std::string get_text_reason(const cryptonote::COMMAND_RPC_SEND_RAW_TX::response &res)
  {
    std::string reason;
    if (res.low_mixin)
      add_reason(reason, "bad ring size");
    if (res.double_spend)
      add_reason(reason, "double spend");
    if (res.invalid_input)
      add_reason(reason, "invalid input");
    if (res.invalid_output)
      add_reason(reason, "invalid output");
    if (res.too_few_outputs)
      add_reason(reason, "too few outputs");
    if (res.too_big)
      add_reason(reason, "too big");
    if (res.overspend)
      add_reason(reason, "overspend");
    if (res.fee_too_low)
      add_reason(reason, "fee too low");
    if (res.sanity_check_failed)
      add_reason(reason, "tx sanity check failed");
    if (res.not_relayed)
      add_reason(reason, "tx was not relayed");
    return reason;
  }

  inline bool keys_intersect(const std::unordered_set<crypto::public_key>& s1, const std::unordered_set<crypto::public_key>& s2)
  {
    if (s1.empty() || s2.empty())
      return false;

    for (const auto& e: s1)
    {
      if (s2.find(e) != s2.end())
        return true;
    }

    return false;
  }

  inline bool parse_bool(const std::string& s, bool& result)
  {
    if (s == "1" || command_line::is_yes(s))
    {
      result = true;
      return true;
    }
    if (s == "0" || command_line::is_no(s))
    {
      result = false;
      return true;
    }

    boost::algorithm::is_iequal ignore_case{};
    if (boost::algorithm::equals("true", s, ignore_case))
    {
      result = true;
      return true;
    }
    if (boost::algorithm::equals("false", s, ignore_case))
    {
      result = false;
      return true;
    }

    return false;
  }

  inline bool datestr_to_int(const std::string &heightstr, uint16_t &year, uint8_t &month, uint8_t &day)
  {
    if (heightstr.size() != 10 || heightstr[4] != '-' || heightstr[7] != '-')
      return false;
    try
    {
      year  = boost::lexical_cast<uint16_t>(heightstr.substr(0,4));
      month = boost::lexical_cast<uint16_t>(heightstr.substr(5,2));
      day   = boost::lexical_cast<uint16_t>(heightstr.substr(8,2));
    }
    catch (const boost::bad_lexical_cast &)
    {
      return false;
    }
    return true;
  }

  template<typename T>
  T pop_index(std::vector<T>& vec, size_t idx)
  {
    CHECK_AND_ASSERT_MES(!vec.empty(), T(), "Vector must be non-empty");
    CHECK_AND_ASSERT_MES(idx < vec.size(), T(), "idx out of bounds");

    T res = vec[idx];
    if (idx + 1 != vec.size())
    {
      vec[idx] = vec.back();
    }
    vec.resize(vec.size() - 1);

    return res;
  }

  template<typename T>
  T pop_random_value(std::vector<T>& vec)
  {
    CHECK_AND_ASSERT_MES(!vec.empty(), T(), "Vector must be non-empty");

    size_t idx = crypto::rand_idx(vec.size());
    return pop_index(vec, idx);
  }

  template<typename T>
  T pop_back(std::vector<T>& vec)
  {
    CHECK_AND_ASSERT_MES(!vec.empty(), T(), "Vector must be non-empty");

    T res = vec.back();
    vec.pop_back();
    return res;
  }

  template<typename T>
  void pop_if_present(std::vector<T>& vec, T e)
  {
    for (size_t i = 0; i < vec.size(); ++i)
    {
      if (e == vec[i])
      {
        pop_index(vec, i);
        return;
      }
    }
  }

} // namespace wallet_utils
} // namespace tools
