// Copyright (c) 2020-2024, The Monero Project
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

#include "gtest/gtest.h"

#include "cryptonote_basic/merge_mining.h"
#include "crypto/crypto.h"

TEST(merge_mining, get_aux_slot_basic)
{
  crypto::hash id = crypto::rand<crypto::hash>();
  uint32_t slot = cryptonote::get_aux_slot(id, 0, 1);
  ASSERT_EQ(slot, 0u);
}

TEST(merge_mining, get_aux_slot_range)
{
  crypto::hash id = crypto::rand<crypto::hash>();
  for (uint32_t n = 1; n <= 256; ++n)
  {
    uint32_t slot = cryptonote::get_aux_slot(id, 0, n);
    ASSERT_LT(slot, n);
  }
}

TEST(merge_mining, get_aux_slot_deterministic)
{
  crypto::hash id = crypto::rand<crypto::hash>();
  uint32_t slot1 = cryptonote::get_aux_slot(id, 42, 10);
  uint32_t slot2 = cryptonote::get_aux_slot(id, 42, 10);
  ASSERT_EQ(slot1, slot2);
}

TEST(merge_mining, get_aux_slot_different_nonces)
{
  crypto::hash id = crypto::rand<crypto::hash>();
  // Different nonces may produce different slots (not guaranteed but likely over many trials)
  std::set<uint32_t> slots;
  for (uint32_t nonce = 0; nonce < 100; ++nonce)
    slots.insert(cryptonote::get_aux_slot(id, nonce, 256));
  // With 100 tries and 256 slots, we should get at least a few different values
  ASSERT_GT(slots.size(), 1u);
}

TEST(merge_mining, get_aux_slot_zero_chains_throws)
{
  crypto::hash id = crypto::rand<crypto::hash>();
  ASSERT_THROW(cryptonote::get_aux_slot(id, 0, 0), std::exception);
}

TEST(merge_mining, get_path_from_aux_slot_basic)
{
  uint32_t path = cryptonote::get_path_from_aux_slot(0, 1);
  // Single chain: path should be 0
  ASSERT_EQ(path, 0u);
}

TEST(merge_mining, get_path_from_aux_slot_zero_chains_throws)
{
  ASSERT_THROW(cryptonote::get_path_from_aux_slot(0, 0), std::exception);
}

TEST(merge_mining, get_path_from_aux_slot_out_of_range_throws)
{
  ASSERT_THROW(cryptonote::get_path_from_aux_slot(5, 3), std::exception);
}

TEST(merge_mining, encode_decode_mm_depth_roundtrip)
{
  for (uint32_t n = 1; n <= 256; ++n)
  {
    for (uint32_t nonce = 0; nonce < 10; ++nonce)
    {
      uint64_t depth = cryptonote::encode_mm_depth(n, nonce);
      uint32_t decoded_n = 0, decoded_nonce = 0;
      ASSERT_TRUE(cryptonote::decode_mm_depth(depth, decoded_n, decoded_nonce));
      ASSERT_EQ(n, decoded_n);
      ASSERT_EQ(nonce, decoded_nonce);
    }
  }
}

TEST(merge_mining, encode_mm_depth_zero_chains_throws)
{
  ASSERT_THROW(cryptonote::encode_mm_depth(0, 0), std::exception);
}

TEST(merge_mining, encode_mm_depth_too_many_chains_throws)
{
  ASSERT_THROW(cryptonote::encode_mm_depth(257, 0), std::exception);
}

TEST(merge_mining, encode_mm_depth_boundary)
{
  // Exactly 256 chains should work
  uint64_t depth = cryptonote::encode_mm_depth(256, 0);
  uint32_t n = 0, nonce = 0;
  ASSERT_TRUE(cryptonote::decode_mm_depth(depth, n, nonce));
  ASSERT_EQ(256u, n);
  ASSERT_EQ(0u, nonce);
}

TEST(merge_mining, encode_mm_depth_large_nonce)
{
  uint64_t depth = cryptonote::encode_mm_depth(1, 0xFFFFFFFF);
  uint32_t n = 0, nonce = 0;
  ASSERT_TRUE(cryptonote::decode_mm_depth(depth, n, nonce));
  ASSERT_EQ(1u, n);
  ASSERT_EQ(0xFFFFFFFF, nonce);
}

TEST(merge_mining, decode_mm_depth_various_encodings)
{
  // Test power-of-two chain counts (exercises different n_bits values)
  for (uint32_t bits = 0; bits < 8; ++bits)
  {
    uint32_t n = 1u << bits;
    if (n > 256) break;
    uint64_t depth = cryptonote::encode_mm_depth(n, 1234);
    uint32_t decoded_n = 0, decoded_nonce = 0;
    ASSERT_TRUE(cryptonote::decode_mm_depth(depth, decoded_n, decoded_nonce));
    ASSERT_EQ(n, decoded_n);
    ASSERT_EQ(1234u, decoded_nonce);
  }
}
