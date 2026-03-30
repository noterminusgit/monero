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

// Extended test database that stores blocks and transactions in memory,
// beyond what BaseTestDB offers. Useful for blockchain integration tests
// that need actual block/tx storage.

#include "blockchain_db/testdb.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include <map>
#include <vector>

namespace test
{
  struct hash_cmp {
    bool operator()(const crypto::hash &a, const crypto::hash &b) const {
      return memcmp(&a, &b, sizeof(a)) < 0;
    }
  };
  class InMemoryDB : public cryptonote::BaseTestDB
  {
  public:
    InMemoryDB() : m_height(0), m_cumulative_difficulty(0) {}

    uint64_t height() const override { return m_height; }

    crypto::hash top_block_hash(uint64_t *block_height = nullptr) const override
    {
      if (block_height) *block_height = m_height > 0 ? m_height - 1 : 0;
      if (m_height == 0) return crypto::null_hash;
      auto it = m_block_hashes.find(m_height - 1);
      return it != m_block_hashes.end() ? it->second : crypto::null_hash;
    }

    cryptonote::block get_top_block() const override
    {
      if (m_height == 0) return cryptonote::block();
      auto it = m_blocks.find(m_height - 1);
      return it != m_blocks.end() ? it->second : cryptonote::block();
    }

    cryptonote::block get_block_from_height(const uint64_t& height) const override
    {
      auto it = m_blocks.find(height);
      if (it != m_blocks.end()) return it->second;
      return cryptonote::block();
    }

    crypto::hash get_block_hash_from_height(const uint64_t& height) const override
    {
      auto it = m_block_hashes.find(height);
      return it != m_block_hashes.end() ? it->second : crypto::null_hash;
    }

    bool block_exists(const crypto::hash& h, uint64_t *height = nullptr) const override
    {
      for (auto& [h2, hash] : m_block_hashes)
      {
        if (hash == h)
        {
          if (height) *height = h2;
          return true;
        }
      }
      return false;
    }

    uint64_t get_block_timestamp(const uint64_t& height) const override
    {
      auto it = m_timestamps.find(height);
      return it != m_timestamps.end() ? it->second : 0;
    }

    cryptonote::difficulty_type get_block_cumulative_difficulty(const uint64_t& height) const override
    {
      auto it = m_cumulative_difficulties.find(height);
      return it != m_cumulative_difficulties.end() ? it->second : 0;
    }

    uint64_t get_block_already_generated_coins(const uint64_t& height) const override
    {
      auto it = m_generated_coins.find(height);
      return it != m_generated_coins.end() ? it->second : 0;
    }

    size_t get_block_weight(const uint64_t& height) const override
    {
      auto it = m_block_weights.find(height);
      return it != m_block_weights.end() ? it->second : 0;
    }

    uint64_t get_block_long_term_weight(const uint64_t& height) const override
    {
      auto it = m_long_term_weights.find(height);
      return it != m_long_term_weights.end() ? it->second : 0;
    }

    uint8_t get_hard_fork_version(uint64_t height) const override { return m_hf_version; }
    void set_hard_fork_version(uint64_t height, uint8_t version) override { m_hf_version = version; }

    bool has_key_image(const crypto::key_image& img) const override
    {
      return m_key_images.count(img) > 0;
    }
    void add_spent_key(const crypto::key_image& k_image) override { m_key_images.insert(k_image); }
    void remove_spent_key(const crypto::key_image& k_image) override { m_key_images.erase(k_image); }

    uint64_t get_tx_count() const override { return m_txs.size(); }
    uint64_t get_database_size() const override { return m_db_size; }

    bool tx_exists(const crypto::hash& h) const override
    {
      return m_txs.count(h) > 0;
    }

    bool tx_exists(const crypto::hash& h, uint64_t& tx_index) const override
    {
      if (m_txs.count(h) > 0)
      {
        tx_index = 0;
        return true;
      }
      return false;
    }

    bool get_tx_blob(const crypto::hash& h, cryptonote::blobdata &tx) const override
    {
      auto it = m_txs.find(h);
      if (it == m_txs.end())
        return false;
      tx = cryptonote::t_serializable_object_to_blob(it->second);
      return true;
    }

    cryptonote::transaction get_tx(const crypto::hash& h) const override
    {
      auto it = m_txs.find(h);
      if (it != m_txs.end())
        return it->second;
      return cryptonote::transaction();
    }

    bool get_tx(const crypto::hash& h, cryptonote::transaction &tx) const override
    {
      auto it = m_txs.find(h);
      if (it == m_txs.end())
        return false;
      tx = it->second;
      return true;
    }

    // --- Mutation methods for test setup ---

    void add_test_block(uint64_t height, const cryptonote::block& blk, const crypto::hash& hash,
                        uint64_t timestamp = 0, cryptonote::difficulty_type cum_diff = 0,
                        uint64_t generated_coins = 0, size_t weight = 128, uint64_t ltw = 128)
    {
      m_blocks[height] = blk;
      m_block_hashes[height] = hash;
      m_timestamps[height] = timestamp;
      m_cumulative_difficulties[height] = cum_diff;
      m_generated_coins[height] = generated_coins;
      m_block_weights[height] = weight;
      m_long_term_weights[height] = ltw;
      if (height >= m_height) m_height = height + 1;
    }

    void add_test_tx(const crypto::hash& txid, const cryptonote::transaction& tx)
    {
      m_txs[txid] = tx;
    }

    void set_db_size(uint64_t s) { m_db_size = s; }

  private:
    uint64_t m_height;
    cryptonote::difficulty_type m_cumulative_difficulty;
    uint8_t m_hf_version = 16;
    uint64_t m_db_size = 0;

    std::map<uint64_t, cryptonote::block> m_blocks;
    std::map<uint64_t, crypto::hash> m_block_hashes;
    std::map<uint64_t, uint64_t> m_timestamps;
    std::map<uint64_t, cryptonote::difficulty_type> m_cumulative_difficulties;
    std::map<uint64_t, uint64_t> m_generated_coins;
    std::map<uint64_t, size_t> m_block_weights;
    std::map<uint64_t, uint64_t> m_long_term_weights;
    std::map<crypto::hash, cryptonote::transaction, hash_cmp> m_txs;
    std::set<crypto::key_image> m_key_images;
  };

} // namespace test
