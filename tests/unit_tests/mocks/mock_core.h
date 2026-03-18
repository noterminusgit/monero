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

// Provides a minimal test helper for scenarios that need core-related types
// without instantiating the full cryptonote::core. Most RPC tests can be done
// via serialization round-trips. For integration-level tests that need a real
// Blockchain + tx_pool, use BlockchainAndPool (from blockchain_and_pool.h) with
// BaseTestDB (from blockchain_db/testdb.h).

#include "cryptonote_core/blockchain_and_pool.h"
#include "blockchain_db/testdb.h"
#include "cryptonote_basic/cryptonote_format_utils.h"

namespace test
{
  // A TestDB that tracks height and allows simple block storage
  class TrackingTestDB : public cryptonote::BaseTestDB
  {
  public:
    TrackingTestDB() : m_height(1), m_cumulative_difficulty(1) {}

    uint64_t height() const override { return m_height; }
    void set_height(uint64_t h) { m_height = h; }

    crypto::hash top_block_hash(uint64_t *block_height = nullptr) const override
    {
      if (block_height) *block_height = m_height > 0 ? m_height - 1 : 0;
      return m_top_hash;
    }
    void set_top_hash(const crypto::hash& h) { m_top_hash = h; }

    cryptonote::difficulty_type get_block_cumulative_difficulty(const uint64_t& height) const override
    {
      return m_cumulative_difficulty;
    }
    void set_cumulative_difficulty(cryptonote::difficulty_type d) { m_cumulative_difficulty = d; }

    uint64_t get_block_timestamp(const uint64_t& height) const override
    {
      auto it = m_timestamps.find(height);
      if (it != m_timestamps.end()) return it->second;
      return 0;
    }
    void set_block_timestamp(uint64_t height, uint64_t ts) { m_timestamps[height] = ts; }

    uint8_t get_hard_fork_version(uint64_t height) const override
    {
      return m_hf_version;
    }
    void set_hard_fork_version(uint64_t height, uint8_t version) override { m_hf_version = version; }
    void set_default_hf_version(uint8_t v) { m_hf_version = v; }

    uint64_t get_block_already_generated_coins(const uint64_t& height) const override
    {
      return m_already_generated_coins;
    }
    void set_already_generated_coins(uint64_t c) { m_already_generated_coins = c; }

    size_t get_block_weight(const uint64_t& height) const override { return m_block_weight; }
    void set_block_weight(size_t w) { m_block_weight = w; }

    uint64_t get_block_long_term_weight(const uint64_t& height) const override { return m_long_term_weight; }
    void set_long_term_weight(uint64_t w) { m_long_term_weight = w; }

    bool has_key_image(const crypto::key_image& img) const override
    {
      return m_spent_keys.count(img) > 0;
    }
    void add_spent_key(const crypto::key_image& k_image) override { m_spent_keys.insert(k_image); }
    void remove_spent_key(const crypto::key_image& k_image) override { m_spent_keys.erase(k_image); }

    uint64_t get_num_outputs(const uint64_t& amount) const override { return m_num_outputs; }
    void set_num_outputs(uint64_t n) { m_num_outputs = n; }

    uint64_t get_database_size() const override { return m_db_size; }
    void set_database_size(uint64_t s) { m_db_size = s; }

    uint64_t get_tx_count() const override { return m_tx_count; }
    void set_tx_count(uint64_t c) { m_tx_count = c; }

  private:
    uint64_t m_height;
    crypto::hash m_top_hash = crypto::null_hash;
    cryptonote::difficulty_type m_cumulative_difficulty;
    std::map<uint64_t, uint64_t> m_timestamps;
    uint8_t m_hf_version = 16;
    uint64_t m_already_generated_coins = 10000000000ULL;
    size_t m_block_weight = 128;
    uint64_t m_long_term_weight = 128;
    std::set<crypto::key_image> m_spent_keys;
    uint64_t m_num_outputs = 1;
    uint64_t m_db_size = 0;
    uint64_t m_tx_count = 0;
  };

  // Helper to initialize a BlockchainAndPool with a test database
  struct TestBlockchainContext
  {
    cryptonote::BlockchainAndPool bap;
    TrackingTestDB* db = nullptr;

    bool init(const std::string& nettype = "mainnet")
    {
      auto tdb = new TrackingTestDB();
      db = tdb;
      // Blockchain::init takes ownership of the DB pointer
      cryptonote::network_type net =
        (nettype == "testnet") ? cryptonote::TESTNET :
        (nettype == "stagenet") ? cryptonote::STAGENET :
        cryptonote::MAINNET;

      bool r = bap.blockchain.init(tdb, net, true);
      if (r) r = bap.tx_pool.init(0);
      return r;
    }

    void deinit()
    {
      bap.tx_pool.deinit();
      bap.blockchain.deinit();
    }
  };

} // namespace test
