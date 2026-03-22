// Copyright (c) 2017-2024, The Monero Project
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

#include <boost/uuid/uuid.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <limits>
#include "gtest/gtest.h"
#include "crypto/crypto.h"
#include "cryptonote_protocol/cryptonote_protocol_defs.h"
#include "cryptonote_protocol/block_queue.h"
#include "net/net_utils_base.h"

static const boost::uuids::uuid &uuid1()
{
  static const boost::uuids::uuid uuid = crypto::rand<boost::uuids::uuid>();
  return uuid;
}

static const boost::uuids::uuid &uuid2()
{
  static const boost::uuids::uuid uuid = crypto::rand<boost::uuids::uuid>();
  return uuid;
}

static const boost::uuids::uuid &uuid3()
{
  static const boost::uuids::uuid uuid = crypto::rand<boost::uuids::uuid>();
  return uuid;
}

// Helper: generate a random crypto::hash
static crypto::hash make_hash(uint8_t val)
{
  crypto::hash h;
  memset(h.data, 0, sizeof(h.data));
  h.data[0] = val;
  return h;
}

// Helper: make a network_address from an ipv4 address
static epee::net_utils::network_address make_addr(uint32_t ip, uint16_t port)
{
  return epee::net_utils::network_address(epee::net_utils::ipv4_network_address(ip, port));
}

// Helper: create a vector of block_complete_entry of given size
static std::vector<cryptonote::block_complete_entry> make_bcel(size_t count)
{
  std::vector<cryptonote::block_complete_entry> bcel(count);
  for (size_t i = 0; i < count; ++i)
  {
    bcel[i].pruned = false;
    bcel[i].block = std::string(100, 'x'); // 100 bytes each
    bcel[i].block_weight = 0;
  }
  return bcel;
}

// ============================================================================
// Original tests
// ============================================================================

TEST(block_queue, empty)
{
  cryptonote::block_queue bq;
  ASSERT_EQ(bq.get_max_block_height(), 0);
}

TEST(block_queue, add_stepwise)
{
  epee::net_utils::network_address na;
  cryptonote::block_queue bq;
  bq.add_blocks(0, 200, uuid1(), na);
  ASSERT_EQ(bq.get_max_block_height(), 199);
  bq.add_blocks(200, 200, uuid1(), na);
  ASSERT_EQ(bq.get_max_block_height(), 399);
  bq.add_blocks(401, 200, uuid1(), na);
  ASSERT_EQ(bq.get_max_block_height(), 600);
  bq.add_blocks(400, 10, uuid1(), na);
  ASSERT_EQ(bq.get_max_block_height(), 600);
}

TEST(block_queue, flush_uuid)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 200, uuid1(), na);
  ASSERT_EQ(bq.get_max_block_height(), 199);
  bq.add_blocks(200, 200, uuid2(), na);
  ASSERT_EQ(bq.get_max_block_height(), 399);
  bq.flush_spans(uuid2());
  ASSERT_EQ(bq.get_max_block_height(), 199);
  bq.flush_spans(uuid1());
  ASSERT_EQ(bq.get_max_block_height(), 0);

  bq.add_blocks(0, 200, uuid1(), na);
  ASSERT_EQ(bq.get_max_block_height(), 199);
  bq.add_blocks(200, 200, uuid2(), na);
  ASSERT_EQ(bq.get_max_block_height(), 399);
  bq.flush_spans(uuid1());
  ASSERT_EQ(bq.get_max_block_height(), 399);
  bq.add_blocks(0, 200, uuid1(), na);
  ASSERT_EQ(bq.get_max_block_height(), 399);
}

TEST(block_queue, empty_max_height_zero)
{
  cryptonote::block_queue bq;
  ASSERT_EQ(bq.get_max_block_height(), 0u);
}

TEST(block_queue, add_single_span)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;
  bq.add_blocks(100, 50, uuid1(), na);
  ASSERT_EQ(bq.get_max_block_height(), 149u);
}

TEST(block_queue, add_overlapping_spans)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;
  bq.add_blocks(0, 100, uuid1(), na);
  bq.add_blocks(50, 100, uuid2(), na);
  ASSERT_EQ(bq.get_max_block_height(), 149u);
}

TEST(block_queue, add_gap_between_spans)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;
  bq.add_blocks(0, 100, uuid1(), na);
  bq.add_blocks(200, 100, uuid2(), na);
  ASSERT_EQ(bq.get_max_block_height(), 299u);
}

TEST(block_queue, flush_all)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;
  bq.add_blocks(0, 100, uuid1(), na);
  bq.add_blocks(100, 100, uuid2(), na);
  bq.flush_spans(uuid1());
  bq.flush_spans(uuid2());
  ASSERT_EQ(bq.get_max_block_height(), 0u);
}

TEST(block_queue, flush_nonexistent_uuid)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;
  bq.add_blocks(0, 100, uuid1(), na);
  bq.flush_spans(uuid3()); // uuid3 never added
  ASSERT_EQ(bq.get_max_block_height(), 99u); // unchanged
}

TEST(block_queue, multiple_spans_same_uuid)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;
  bq.add_blocks(0, 50, uuid1(), na);
  bq.add_blocks(50, 50, uuid1(), na);
  bq.add_blocks(100, 50, uuid1(), na);
  ASSERT_EQ(bq.get_max_block_height(), 149u);
  bq.flush_spans(uuid1());
  ASSERT_EQ(bq.get_max_block_height(), 0u);
}

TEST(block_queue, large_height)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;
  bq.add_blocks(1000000, 100, uuid1(), na);
  ASSERT_EQ(bq.get_max_block_height(), 1000099u);
}

TEST(block_queue, single_block_span)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;
  bq.add_blocks(42, 1, uuid1(), na);
  ASSERT_EQ(bq.get_max_block_height(), 42u);
}

TEST(block_queue, adjacent_spans_different_uuids)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;
  bq.add_blocks(0, 100, uuid1(), na);
  bq.add_blocks(100, 100, uuid2(), na);
  bq.add_blocks(200, 100, uuid3(), na);
  ASSERT_EQ(bq.get_max_block_height(), 299u);
  // Remove middle span
  bq.flush_spans(uuid2());
  ASSERT_EQ(bq.get_max_block_height(), 299u);
}

TEST(block_queue, flush_then_readd)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;
  bq.add_blocks(0, 100, uuid1(), na);
  bq.flush_spans(uuid1());
  ASSERT_EQ(bq.get_max_block_height(), 0u);
  bq.add_blocks(0, 200, uuid1(), na);
  ASSERT_EQ(bq.get_max_block_height(), 199u);
}

TEST(block_queue, add_zero_nblocks)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;
  // add_blocks with nblocks=0 throws "Empty span"
  ASSERT_ANY_THROW(bq.add_blocks(0, 0, uuid1(), na));
}

TEST(block_queue, add_at_height_zero)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;
  bq.add_blocks(0, 1, uuid1(), na);
  ASSERT_EQ(bq.get_max_block_height(), 0u);
}

TEST(block_queue, many_small_spans)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;
  for (uint64_t i = 0; i < 100; ++i)
    bq.add_blocks(i * 10, 10, uuid1(), na);
  ASSERT_EQ(bq.get_max_block_height(), 999u);
}

// ============================================================================
// Tests for add_blocks with block_complete_entry vector (filled spans)
// ============================================================================

TEST(block_queue, add_blocks_with_bcel)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  auto bcel = make_bcel(10);
  bq.add_blocks(0, std::move(bcel), uuid1(), na, 1024.0f, 1000);
  ASSERT_EQ(bq.get_max_block_height(), 9u);
}

TEST(block_queue, add_blocks_bcel_multiple)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 500.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid2(), na, 600.0f, 600);
  ASSERT_EQ(bq.get_max_block_height(), 9u);
}

TEST(block_queue, add_blocks_bcel_replaces_scheduled)
{
  // First add a scheduled (empty) span, then fill it with bcel
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, 10, uuid1(), na);
  ASSERT_EQ(bq.get_max_block_height(), 9u);

  // Now add filled blocks at same height - the bcel add calls remove_span internally
  bq.add_blocks(0, make_bcel(10), uuid1(), na, 1024.0f, 1000);
  ASSERT_EQ(bq.get_max_block_height(), 9u);
}

// ============================================================================
// Tests for flush_spans with all=true
// ============================================================================

TEST(block_queue, flush_spans_all_false_skips_filled)
{
  // flush_spans with all=false (default) only removes spans with empty blocks
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Add a filled span
  bq.add_blocks(0, make_bcel(10), uuid1(), na, 1024.0f, 1000);
  // Add a scheduled (empty) span
  bq.add_blocks(10, 10, uuid1(), na);

  // Default flush (all=false) should only remove scheduled spans
  bq.flush_spans(uuid1(), false);
  // The filled span should remain
  ASSERT_EQ(bq.get_max_block_height(), 9u);
}

TEST(block_queue, flush_spans_all_true_removes_everything)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Add a filled span
  bq.add_blocks(0, make_bcel(10), uuid1(), na, 1024.0f, 1000);
  // Add a scheduled (empty) span
  bq.add_blocks(10, 10, uuid1(), na);

  // flush with all=true should remove everything for this uuid
  bq.flush_spans(uuid1(), true);
  ASSERT_EQ(bq.get_max_block_height(), 0u);
}

// ============================================================================
// Tests for flush_stale_spans
// ============================================================================

TEST(block_queue, flush_stale_spans_removes_disconnected)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 100, uuid1(), na);
  bq.add_blocks(100, 100, uuid2(), na);

  // Only uuid1 is live
  std::set<boost::uuids::uuid> live;
  live.insert(uuid1());

  bq.flush_stale_spans(live);
  // uuid2's span should be removed (it's scheduled/empty and not live)
  ASSERT_EQ(bq.get_max_block_height(), 99u);
}

TEST(block_queue, flush_stale_spans_keeps_live)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 100, uuid1(), na);
  bq.add_blocks(100, 100, uuid2(), na);

  // Both are live
  std::set<boost::uuids::uuid> live;
  live.insert(uuid1());
  live.insert(uuid2());

  bq.flush_stale_spans(live);
  ASSERT_EQ(bq.get_max_block_height(), 199u);
}

TEST(block_queue, flush_stale_spans_empty_live_set)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 100, uuid1(), na);
  bq.add_blocks(100, 100, uuid2(), na);

  // No live connections, all scheduled spans removed
  std::set<boost::uuids::uuid> live;
  bq.flush_stale_spans(live);
  ASSERT_EQ(bq.get_max_block_height(), 0u);
}

TEST(block_queue, flush_stale_spans_keeps_filled)
{
  // flush_stale_spans only removes spans where blocks is empty
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Add a filled span for uuid1
  bq.add_blocks(0, make_bcel(10), uuid1(), na, 1024.0f, 1000);
  // Add a scheduled span for uuid2
  bq.add_blocks(10, 10, uuid2(), na);

  // No live connections
  std::set<boost::uuids::uuid> live;
  bq.flush_stale_spans(live);
  // The filled span for uuid1 should remain (flush_stale only removes empty/scheduled)
  // uuid2's scheduled span at 10 is removed, so max height is 9 (from uuid1's filled span)
  ASSERT_EQ(bq.get_max_block_height(), 9u);
  // Actually check: uuid1's filled span remains
  bool has_uuid1 = bq.has_spans(uuid1());
  ASSERT_TRUE(has_uuid1);
}

// ============================================================================
// Tests for remove_span
// ============================================================================

TEST(block_queue, remove_span_existing)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 100, uuid1(), na);
  bq.add_blocks(100, 100, uuid2(), na);

  bool removed = bq.remove_span(0);
  ASSERT_TRUE(removed);
  ASSERT_EQ(bq.get_max_block_height(), 199u);
}

TEST(block_queue, remove_span_nonexistent)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 100, uuid1(), na);

  bool removed = bq.remove_span(50); // no span starts at 50
  ASSERT_FALSE(removed);
  ASSERT_EQ(bq.get_max_block_height(), 99u);
}

TEST(block_queue, remove_span_returns_hashes)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  // Add a scheduled span then set hashes on it
  bq.add_blocks(0, 3, uuid1(), na);
  std::vector<crypto::hash> hashes = {make_hash(1), make_hash(2), make_hash(3)};
  bq.set_span_hashes(0, uuid1(), hashes);

  std::vector<crypto::hash> out_hashes;
  bool removed = bq.remove_span(0, &out_hashes);
  ASSERT_TRUE(removed);
  ASSERT_EQ(out_hashes.size(), 3u);
  ASSERT_EQ(out_hashes[0], make_hash(1));
  ASSERT_EQ(out_hashes[1], make_hash(2));
  ASSERT_EQ(out_hashes[2], make_hash(3));
}

TEST(block_queue, remove_span_empty_queue)
{
  cryptonote::block_queue bq;
  bool removed = bq.remove_span(0);
  ASSERT_FALSE(removed);
}

// ============================================================================
// Tests for remove_spans
// ============================================================================

TEST(block_queue, remove_spans_by_connection_and_height)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 50, uuid1(), na);
  bq.add_blocks(50, 50, uuid1(), na);
  bq.add_blocks(100, 50, uuid1(), na);
  bq.add_blocks(150, 50, uuid2(), na);

  // Remove all uuid1 spans at or below height 50
  bq.remove_spans(uuid1(), 50);
  // Spans at 0 and 50 for uuid1 should be removed
  // Span at 100 for uuid1 and 150 for uuid2 remain
  ASSERT_EQ(bq.get_max_block_height(), 199u);
}

TEST(block_queue, remove_spans_does_not_affect_other_uuid)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  // Use different start heights since block_queue uses std::set<span> sorted by
  // start_block_height, and two spans at the same height would collide.
  bq.add_blocks(0, 50, uuid1(), na);
  bq.add_blocks(50, 50, uuid2(), na);

  bq.remove_spans(uuid1(), 100);
  // uuid1 span removed, uuid2 remains
  ASSERT_TRUE(bq.has_spans(uuid2()));
  ASSERT_FALSE(bq.has_spans(uuid1()));
}

TEST(block_queue, remove_spans_none_below_height)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(100, 50, uuid1(), na);
  // Remove spans at or below 50 - none qualify
  bq.remove_spans(uuid1(), 50);
  ASSERT_EQ(bq.get_max_block_height(), 149u);
}

// ============================================================================
// Tests for get_next_needed_height
// ============================================================================

TEST(block_queue, get_next_needed_height_empty)
{
  cryptonote::block_queue bq;
  ASSERT_EQ(bq.get_next_needed_height(0), 0u);
  ASSERT_EQ(bq.get_next_needed_height(100), 100u);
}

TEST(block_queue, get_next_needed_height_contiguous)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 100, uuid1(), na);
  // blockchain_height=0, everything from 0..99 covered
  ASSERT_EQ(bq.get_next_needed_height(0), 100u);
}

TEST(block_queue, get_next_needed_height_with_gap)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 50, uuid1(), na);
  bq.add_blocks(100, 50, uuid2(), na);
  // blockchain_height=0, gap at 50..99
  ASSERT_EQ(bq.get_next_needed_height(0), 50u);
}

TEST(block_queue, get_next_needed_height_blockchain_ahead)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 50, uuid1(), na);
  // blockchain already at 100, span is below
  ASSERT_EQ(bq.get_next_needed_height(100), 100u);
}

TEST(block_queue, get_next_needed_height_adjacent_spans)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 50, uuid1(), na);
  bq.add_blocks(50, 50, uuid2(), na);
  bq.add_blocks(100, 50, uuid3(), na);
  ASSERT_EQ(bq.get_next_needed_height(0), 150u);
}

// ============================================================================
// Tests for print
// ============================================================================

TEST(block_queue, print_does_not_crash)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  // Empty queue
  bq.print();

  // With scheduled spans
  bq.add_blocks(0, 100, uuid1(), na);
  bq.print();

  // With filled spans
  bq.add_blocks(100, make_bcel(10), uuid2(), na, 1024.0f, 1000);
  bq.print();
}

// ============================================================================
// Tests for get_overview
// ============================================================================

TEST(block_queue, get_overview_empty)
{
  cryptonote::block_queue bq;
  ASSERT_EQ(bq.get_overview(0), "[]");
}

TEST(block_queue, get_overview_single_scheduled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  std::string overview = bq.get_overview(0);
  ASSERT_FALSE(overview.empty());
  ASSERT_EQ(overview.front(), '[');
  ASSERT_EQ(overview.back(), ']');
  // Scheduled spans show as "."
  ASSERT_NE(overview.find('.'), std::string::npos);
}

TEST(block_queue, get_overview_filled_at_blockchain_height)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(10), uuid1(), na, 1024.0f, 1000);
  std::string overview = bq.get_overview(0);
  // Filled span at blockchain_height shows as "m"
  ASSERT_NE(overview.find('m'), std::string::npos);
}

TEST(block_queue, get_overview_filled_not_at_blockchain_height)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(100, make_bcel(10), uuid1(), na, 1024.0f, 1000);
  std::string overview = bq.get_overview(0);
  // Filled span not at blockchain_height shows as "o", gap shown as "_"
  ASSERT_NE(overview.find('o'), std::string::npos);
}

TEST(block_queue, get_overview_with_gap)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  bq.add_blocks(100, 10, uuid2(), na);
  std::string overview = bq.get_overview(0);
  // Should contain underscores for the gap
  ASSERT_NE(overview.find('_'), std::string::npos);
}

// ============================================================================
// Tests for requested, have, have_height (via set_span_hashes and add_blocks with bcel)
// ============================================================================

TEST(block_queue, requested_with_hashes)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 3, uuid1(), na);
  std::vector<crypto::hash> hashes = {make_hash(10), make_hash(11), make_hash(12)};
  bq.set_span_hashes(0, uuid1(), hashes);

  ASSERT_TRUE(bq.requested(make_hash(10)));
  ASSERT_TRUE(bq.requested(make_hash(11)));
  ASSERT_TRUE(bq.requested(make_hash(12)));
  ASSERT_FALSE(bq.requested(make_hash(99)));
}

TEST(block_queue, requested_empty)
{
  cryptonote::block_queue bq;
  ASSERT_FALSE(bq.requested(make_hash(0)));
}

TEST(block_queue, have_with_filled_span)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  // First add a scheduled span with hashes, then replace with filled
  bq.add_blocks(0, 3, uuid1(), na);
  std::vector<crypto::hash> hashes = {make_hash(20), make_hash(21), make_hash(22)};
  bq.set_span_hashes(0, uuid1(), hashes);

  // Now add filled blocks at same height (which calls remove_span + re-insert with hashes)
  bq.add_blocks(0, make_bcel(3), uuid1(), na, 1024.0f, 300);

  // After add_blocks with bcel, the hashes from the previous span should be in have_blocks
  ASSERT_TRUE(bq.have(make_hash(20)));
  ASSERT_TRUE(bq.have(make_hash(21)));
  ASSERT_TRUE(bq.have(make_hash(22)));
  ASSERT_FALSE(bq.have(make_hash(99)));
}

TEST(block_queue, have_empty)
{
  cryptonote::block_queue bq;
  ASSERT_FALSE(bq.have(make_hash(0)));
}

TEST(block_queue, have_height_returns_correct_height)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(100, 3, uuid1(), na);
  std::vector<crypto::hash> hashes = {make_hash(30), make_hash(31), make_hash(32)};
  bq.set_span_hashes(100, uuid1(), hashes);

  // Replace with filled
  bq.add_blocks(100, make_bcel(3), uuid1(), na, 1024.0f, 300);

  ASSERT_EQ(bq.have_height(make_hash(30)), 100u);
  ASSERT_EQ(bq.have_height(make_hash(31)), 101u);
  ASSERT_EQ(bq.have_height(make_hash(32)), 102u);
}

TEST(block_queue, have_height_not_found)
{
  cryptonote::block_queue bq;
  ASSERT_EQ(bq.have_height(make_hash(0)), std::numeric_limits<uint64_t>::max());
}

// ============================================================================
// Tests for set_span_hashes
// ============================================================================

TEST(block_queue, set_span_hashes_basic)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 3, uuid1(), na);

  std::vector<crypto::hash> hashes = {make_hash(40), make_hash(41), make_hash(42)};
  bq.set_span_hashes(0, uuid1(), hashes);

  // The hashes should now be in requested_hashes
  ASSERT_TRUE(bq.requested(make_hash(40)));
  ASSERT_TRUE(bq.requested(make_hash(41)));
  ASSERT_TRUE(bq.requested(make_hash(42)));
}

TEST(block_queue, set_span_hashes_wrong_height)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 3, uuid1(), na);

  // Set hashes for wrong height - should not match, no hashes set
  std::vector<crypto::hash> hashes = {make_hash(50)};
  bq.set_span_hashes(999, uuid1(), hashes);
  ASSERT_FALSE(bq.requested(make_hash(50)));
}

TEST(block_queue, set_span_hashes_wrong_uuid)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 3, uuid1(), na);

  // Set hashes for wrong UUID
  std::vector<crypto::hash> hashes = {make_hash(60)};
  bq.set_span_hashes(0, uuid2(), hashes);
  ASSERT_FALSE(bq.requested(make_hash(60)));
}

// ============================================================================
// Tests for get_next_span
// ============================================================================

TEST(block_queue, get_next_span_empty)
{
  cryptonote::block_queue bq;
  uint64_t height;
  std::vector<cryptonote::block_complete_entry> bcel;
  boost::uuids::uuid conn_id;
  epee::net_utils::network_address addr;

  ASSERT_FALSE(bq.get_next_span(height, bcel, conn_id, addr));
}

TEST(block_queue, get_next_span_filled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);

  uint64_t height;
  std::vector<cryptonote::block_complete_entry> bcel;
  boost::uuids::uuid conn_id;
  epee::net_utils::network_address addr;

  // Default filled=true
  ASSERT_TRUE(bq.get_next_span(height, bcel, conn_id, addr, true));
  ASSERT_EQ(height, 0u);
  ASSERT_EQ(bcel.size(), 5u);
  ASSERT_EQ(conn_id, uuid1());
}

TEST(block_queue, get_next_span_no_filled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  // Only scheduled spans
  bq.add_blocks(0, 10, uuid1(), na);

  uint64_t height;
  std::vector<cryptonote::block_complete_entry> bcel;
  boost::uuids::uuid conn_id;
  epee::net_utils::network_address addr;

  // Looking for filled=true should return false
  ASSERT_FALSE(bq.get_next_span(height, bcel, conn_id, addr, true));

  // Looking for filled=false should return the scheduled span
  ASSERT_TRUE(bq.get_next_span(height, bcel, conn_id, addr, false));
  ASSERT_EQ(height, 0u);
  ASSERT_EQ(conn_id, uuid1());
}

TEST(block_queue, get_next_span_skips_unfilled)
{
  // First span is scheduled, second is filled
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, 10, uuid1(), na);
  bq.add_blocks(10, make_bcel(5), uuid2(), na, 1024.0f, 500);

  uint64_t height;
  std::vector<cryptonote::block_complete_entry> bcel;
  boost::uuids::uuid conn_id;
  epee::net_utils::network_address addr;

  // filled=true should skip the first span and return the second
  ASSERT_TRUE(bq.get_next_span(height, bcel, conn_id, addr, true));
  ASSERT_EQ(height, 10u);
  ASSERT_EQ(bcel.size(), 5u);
}

// ============================================================================
// Tests for has_next_span (uuid overload)
// ============================================================================

TEST(block_queue, has_next_span_uuid_empty)
{
  cryptonote::block_queue bq;
  bool filled;
  boost::posix_time::ptime time;
  ASSERT_FALSE(bq.has_next_span(uuid1(), filled, time));
}

TEST(block_queue, has_next_span_uuid_matching)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);

  bool filled;
  boost::posix_time::ptime time;
  ASSERT_TRUE(bq.has_next_span(uuid1(), filled, time));
  ASSERT_FALSE(filled); // scheduled, not filled
}

TEST(block_queue, has_next_span_uuid_not_matching)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);

  bool filled;
  boost::posix_time::ptime time;
  ASSERT_FALSE(bq.has_next_span(uuid2(), filled, time));
}

TEST(block_queue, has_next_span_uuid_filled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);

  bool filled;
  boost::posix_time::ptime time;
  ASSERT_TRUE(bq.has_next_span(uuid1(), filled, time));
  ASSERT_TRUE(filled);
}

// ============================================================================
// Tests for has_next_span (height overload)
// ============================================================================

TEST(block_queue, has_next_span_height_empty)
{
  cryptonote::block_queue bq;
  bool filled;
  boost::posix_time::ptime time;
  boost::uuids::uuid conn_id;
  ASSERT_FALSE(bq.has_next_span(0, filled, time, conn_id));
}

TEST(block_queue, has_next_span_height_matching)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);

  bool filled;
  boost::posix_time::ptime time;
  boost::uuids::uuid conn_id;
  // height=0, first span starts at 0, so it should match
  ASSERT_TRUE(bq.has_next_span(0, filled, time, conn_id));
  ASSERT_FALSE(filled);
  ASSERT_EQ(conn_id, uuid1());
}

TEST(block_queue, has_next_span_height_too_low)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(100, 10, uuid1(), na);

  bool filled;
  boost::posix_time::ptime time;
  boost::uuids::uuid conn_id;
  // height=50 is less than start_block_height=100, so check returns false
  // because i->start_block_height > height
  ASSERT_FALSE(bq.has_next_span(50, filled, time, conn_id));
}

TEST(block_queue, has_next_span_height_at_start)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(100, 10, uuid1(), na);

  bool filled;
  boost::posix_time::ptime time;
  boost::uuids::uuid conn_id;
  // height=100, exactly at start
  ASSERT_TRUE(bq.has_next_span(100, filled, time, conn_id));
  ASSERT_EQ(conn_id, uuid1());
}

TEST(block_queue, has_next_span_height_above_start)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);

  bool filled;
  boost::posix_time::ptime time;
  boost::uuids::uuid conn_id;
  // height=200 is above start, should still match (checks start_block_height <= height)
  ASSERT_TRUE(bq.has_next_span(200, filled, time, conn_id));
}

// ============================================================================
// Tests for get_next_span_if_scheduled
// ============================================================================

TEST(block_queue, get_next_span_if_scheduled_empty)
{
  cryptonote::block_queue bq;
  std::vector<crypto::hash> hashes;
  boost::uuids::uuid conn_id;
  boost::posix_time::ptime time;

  auto result = bq.get_next_span_if_scheduled(hashes, conn_id, time);
  ASSERT_EQ(result.first, 0u);
  ASSERT_EQ(result.second, 0u);
}

TEST(block_queue, get_next_span_if_scheduled_with_scheduled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(10, 5, uuid1(), na);

  std::vector<crypto::hash> hashes;
  boost::uuids::uuid conn_id;
  boost::posix_time::ptime time;

  auto result = bq.get_next_span_if_scheduled(hashes, conn_id, time);
  ASSERT_EQ(result.first, 10u);
  ASSERT_EQ(result.second, 5u);
  ASSERT_EQ(conn_id, uuid1());
}

TEST(block_queue, get_next_span_if_scheduled_filled_returns_zero)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);

  std::vector<crypto::hash> hashes;
  boost::uuids::uuid conn_id;
  boost::posix_time::ptime time;

  auto result = bq.get_next_span_if_scheduled(hashes, conn_id, time);
  ASSERT_EQ(result.first, 0u);
  ASSERT_EQ(result.second, 0u);
}

TEST(block_queue, get_next_span_if_scheduled_with_hashes)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 3, uuid1(), na);
  std::vector<crypto::hash> set_hashes = {make_hash(70), make_hash(71), make_hash(72)};
  bq.set_span_hashes(0, uuid1(), set_hashes);

  std::vector<crypto::hash> out_hashes;
  boost::uuids::uuid conn_id;
  boost::posix_time::ptime time;

  auto result = bq.get_next_span_if_scheduled(out_hashes, conn_id, time);
  ASSERT_EQ(result.first, 0u);
  ASSERT_EQ(result.second, 3u);
  ASSERT_EQ(out_hashes.size(), 3u);
  ASSERT_EQ(out_hashes[0], make_hash(70));
}

// ============================================================================
// Tests for reset_next_span_time
// ============================================================================

TEST(block_queue, reset_next_span_time_basic)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);

  boost::posix_time::ptime new_time = boost::posix_time::from_time_t(1000000);
  bq.reset_next_span_time(new_time);

  // Verify the time was set by retrieving it
  std::vector<crypto::hash> hashes;
  boost::uuids::uuid conn_id;
  boost::posix_time::ptime time;

  auto result = bq.get_next_span_if_scheduled(hashes, conn_id, time);
  ASSERT_EQ(result.first, 0u);
  ASSERT_EQ(result.second, 10u);
  ASSERT_EQ(time, new_time);
}

TEST(block_queue, reset_next_span_time_empty_throws)
{
  cryptonote::block_queue bq;
  boost::posix_time::ptime new_time = boost::posix_time::from_time_t(1000000);
  ASSERT_ANY_THROW(bq.reset_next_span_time(new_time));
}

TEST(block_queue, reset_next_span_time_filled_throws)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);
  boost::posix_time::ptime new_time = boost::posix_time::from_time_t(1000000);
  ASSERT_ANY_THROW(bq.reset_next_span_time(new_time));
}

// ============================================================================
// Tests for get_data_size
// ============================================================================

TEST(block_queue, get_data_size_empty)
{
  cryptonote::block_queue bq;
  ASSERT_EQ(bq.get_data_size(), 0u);
}

TEST(block_queue, get_data_size_scheduled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  // Scheduled spans have size=0
  bq.add_blocks(0, 10, uuid1(), na);
  ASSERT_EQ(bq.get_data_size(), 0u);
}

TEST(block_queue, get_data_size_filled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid2(), na, 1024.0f, 300);
  ASSERT_EQ(bq.get_data_size(), 800u);
}

// ============================================================================
// Tests for get_num_filled_spans_prefix
// ============================================================================

TEST(block_queue, get_num_filled_spans_prefix_empty)
{
  cryptonote::block_queue bq;
  ASSERT_EQ(bq.get_num_filled_spans_prefix(), 0u);
}

TEST(block_queue, get_num_filled_spans_prefix_all_filled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid2(), na, 1024.0f, 500);
  ASSERT_EQ(bq.get_num_filled_spans_prefix(), 2u);
}

TEST(block_queue, get_num_filled_spans_prefix_stops_at_scheduled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);
  bq.add_blocks(5, 5, uuid2(), na); // scheduled
  bq.add_blocks(10, make_bcel(5), uuid1(), na, 1024.0f, 500);
  // Prefix stops at first scheduled span
  ASSERT_EQ(bq.get_num_filled_spans_prefix(), 1u);
}

TEST(block_queue, get_num_filled_spans_prefix_all_scheduled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  bq.add_blocks(10, 10, uuid2(), na);
  ASSERT_EQ(bq.get_num_filled_spans_prefix(), 0u);
}

// ============================================================================
// Tests for get_num_filled_spans
// ============================================================================

TEST(block_queue, get_num_filled_spans_empty)
{
  cryptonote::block_queue bq;
  ASSERT_EQ(bq.get_num_filled_spans(), 0u);
}

TEST(block_queue, get_num_filled_spans_mixed)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);
  bq.add_blocks(5, 5, uuid2(), na); // scheduled
  bq.add_blocks(10, make_bcel(5), uuid1(), na, 1024.0f, 500);
  // Total filled spans = 2 (even though they're not contiguous prefix)
  ASSERT_EQ(bq.get_num_filled_spans(), 2u);
}

TEST(block_queue, get_num_filled_spans_all_scheduled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  bq.add_blocks(10, 10, uuid2(), na);
  ASSERT_EQ(bq.get_num_filled_spans(), 0u);
}

// ============================================================================
// Tests for get_last_known_hash
// ============================================================================

TEST(block_queue, get_last_known_hash_empty)
{
  cryptonote::block_queue bq;
  crypto::hash h = bq.get_last_known_hash(uuid1());
  ASSERT_EQ(h, crypto::null_hash);
}

TEST(block_queue, get_last_known_hash_with_hashes)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 3, uuid1(), na);
  std::vector<crypto::hash> hashes1 = {make_hash(80), make_hash(81), make_hash(82)};
  bq.set_span_hashes(0, uuid1(), hashes1);

  bq.add_blocks(3, 2, uuid1(), na);
  std::vector<crypto::hash> hashes2 = {make_hash(83), make_hash(84)};
  bq.set_span_hashes(3, uuid1(), hashes2);

  // Last known hash should be the last hash of the highest span
  crypto::hash h = bq.get_last_known_hash(uuid1());
  ASSERT_EQ(h, make_hash(84));
}

TEST(block_queue, get_last_known_hash_wrong_uuid)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 3, uuid1(), na);
  std::vector<crypto::hash> hashes = {make_hash(90), make_hash(91), make_hash(92)};
  bq.set_span_hashes(0, uuid1(), hashes);

  crypto::hash h = bq.get_last_known_hash(uuid2());
  ASSERT_EQ(h, crypto::null_hash);
}

TEST(block_queue, get_last_known_hash_partial_hashes)
{
  // If hashes.size() != nblocks, the span is skipped
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 5, uuid1(), na);
  // Set only 3 hashes for a span of 5 blocks
  std::vector<crypto::hash> hashes = {make_hash(1), make_hash(2), make_hash(3)};
  bq.set_span_hashes(0, uuid1(), hashes);

  // hashes.size() (3) != nblocks (5), so it should not count
  crypto::hash h = bq.get_last_known_hash(uuid1());
  ASSERT_EQ(h, crypto::null_hash);
}

// ============================================================================
// Tests for has_spans
// ============================================================================

TEST(block_queue, has_spans_empty)
{
  cryptonote::block_queue bq;
  ASSERT_FALSE(bq.has_spans(uuid1()));
}

TEST(block_queue, has_spans_present)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  ASSERT_TRUE(bq.has_spans(uuid1()));
  ASSERT_FALSE(bq.has_spans(uuid2()));
}

TEST(block_queue, has_spans_after_flush)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  bq.flush_spans(uuid1());
  ASSERT_FALSE(bq.has_spans(uuid1()));
}

// ============================================================================
// Tests for get_speed
// ============================================================================

TEST(block_queue, get_speed_empty)
{
  cryptonote::block_queue bq;
  // No spans, returns 1.0 (default good speed)
  ASSERT_FLOAT_EQ(bq.get_speed(uuid1()), 1.0f);
}

TEST(block_queue, get_speed_only_scheduled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  // Scheduled spans have no rate, so speed is 1.0 (not found)
  ASSERT_FLOAT_EQ(bq.get_speed(uuid1()), 1.0f);
}

TEST(block_queue, get_speed_single_connection)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);
  // Only one connection, its speed relative to best is 1.0
  ASSERT_FLOAT_EQ(bq.get_speed(uuid1()), 1.0f);
}

TEST(block_queue, get_speed_two_connections)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid2(), na, 512.0f, 500);

  // uuid1 is faster: speed = 1024/1024 = 1.0
  ASSERT_FLOAT_EQ(bq.get_speed(uuid1()), 1.0f);
  // uuid2 is slower: speed = 512/1024 = 0.5
  ASSERT_FLOAT_EQ(bq.get_speed(uuid2()), 0.5f);
}

TEST(block_queue, get_speed_unknown_connection)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);
  // uuid3 not present, returns 1.0
  ASSERT_FLOAT_EQ(bq.get_speed(uuid3()), 1.0f);
}

// ============================================================================
// Tests for get_download_rate
// ============================================================================

TEST(block_queue, get_download_rate_empty)
{
  cryptonote::block_queue bq;
  ASSERT_FLOAT_EQ(bq.get_download_rate(uuid1()), 0.0f);
}

TEST(block_queue, get_download_rate_scheduled_only)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  // Scheduled spans have empty blocks, so download rate is 0
  ASSERT_FLOAT_EQ(bq.get_download_rate(uuid1()), 0.0f);
}

TEST(block_queue, get_download_rate_single_filled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 2048.0f, 500);
  ASSERT_FLOAT_EQ(bq.get_download_rate(uuid1()), 2048.0f);
}

TEST(block_queue, get_download_rate_multiple_filled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1000.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid1(), na, 2000.0f, 500);
  // Rate averages: (1000 + 2000) / 2 = 1500
  ASSERT_FLOAT_EQ(bq.get_download_rate(uuid1()), 1500.0f);
}

TEST(block_queue, get_download_rate_wrong_uuid)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 2048.0f, 500);
  ASSERT_FLOAT_EQ(bq.get_download_rate(uuid2()), 0.0f);
}

// ============================================================================
// Tests for foreach
// ============================================================================

TEST(block_queue, foreach_empty)
{
  cryptonote::block_queue bq;
  int count = 0;
  bool result = bq.foreach([&](const cryptonote::block_queue::span &s) -> bool {
    ++count;
    return true;
  });
  ASSERT_TRUE(result);
  ASSERT_EQ(count, 0);
}

TEST(block_queue, foreach_counts_spans)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  bq.add_blocks(10, 10, uuid2(), na);
  bq.add_blocks(20, 10, uuid3(), na);

  int count = 0;
  bq.foreach([&](const cryptonote::block_queue::span &s) -> bool {
    ++count;
    return true;
  });
  ASSERT_EQ(count, 3);
}

TEST(block_queue, foreach_early_exit)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  bq.add_blocks(10, 10, uuid2(), na);
  bq.add_blocks(20, 10, uuid3(), na);

  int count = 0;
  bool result = bq.foreach([&](const cryptonote::block_queue::span &s) -> bool {
    ++count;
    return false; // stop after first
  });
  ASSERT_FALSE(result);
  ASSERT_EQ(count, 1);
}

TEST(block_queue, foreach_reads_span_data)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(100, 50, uuid1(), na);

  uint64_t seen_start = 0;
  uint64_t seen_nblocks = 0;
  bq.foreach([&](const cryptonote::block_queue::span &s) -> bool {
    seen_start = s.start_block_height;
    seen_nblocks = s.nblocks;
    return true;
  });
  ASSERT_EQ(seen_start, 100u);
  ASSERT_EQ(seen_nblocks, 50u);
}

// ============================================================================
// Tests for reserve_span
// ============================================================================

TEST(block_queue, reserve_span_basic)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Create block hashes for heights 10..19 (last_block_height = 19, 10 hashes)
  // Note: block_hashes.size() must be <= last_block_height to pass the guard check.
  std::vector<std::pair<crypto::hash, uint64_t>> block_hashes;
  for (uint64_t i = 0; i < 10; ++i)
    block_hashes.push_back(std::make_pair(make_hash(static_cast<uint8_t>(i)), 10 + i));

  // pruning_seed=0 means unpruned peer
  // span_start_height = last_block_height - block_hashes.size() + 1 = 19 - 10 + 1 = 10
  auto result = bq.reserve_span(10, 19, 10, uuid1(), na, false, 0, 0, 100, block_hashes);
  ASSERT_EQ(result.first, 10u);
  ASSERT_EQ(result.second, 10u);
  ASSERT_EQ(bq.get_max_block_height(), 19u);
}

TEST(block_queue, reserve_span_last_less_than_first)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  std::vector<std::pair<crypto::hash, uint64_t>> block_hashes;
  auto result = bq.reserve_span(10, 5, 10, uuid1(), na, false, 0, 0, 100, block_hashes);
  ASSERT_EQ(result.first, 0u);
  ASSERT_EQ(result.second, 0u);
}

TEST(block_queue, reserve_span_max_blocks_zero)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  std::vector<std::pair<crypto::hash, uint64_t>> block_hashes;
  block_hashes.push_back(std::make_pair(make_hash(1), 0));
  auto result = bq.reserve_span(0, 0, 0, uuid1(), na, false, 0, 0, 100, block_hashes);
  ASSERT_EQ(result.first, 0u);
  ASSERT_EQ(result.second, 0u);
}

TEST(block_queue, reserve_span_limits_to_max_blocks)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // 20 hashes covering heights 20..39 (last_block_height=39, so 20 <= 39 passes guard)
  std::vector<std::pair<crypto::hash, uint64_t>> block_hashes;
  for (uint64_t i = 0; i < 20; ++i)
    block_hashes.push_back(std::make_pair(make_hash(static_cast<uint8_t>(i)), 20 + i));

  // max_blocks=5, so only reserve 5 blocks
  // span_start_height = 39 - 20 + 1 = 20
  auto result = bq.reserve_span(20, 39, 5, uuid1(), na, false, 0, 0, 100, block_hashes);
  ASSERT_EQ(result.first, 20u);
  ASSERT_EQ(result.second, 5u);
}

TEST(block_queue, reserve_span_skips_already_requested)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // 10 hashes covering heights 10..19 (last_block_height=19, so 10 <= 19 passes guard)
  std::vector<std::pair<crypto::hash, uint64_t>> block_hashes;
  for (uint64_t i = 0; i < 10; ++i)
    block_hashes.push_back(std::make_pair(make_hash(static_cast<uint8_t>(i)), 10 + i));

  // First reserve: span_start_height = 19 - 10 + 1 = 10, reserves 5 blocks at heights 10..14
  bq.reserve_span(10, 19, 5, uuid1(), na, false, 0, 0, 100, block_hashes);

  // Now try to reserve again - the already-requested hashes should be skipped
  // Skips first 5 hashes (already requested), then reserves next 5 at heights 15..19
  auto result = bq.reserve_span(10, 19, 5, uuid2(), na, false, 0, 0, 100, block_hashes);
  ASSERT_EQ(result.first, 15u);
  ASSERT_EQ(result.second, 5u);
}

TEST(block_queue, reserve_span_too_many_hashes)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  // block_hashes.size() > last_block_height triggers early return
  std::vector<std::pair<crypto::hash, uint64_t>> block_hashes;
  for (uint64_t i = 0; i < 20; ++i)
    block_hashes.push_back(std::make_pair(make_hash(static_cast<uint8_t>(i)), i));

  // last_block_height = 10, but block_hashes has 20 entries -> 20 > 10 -> early return
  auto result = bq.reserve_span(0, 10, 10, uuid1(), na, false, 0, 0, 100, block_hashes);
  ASSERT_EQ(result.first, 0u);
  ASSERT_EQ(result.second, 0u);
}

TEST(block_queue, reserve_span_all_already_requested)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  std::vector<std::pair<crypto::hash, uint64_t>> block_hashes;
  for (uint64_t i = 0; i < 5; ++i)
    block_hashes.push_back(std::make_pair(make_hash(static_cast<uint8_t>(i)), i));

  // Reserve all
  bq.reserve_span(0, 4, 5, uuid1(), na, false, 0, 0, 100, block_hashes);

  // Try to reserve same hashes again - all already requested
  auto result = bq.reserve_span(0, 4, 5, uuid2(), na, false, 0, 0, 100, block_hashes);
  ASSERT_EQ(result.first, 0u);
  ASSERT_EQ(result.second, 0u);
}

// ============================================================================
// Tests for erase_block (tested indirectly through other operations)
// ============================================================================

TEST(block_queue, erase_cleans_up_hashes)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 3, uuid1(), na);
  std::vector<crypto::hash> hashes = {make_hash(100), make_hash(101), make_hash(102)};
  bq.set_span_hashes(0, uuid1(), hashes);

  ASSERT_TRUE(bq.requested(make_hash(100)));

  // Remove the span, which calls erase_block internally
  bq.remove_span(0);
  ASSERT_FALSE(bq.requested(make_hash(100)));
  ASSERT_FALSE(bq.requested(make_hash(101)));
  ASSERT_FALSE(bq.requested(make_hash(102)));
}

// ============================================================================
// Tests for requested_internal (tested indirectly via requested)
// ============================================================================

TEST(block_queue, requested_after_set_hashes_and_remove)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 2, uuid1(), na);
  bq.set_span_hashes(0, uuid1(), {make_hash(110), make_hash(111)});
  ASSERT_TRUE(bq.requested(make_hash(110)));
  ASSERT_TRUE(bq.requested(make_hash(111)));

  bq.flush_spans(uuid1());
  ASSERT_FALSE(bq.requested(make_hash(110)));
  ASSERT_FALSE(bq.requested(make_hash(111)));
}

// ============================================================================
// Tests for network address handling
// ============================================================================

TEST(block_queue, get_next_span_returns_addr)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0xC0A80001, 8080); // 192.168.0.1:8080

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);

  uint64_t height;
  std::vector<cryptonote::block_complete_entry> bcel;
  boost::uuids::uuid conn_id;
  epee::net_utils::network_address addr;

  ASSERT_TRUE(bq.get_next_span(height, bcel, conn_id, addr, true));
  // The address should be set
  ASSERT_TRUE(addr.equal(na));
}

// ============================================================================
// Integration-style tests combining multiple operations
// ============================================================================

TEST(block_queue, full_workflow)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na1 = make_addr(0x01020304, 18080);
  epee::net_utils::network_address na2 = make_addr(0x05060708, 18080);

  // 1. Reserve spans for two connections
  // 20 hashes covering heights 20..39 (last_block_height=39, so 20 <= 39 passes guard)
  std::vector<std::pair<crypto::hash, uint64_t>> block_hashes;
  for (uint64_t i = 0; i < 20; ++i)
    block_hashes.push_back(std::make_pair(make_hash(static_cast<uint8_t>(i)), 20 + i));

  // span_start_height = 39 - 20 + 1 = 20; reserves heights 20..29
  auto r1 = bq.reserve_span(20, 39, 10, uuid1(), na1, false, 0, 0, 100, block_hashes);
  ASSERT_EQ(r1.first, 20u);
  ASSERT_EQ(r1.second, 10u);

  // Skips first 10 (already requested), reserves heights 30..39
  auto r2 = bq.reserve_span(20, 39, 10, uuid2(), na2, false, 0, 0, 100, block_hashes);
  ASSERT_EQ(r2.first, 30u);
  ASSERT_EQ(r2.second, 10u);

  // 2. Verify spans are scheduled
  ASSERT_EQ(bq.get_num_filled_spans(), 0u);
  ASSERT_TRUE(bq.has_spans(uuid1()));
  ASSERT_TRUE(bq.has_spans(uuid2()));

  // 3. Fill the first span with blocks
  bq.add_blocks(20, make_bcel(10), uuid1(), na1, 2048.0f, 10000);
  ASSERT_EQ(bq.get_num_filled_spans(), 1u);
  ASSERT_EQ(bq.get_num_filled_spans_prefix(), 1u);

  // 4. Get next filled span
  uint64_t height;
  std::vector<cryptonote::block_complete_entry> bcel;
  boost::uuids::uuid conn_id;
  epee::net_utils::network_address addr;

  ASSERT_TRUE(bq.get_next_span(height, bcel, conn_id, addr, true));
  ASSERT_EQ(height, 20u);
  ASSERT_EQ(bcel.size(), 10u);

  // 5. Check overview
  std::string overview = bq.get_overview(20);
  ASSERT_FALSE(overview.empty());

  // 6. Check data size
  ASSERT_EQ(bq.get_data_size(), 10000u);

  // 7. Remove first span after processing
  bq.remove_span(20);
  ASSERT_EQ(bq.get_num_filled_spans(), 0u);

  // 8. Flush stale (remove uuid2 if disconnected)
  std::set<boost::uuids::uuid> live;
  live.insert(uuid1());
  bq.flush_stale_spans(live);
  ASSERT_FALSE(bq.has_spans(uuid2()));
}

TEST(block_queue, concurrent_operations)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Add many spans from different connections
  for (int i = 0; i < 10; ++i)
  {
    boost::uuids::uuid u = crypto::rand<boost::uuids::uuid>();
    bq.add_blocks(i * 10, 10, u, na);
  }

  ASSERT_EQ(bq.get_max_block_height(), 99u);

  // foreach to count all spans
  int count = 0;
  bq.foreach([&](const cryptonote::block_queue::span &s) -> bool {
    ++count;
    return true;
  });
  ASSERT_EQ(count, 10);
}

TEST(block_queue, add_blocks_bcel_with_prior_hashes_populates_have)
{
  // This tests the path in add_blocks(bcel) where remove_span returns hashes
  // and those hashes are then added to have_blocks
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Step 1: Add a scheduled span
  bq.add_blocks(50, 3, uuid1(), na);

  // Step 2: Set hashes on that span
  std::vector<crypto::hash> hashes = {make_hash(200), make_hash(201), make_hash(202)};
  bq.set_span_hashes(50, uuid1(), hashes);

  // Step 3: Verify they are in requested
  ASSERT_TRUE(bq.requested(make_hash(200)));

  // Step 4: Now add filled blocks at same height - remove_span will get the hashes
  bq.add_blocks(50, make_bcel(3), uuid1(), na, 1024.0f, 300);

  // Step 5: The hashes should now be in have_blocks
  ASSERT_TRUE(bq.have(make_hash(200)));
  ASSERT_TRUE(bq.have(make_hash(201)));
  ASSERT_TRUE(bq.have(make_hash(202)));

  // Step 6: Check heights
  ASSERT_EQ(bq.have_height(make_hash(200)), 50u);
  ASSERT_EQ(bq.have_height(make_hash(201)), 51u);
  ASSERT_EQ(bq.have_height(make_hash(202)), 52u);
}

TEST(block_queue, overview_with_span_below_blockchain_height)
{
  // Tests the "<" path in get_overview when expected > i->start_block_height
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  // blockchain_height=50, span starts at 0 which is below 50
  std::string overview = bq.get_overview(50);
  ASSERT_NE(overview.find('<'), std::string::npos);
}

TEST(block_queue, reserve_span_with_sync_pruned)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // 10 hashes covering heights 10..19 (last_block_height=19, so 10 <= 19 passes guard)
  std::vector<std::pair<crypto::hash, uint64_t>> block_hashes;
  for (uint64_t i = 0; i < 10; ++i)
    block_hashes.push_back(std::make_pair(make_hash(static_cast<uint8_t>(i + 150)), 10 + i));

  // sync_pruned_blocks=true, local_pruning_seed=0 (unpruned), pruning_seed=0 (unpruned)
  // span_start_height = 19 - 10 + 1 = 10
  auto result = bq.reserve_span(10, 19, 10, uuid1(), na, true, 0, 0, 100, block_hashes);
  // With both seeds=0, has_unpruned_block returns true for all blocks
  // first_is_pruned = sync_pruned_blocks && !has_unpruned_block(10, 100, 0)
  // has_unpruned_block with seed=0 returns true, so first_is_pruned = false
  // Loop: sync_pruned_blocks && first_is_pruned(false) == has_unpruned_block(true) -> false == true is false, so no break
  // So it should reserve blocks
  ASSERT_GT(result.second, 0u);
}

TEST(block_queue, get_speed_multiple_spans_same_connection)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Multiple filled spans for the same connection - rates should average
  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1000.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid1(), na, 3000.0f, 500);
  // Average: (1000 + 3000)/2 = 2000
  // Only one connection, so speed = 2000/2000 = 1.0
  ASSERT_FLOAT_EQ(bq.get_speed(uuid1()), 1.0f);
}

TEST(block_queue, download_rate_three_spans)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1000.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid1(), na, 2000.0f, 500);
  bq.add_blocks(10, make_bcel(5), uuid1(), na, 3000.0f, 500);
  // Pseudo-average: first 1000, then (1000+2000)/2 = 1500, then (1500+3000)/2 = 2250
  ASSERT_FLOAT_EQ(bq.get_download_rate(uuid1()), 2250.0f);
}

TEST(block_queue, foreach_stop_at_second)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  bq.add_blocks(10, 10, uuid2(), na);
  bq.add_blocks(20, 10, uuid3(), na);

  int count = 0;
  bq.foreach([&](const cryptonote::block_queue::span &s) -> bool {
    ++count;
    return count < 2; // stop after second iteration
  });
  ASSERT_EQ(count, 2);
}

TEST(block_queue, get_next_needed_height_overlapping_spans)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  // Overlapping spans
  bq.add_blocks(0, 100, uuid1(), na);
  bq.add_blocks(50, 100, uuid2(), na);
  // Covered until 150
  ASSERT_EQ(bq.get_next_needed_height(0), 150u);
}

TEST(block_queue, remove_span_filled_span)
{
  // Test remove_span on a filled span
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(10), uuid1(), na, 1024.0f, 1000);
  ASSERT_EQ(bq.get_max_block_height(), 9u);

  bool removed = bq.remove_span(0);
  ASSERT_TRUE(removed);
  ASSERT_EQ(bq.get_max_block_height(), 0u);
}

TEST(block_queue, set_span_hashes_replaces_existing)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 2, uuid1(), na);

  // Set initial hashes
  bq.set_span_hashes(0, uuid1(), {make_hash(1), make_hash(2)});
  ASSERT_TRUE(bq.requested(make_hash(1)));
  ASSERT_TRUE(bq.requested(make_hash(2)));

  // Set new hashes (replaces old ones via erase + reinsert)
  bq.set_span_hashes(0, uuid1(), {make_hash(3), make_hash(4)});
  ASSERT_TRUE(bq.requested(make_hash(3)));
  ASSERT_TRUE(bq.requested(make_hash(4)));
  // Old hashes should be removed (erase_block cleans them up)
  ASSERT_FALSE(bq.requested(make_hash(1)));
  ASSERT_FALSE(bq.requested(make_hash(2)));
}

TEST(block_queue, get_overview_multiple_filled_and_scheduled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(10), uuid1(), na, 1024.0f, 1000);
  bq.add_blocks(10, 10, uuid2(), na);
  bq.add_blocks(20, make_bcel(10), uuid3(), na, 1024.0f, 1000);

  std::string overview = bq.get_overview(0);
  // Should contain "m" (filled at blockchain height), "." (scheduled), "o" (filled not at bh)
  ASSERT_NE(overview.find('m'), std::string::npos);
  ASSERT_NE(overview.find('.'), std::string::npos);
  ASSERT_NE(overview.find('o'), std::string::npos);
}

TEST(block_queue, has_next_span_uuid_returns_time)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  boost::posix_time::ptime t = boost::posix_time::from_time_t(12345);
  bq.add_blocks(0, 10, uuid1(), na, t);

  bool filled;
  boost::posix_time::ptime out_time;
  ASSERT_TRUE(bq.has_next_span(uuid1(), filled, out_time));
  ASSERT_EQ(out_time, t);
}

TEST(block_queue, has_next_span_height_returns_time)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  boost::posix_time::ptime t = boost::posix_time::from_time_t(54321);
  bq.add_blocks(0, 10, uuid1(), na, t);

  bool filled;
  boost::posix_time::ptime out_time;
  boost::uuids::uuid conn_id;
  ASSERT_TRUE(bq.has_next_span(0, filled, out_time, conn_id));
  ASSERT_EQ(out_time, t);
  ASSERT_EQ(conn_id, uuid1());
}

TEST(block_queue, remove_spans_all_below)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  bq.add_blocks(10, 10, uuid1(), na);
  bq.add_blocks(20, 10, uuid1(), na);

  // Remove all spans at or below height 25 for uuid1
  bq.remove_spans(uuid1(), 25);
  // Spans at 0, 10, 20 are all at or below 25
  ASSERT_EQ(bq.get_max_block_height(), 0u);
  ASSERT_FALSE(bq.has_spans(uuid1()));
}

TEST(block_queue, get_num_filled_spans_single_filled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);
  ASSERT_EQ(bq.get_num_filled_spans(), 1u);
}

TEST(block_queue, get_data_size_after_remove)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid2(), na, 1024.0f, 300);
  ASSERT_EQ(bq.get_data_size(), 800u);

  bq.remove_span(0);
  ASSERT_EQ(bq.get_data_size(), 300u);
}

TEST(block_queue, get_next_needed_height_partial_overlap)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  // blockchain_height = 50, span starts at 40 and covers up to 59
  bq.add_blocks(40, 20, uuid1(), na);
  // Next needed is 60 (blockchain_height=50, span covers 40-59, max(50, 40+20)=60)
  ASSERT_EQ(bq.get_next_needed_height(50), 60u);
}

TEST(block_queue, print_filled_and_scheduled)
{
  // Just verify print doesn't crash with mixed span types
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(3), uuid1(), na, 51200.0f, 1000);
  bq.add_blocks(3, 5, uuid2(), na);

  // Should not throw or crash
  bq.print();
}

TEST(block_queue, flush_stale_multiple_stale)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  boost::uuids::uuid u1 = crypto::rand<boost::uuids::uuid>();
  boost::uuids::uuid u2 = crypto::rand<boost::uuids::uuid>();
  boost::uuids::uuid u3 = crypto::rand<boost::uuids::uuid>();
  boost::uuids::uuid u4 = crypto::rand<boost::uuids::uuid>();

  bq.add_blocks(0, 10, u1, na);
  bq.add_blocks(10, 10, u2, na);
  bq.add_blocks(20, 10, u3, na);
  bq.add_blocks(30, 10, u4, na);

  // Only u2 is live
  std::set<boost::uuids::uuid> live;
  live.insert(u2);
  bq.flush_stale_spans(live);

  ASSERT_FALSE(bq.has_spans(u1));
  ASSERT_TRUE(bq.has_spans(u2));
  ASSERT_FALSE(bq.has_spans(u3));
  ASSERT_FALSE(bq.has_spans(u4));
  ASSERT_EQ(bq.get_max_block_height(), 19u);
}

TEST(block_queue, reserve_span_sets_hashes_and_requested)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // 5 hashes covering heights 5..9 (last_block_height=9, so 5 <= 9 passes guard)
  std::vector<std::pair<crypto::hash, uint64_t>> block_hashes;
  for (uint64_t i = 0; i < 5; ++i)
    block_hashes.push_back(std::make_pair(make_hash(static_cast<uint8_t>(i + 220)), 5 + i));

  // span_start_height = 9 - 5 + 1 = 5
  auto result = bq.reserve_span(5, 9, 5, uuid1(), na, false, 0, 0, 100, block_hashes);
  ASSERT_EQ(result.second, 5u);

  // The hashes should be in the requested set
  ASSERT_TRUE(bq.requested(make_hash(220)));
  ASSERT_TRUE(bq.requested(make_hash(221)));
  ASSERT_TRUE(bq.requested(make_hash(224)));
}

TEST(block_queue, get_last_known_hash_picks_highest)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  // Add two spans for same uuid, set hashes matching nblocks
  bq.add_blocks(0, 2, uuid1(), na);
  bq.set_span_hashes(0, uuid1(), {make_hash(10), make_hash(11)});

  bq.add_blocks(100, 2, uuid1(), na);
  bq.set_span_hashes(100, uuid1(), {make_hash(20), make_hash(21)});

  // Should return last hash of highest span (height 101)
  crypto::hash h = bq.get_last_known_hash(uuid1());
  ASSERT_EQ(h, make_hash(21));
}

TEST(block_queue, add_blocks_with_time)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  boost::posix_time::ptime t = boost::posix_time::from_time_t(99999);
  bq.add_blocks(0, 10, uuid1(), na, t);

  // Verify via get_next_span_if_scheduled
  std::vector<crypto::hash> hashes;
  boost::uuids::uuid conn_id;
  boost::posix_time::ptime out_time;
  auto result = bq.get_next_span_if_scheduled(hashes, conn_id, out_time);
  ASSERT_EQ(result.first, 0u);
  ASSERT_EQ(result.second, 10u);
  ASSERT_EQ(out_time, t);
}

// ============================================================================
// Additional coverage tests for block_queue
// ============================================================================

// --- get_max_block_height with filled spans ---

TEST(block_queue, get_max_block_height_mixed_filled_and_scheduled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(10), uuid1(), na, 1024.0f, 1000);
  bq.add_blocks(10, 20, uuid2(), na); // scheduled, 10..29
  bq.add_blocks(30, make_bcel(5), uuid3(), na, 512.0f, 500);

  ASSERT_EQ(bq.get_max_block_height(), 34u);
}

// --- get_next_needed_height with multiple gaps ---

TEST(block_queue, get_next_needed_height_multiple_gaps)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);   // 0..9
  bq.add_blocks(20, 10, uuid2(), na);  // 20..29
  bq.add_blocks(40, 10, uuid3(), na);  // 40..49

  // blockchain_height=0: first gap at 10
  ASSERT_EQ(bq.get_next_needed_height(0), 10u);
}

TEST(block_queue, get_next_needed_height_blockchain_in_middle_of_span)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 100, uuid1(), na);   // 0..99
  bq.add_blocks(100, 100, uuid2(), na); // 100..199

  // blockchain at 50: still covered by spans
  ASSERT_EQ(bq.get_next_needed_height(50), 200u);
}

// --- add_blocks with BCEL replaces and preserves hashes ---

TEST(block_queue, add_blocks_bcel_preserves_hashes_from_prior_scheduled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Set up a scheduled span with hashes
  bq.add_blocks(0, 3, uuid1(), na);
  std::vector<crypto::hash> hashes = {make_hash(200), make_hash(201), make_hash(202)};
  bq.set_span_hashes(0, uuid1(), hashes);

  // Now replace with filled blocks - hashes should be preserved
  bq.add_blocks(0, make_bcel(3), uuid1(), na, 1024.0f, 300);

  // Hashes should still be in requested_hashes AND have_blocks
  ASSERT_TRUE(bq.requested(make_hash(200)));
  ASSERT_TRUE(bq.have(make_hash(200)));
  ASSERT_EQ(bq.have_height(make_hash(200)), 0u);
  ASSERT_EQ(bq.have_height(make_hash(201)), 1u);
  ASSERT_EQ(bq.have_height(make_hash(202)), 2u);
}

// --- flush_spans edge cases ---

TEST(block_queue, flush_spans_all_false_with_no_scheduled)
{
  // If there are only filled spans for a UUID, flush with all=false removes nothing
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(10), uuid1(), na, 1024.0f, 1000);

  bq.flush_spans(uuid1(), false); // should not remove anything
  ASSERT_EQ(bq.get_max_block_height(), 9u);
  ASSERT_TRUE(bq.has_spans(uuid1()));
}

// --- remove_spans edge cases ---

TEST(block_queue, remove_spans_at_exact_height)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(100, 50, uuid1(), na);
  bq.add_blocks(150, 50, uuid1(), na);

  // Remove spans at or below 100 (only span starting at 100)
  bq.remove_spans(uuid1(), 100);
  ASSERT_EQ(bq.get_max_block_height(), 199u);

  // Verify span at 100 is gone, span at 150 remains
  bool found100 = false, found150 = false;
  bq.foreach([&](const cryptonote::block_queue::span &s) -> bool {
    if (s.start_block_height == 100) found100 = true;
    if (s.start_block_height == 150) found150 = true;
    return true;
  });
  ASSERT_FALSE(found100);
  ASSERT_TRUE(found150);
}

// --- get_overview edge cases ---

TEST(block_queue, get_overview_span_at_blockchain_height)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Filled span starting exactly at blockchain height = "m"
  bq.add_blocks(50, make_bcel(10), uuid1(), na, 1024.0f, 1000);
  std::string overview = bq.get_overview(50);
  ASSERT_NE(overview.find('m'), std::string::npos);
}

TEST(block_queue, get_overview_span_below_blockchain_height_shows_lt)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  // Span at height 0 but blockchain height is 100
  bq.add_blocks(0, 10, uuid1(), na);
  std::string overview = bq.get_overview(100);
  // Span is below blockchain height, should show "<"
  ASSERT_NE(overview.find('<'), std::string::npos);
}

// --- get_speed with averaging ---

TEST(block_queue, get_speed_averaging_across_multiple_spans)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // uuid1: rates 1000, 3000 -> pseudo-avg = (1000+3000)/2 = 2000
  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1000.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid1(), na, 3000.0f, 500);

  // uuid2: rate 1000
  bq.add_blocks(10, make_bcel(5), uuid2(), na, 1000.0f, 500);

  // best_rate = 2000 (uuid1)
  // uuid1 speed = 2000/2000 = 1.0
  ASSERT_FLOAT_EQ(bq.get_speed(uuid1()), 1.0f);
  // uuid2 speed = 1000/2000 = 0.5
  ASSERT_FLOAT_EQ(bq.get_speed(uuid2()), 0.5f);
}

// --- get_download_rate averaging ---

TEST(block_queue, get_download_rate_averages_multiple_spans)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Three spans for uuid1: 1000, 3000, 2000
  // Pseudo-avg: first=1000, then (1000+3000)/2=2000, then (2000+2000)/2=2000
  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1000.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid1(), na, 3000.0f, 500);
  bq.add_blocks(10, make_bcel(5), uuid1(), na, 2000.0f, 500);

  ASSERT_FLOAT_EQ(bq.get_download_rate(uuid1()), 2000.0f);
}

// --- foreach with mixed span types ---

TEST(block_queue, foreach_mixed_filled_scheduled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);
  bq.add_blocks(5, 10, uuid2(), na);
  bq.add_blocks(15, make_bcel(3), uuid3(), na, 512.0f, 300);

  int filled_count = 0;
  int scheduled_count = 0;
  bq.foreach([&](const cryptonote::block_queue::span &s) -> bool {
    if (s.blocks.empty())
      ++scheduled_count;
    else
      ++filled_count;
    return true;
  });
  ASSERT_EQ(filled_count, 2);
  ASSERT_EQ(scheduled_count, 1);
}

// --- set_span_hashes overwrites previous hashes ---

TEST(block_queue, set_span_hashes_overwrites_and_cleans_old)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 2, uuid1(), na);

  // First set of hashes
  bq.set_span_hashes(0, uuid1(), {make_hash(210), make_hash(211)});
  ASSERT_TRUE(bq.requested(make_hash(210)));
  ASSERT_TRUE(bq.requested(make_hash(211)));

  // Overwrite with new hashes
  bq.set_span_hashes(0, uuid1(), {make_hash(220), make_hash(221)});
  // Old hashes should be cleaned (erase_block removes them)
  ASSERT_FALSE(bq.requested(make_hash(210)));
  ASSERT_FALSE(bq.requested(make_hash(211)));
  // New hashes should be present
  ASSERT_TRUE(bq.requested(make_hash(220)));
  ASSERT_TRUE(bq.requested(make_hash(221)));
}

// --- get_data_size after adding and removing ---

TEST(block_queue, get_data_size_tracks_additions_and_removals)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);
  ASSERT_EQ(bq.get_data_size(), 500u);

  bq.add_blocks(5, make_bcel(3), uuid2(), na, 512.0f, 300);
  ASSERT_EQ(bq.get_data_size(), 800u);

  // Remove uuid1's span
  bq.flush_spans(uuid1(), true);
  ASSERT_EQ(bq.get_data_size(), 300u);

  // Remove uuid2's span
  bq.flush_spans(uuid2(), true);
  ASSERT_EQ(bq.get_data_size(), 0u);
}

// --- get_num_filled_spans with removals ---

TEST(block_queue, get_num_filled_spans_after_partial_flush)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid2(), na, 1024.0f, 500);
  bq.add_blocks(10, make_bcel(5), uuid3(), na, 1024.0f, 500);
  ASSERT_EQ(bq.get_num_filled_spans(), 3u);

  bq.flush_spans(uuid2(), true);
  ASSERT_EQ(bq.get_num_filled_spans(), 2u);
}

// --- has_next_span height overload with filled span ---

TEST(block_queue, has_next_span_height_with_filled_first)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);

  bool filled;
  boost::posix_time::ptime time;
  boost::uuids::uuid conn_id;

  ASSERT_TRUE(bq.has_next_span(0, filled, time, conn_id));
  ASSERT_TRUE(filled);
  ASSERT_EQ(conn_id, uuid1());
}

// --- get_next_span with multiple filled spans ---

TEST(block_queue, get_next_span_returns_lowest_filled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(100, make_bcel(5), uuid2(), na, 512.0f, 500);
  bq.add_blocks(0, make_bcel(5), uuid1(), na, 1024.0f, 500);

  uint64_t height;
  std::vector<cryptonote::block_complete_entry> bcel;
  boost::uuids::uuid conn_id;
  epee::net_utils::network_address addr;

  ASSERT_TRUE(bq.get_next_span(height, bcel, conn_id, addr, true));
  // Should return the one at height 0 (lowest)
  ASSERT_EQ(height, 0u);
  ASSERT_EQ(bcel.size(), 5u);
  ASSERT_EQ(conn_id, uuid1());
}

// --- reserve_span with partial overlap ---

TEST(block_queue, reserve_span_partial_already_requested)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // 10 hashes covering heights 10..19
  std::vector<std::pair<crypto::hash, uint64_t>> block_hashes;
  for (uint64_t i = 0; i < 10; ++i)
    block_hashes.push_back(std::make_pair(make_hash(static_cast<uint8_t>(i)), 10 + i));

  // Reserve first 3
  auto r1 = bq.reserve_span(10, 19, 3, uuid1(), na, false, 0, 0, 100, block_hashes);
  ASSERT_EQ(r1.first, 10u);
  ASSERT_EQ(r1.second, 3u);

  // Reserve next batch - should skip the first 3
  auto r2 = bq.reserve_span(10, 19, 3, uuid2(), na, false, 0, 0, 100, block_hashes);
  ASSERT_EQ(r2.first, 13u);
  ASSERT_EQ(r2.second, 3u);

  // Reserve remaining - should skip first 6
  auto r3 = bq.reserve_span(10, 19, 10, uuid3(), na, false, 0, 0, 100, block_hashes);
  ASSERT_EQ(r3.first, 16u);
  ASSERT_EQ(r3.second, 4u); // 4 remaining (16,17,18,19)
}

// ============================================================================
// Additional tests for improved coverage
// ============================================================================

// --- get_data_size comprehensive tests ---

TEST(block_queue, get_data_size_multiple_filled_spans)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 512.0f, 500);
  ASSERT_EQ(bq.get_data_size(), 500u);

  bq.add_blocks(5, make_bcel(5), uuid2(), na, 512.0f, 700);
  ASSERT_EQ(bq.get_data_size(), 1200u);

  bq.add_blocks(10, make_bcel(3), uuid1(), na, 256.0f, 300);
  ASSERT_EQ(bq.get_data_size(), 1500u);
}

TEST(block_queue, get_data_size_mixed_scheduled_and_filled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Scheduled spans have size 0
  bq.add_blocks(0, 10, uuid1(), na);
  ASSERT_EQ(bq.get_data_size(), 0u);

  // Add a filled span
  bq.add_blocks(10, make_bcel(5), uuid2(), na, 512.0f, 800);
  ASSERT_EQ(bq.get_data_size(), 800u);

  // Another scheduled
  bq.add_blocks(15, 10, uuid1(), na);
  ASSERT_EQ(bq.get_data_size(), 800u);
}

TEST(block_queue, get_data_size_after_flush)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 512.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid2(), na, 512.0f, 700);
  ASSERT_EQ(bq.get_data_size(), 1200u);

  // Flush uuid1's filled span
  bq.flush_spans(uuid1(), true);
  ASSERT_EQ(bq.get_data_size(), 700u);

  // Flush uuid2's filled span
  bq.flush_spans(uuid2(), true);
  ASSERT_EQ(bq.get_data_size(), 0u);
}

// --- foreach with various patterns ---

TEST(block_queue, foreach_modifies_external_state)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, 5, uuid1(), na);
  bq.add_blocks(5, make_bcel(3), uuid2(), na, 256.0f, 300);
  bq.add_blocks(8, 4, uuid1(), na);

  // Accumulate start heights
  std::vector<uint64_t> start_heights;
  bq.foreach([&start_heights](const cryptonote::block_queue::span &s) -> bool {
    start_heights.push_back(s.start_block_height);
    return true;
  });

  ASSERT_EQ(start_heights.size(), 3u);
  // Spans are sorted by start_block_height
  ASSERT_EQ(start_heights[0], 0u);
  ASSERT_EQ(start_heights[1], 5u);
  ASSERT_EQ(start_heights[2], 8u);
}

TEST(block_queue, foreach_accumulate_total_nblocks)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  bq.add_blocks(10, 20, uuid2(), na);
  bq.add_blocks(30, 5, uuid3(), na);

  uint64_t total_nblocks = 0;
  bq.foreach([&total_nblocks](const cryptonote::block_queue::span &s) -> bool {
    total_nblocks += s.nblocks;
    return true;
  });
  ASSERT_EQ(total_nblocks, 35u);
}

TEST(block_queue, foreach_check_span_connection_ids)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  bq.add_blocks(10, 10, uuid2(), na);

  std::set<boost::uuids::uuid> seen_uuids;
  bq.foreach([&seen_uuids](const cryptonote::block_queue::span &s) -> bool {
    seen_uuids.insert(s.connection_id);
    return true;
  });
  ASSERT_EQ(seen_uuids.size(), 2u);
  ASSERT_TRUE(seen_uuids.count(uuid1()) > 0);
  ASSERT_TRUE(seen_uuids.count(uuid2()) > 0);
}

// --- requested() and have() deeper tests ---

TEST(block_queue, requested_after_reserve_span)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // block_hashes must have size <= last_block_height, so use heights 10-14
  std::vector<std::pair<crypto::hash, uint64_t>> block_hashes;
  for (uint64_t i = 0; i < 5; ++i)
    block_hashes.push_back(std::make_pair(make_hash(static_cast<uint8_t>(i + 10)), 10 + i));

  auto r = bq.reserve_span(10, 14, 5, uuid1(), na, false, 0, 0, 100, block_hashes);
  ASSERT_EQ(r.first, 10u);
  ASSERT_EQ(r.second, 5u);

  // All reserved hashes should be in the requested set
  for (uint64_t i = 0; i < 5; ++i)
    ASSERT_TRUE(bq.requested(make_hash(static_cast<uint8_t>(i + 10))));

  // A hash not reserved should not be requested
  ASSERT_FALSE(bq.requested(make_hash(0xFF)));
}

TEST(block_queue, have_after_adding_filled_blocks)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Add a scheduled span with hashes, then fill it
  bq.add_blocks(0, 3, uuid1(), na);
  std::vector<crypto::hash> hashes = {make_hash(10), make_hash(11), make_hash(12)};
  bq.set_span_hashes(0, uuid1(), hashes);

  // Now add filled blocks
  bq.add_blocks(0, make_bcel(3), uuid1(), na, 100.0f, 300);

  // The filled blocks should be in the have set
  ASSERT_TRUE(bq.have(make_hash(10)));
  ASSERT_TRUE(bq.have(make_hash(11)));
  ASSERT_TRUE(bq.have(make_hash(12)));
  ASSERT_FALSE(bq.have(make_hash(99)));
}

TEST(block_queue, have_height_after_filling)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(100, 3, uuid1(), na);
  std::vector<crypto::hash> hashes = {make_hash(20), make_hash(21), make_hash(22)};
  bq.set_span_hashes(100, uuid1(), hashes);

  bq.add_blocks(100, make_bcel(3), uuid1(), na, 100.0f, 300);

  // Check have_height returns the correct block height
  ASSERT_EQ(bq.have_height(make_hash(20)), 100u);
  ASSERT_EQ(bq.have_height(make_hash(21)), 101u);
  ASSERT_EQ(bq.have_height(make_hash(22)), 102u);
  // have_height returns UINT64_MAX for not-found hashes
  ASSERT_EQ(bq.have_height(make_hash(99)), std::numeric_limits<uint64_t>::max());
}

// --- get_num_filled_spans and prefix tests ---

TEST(block_queue, get_num_filled_spans_multiple_filled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 100.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid2(), na, 100.0f, 500);
  bq.add_blocks(10, make_bcel(5), uuid3(), na, 100.0f, 500);

  ASSERT_EQ(bq.get_num_filled_spans(), 3u);
  ASSERT_EQ(bq.get_num_filled_spans_prefix(), 3u);
}

TEST(block_queue, get_num_filled_spans_prefix_gap_in_middle)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 100.0f, 500);
  bq.add_blocks(5, 5, uuid2(), na);  // scheduled (gap)
  bq.add_blocks(10, make_bcel(5), uuid3(), na, 100.0f, 500);

  ASSERT_EQ(bq.get_num_filled_spans(), 2u);
  // prefix stops at first non-filled
  ASSERT_EQ(bq.get_num_filled_spans_prefix(), 1u);
}

TEST(block_queue, get_num_filled_spans_after_remove)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 100.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid2(), na, 100.0f, 500);
  ASSERT_EQ(bq.get_num_filled_spans(), 2u);

  bq.remove_span(0);
  ASSERT_EQ(bq.get_num_filled_spans(), 1u);
}

// --- get_speed and get_download_rate ---

TEST(block_queue, get_speed_after_remove)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(10), uuid1(), na, 500.0f, 5000);
  float speed = bq.get_speed(uuid1());
  ASSERT_GT(speed, 0.0f);

  bq.flush_spans(uuid1(), true);
  float speed_after = bq.get_speed(uuid1());
  // get_speed returns 1.0f (assumed good speed) when connection is not found
  ASSERT_EQ(speed_after, 1.0f);
}

TEST(block_queue, get_download_rate_after_multiple_adds)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 100.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid1(), na, 200.0f, 1000);
  bq.add_blocks(10, make_bcel(5), uuid1(), na, 300.0f, 1500);

  float rate = bq.get_download_rate(uuid1());
  ASSERT_GT(rate, 0.0f);
}

TEST(block_queue, get_speed_and_rate_consistent)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(10), uuid1(), na, 1024.0f, 10240);
  bq.add_blocks(10, make_bcel(10), uuid2(), na, 512.0f, 5120);

  // Both should report non-zero for their connections
  ASSERT_GT(bq.get_speed(uuid1()), 0.0f);
  ASSERT_GT(bq.get_speed(uuid2()), 0.0f);
  ASSERT_GT(bq.get_download_rate(uuid1()), 0.0f);
  ASSERT_GT(bq.get_download_rate(uuid2()), 0.0f);
}

// --- flush_stale_spans edge cases ---

TEST(block_queue, flush_stale_with_mixed_filled_and_scheduled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // uuid1: filled span
  bq.add_blocks(0, make_bcel(5), uuid1(), na, 100.0f, 500);
  // uuid1: scheduled span
  bq.add_blocks(5, 5, uuid1(), na);
  // uuid2: scheduled span
  bq.add_blocks(10, 5, uuid2(), na);

  // Only uuid1 is live
  std::set<boost::uuids::uuid> live;
  live.insert(uuid1());

  bq.flush_stale_spans(live);

  // uuid1's filled span remains; uuid1's scheduled span remains (it's live);
  // uuid2's scheduled span is removed
  ASSERT_TRUE(bq.has_spans(uuid1()));
  ASSERT_FALSE(bq.has_spans(uuid2()));
  ASSERT_EQ(bq.get_max_block_height(), 9u); // uuid1's spans go up to 9
}

TEST(block_queue, flush_stale_all_filled_none_live)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 100.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid2(), na, 100.0f, 500);

  // No live connections
  std::set<boost::uuids::uuid> live;
  bq.flush_stale_spans(live);

  // Filled spans should remain (stale flush only removes scheduled/empty)
  ASSERT_TRUE(bq.has_spans(uuid1()));
  ASSERT_TRUE(bq.has_spans(uuid2()));
}

// --- set_span_hashes edge cases ---

TEST(block_queue, set_span_hashes_empty_hashes_vector)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 5, uuid1(), na);
  // Set an empty hashes vector - should not crash
  std::vector<crypto::hash> empty_hashes;
  bq.set_span_hashes(0, uuid1(), empty_hashes);
}

TEST(block_queue, set_span_hashes_updates_requested)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 3, uuid1(), na);

  std::vector<crypto::hash> hashes = {make_hash(50), make_hash(51), make_hash(52)};
  bq.set_span_hashes(0, uuid1(), hashes);

  // All hashes should now be in the requested set
  ASSERT_TRUE(bq.requested(make_hash(50)));
  ASSERT_TRUE(bq.requested(make_hash(51)));
  ASSERT_TRUE(bq.requested(make_hash(52)));
}

// --- get_overview tests with various patterns ---

TEST(block_queue, get_overview_empty_returns_brackets)
{
  cryptonote::block_queue bq;
  std::string overview = bq.get_overview(0);
  // Empty queue returns "[]"
  ASSERT_EQ(overview, "[]");
}

TEST(block_queue, get_overview_multiple_scheduled_spans)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  bq.add_blocks(10, 10, uuid2(), na);
  bq.add_blocks(20, 10, uuid3(), na);

  std::string overview = bq.get_overview(0);
  ASSERT_FALSE(overview.empty());
}

TEST(block_queue, get_overview_mixed_types)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Filled at blockchain height
  bq.add_blocks(0, make_bcel(5), uuid1(), na, 100.0f, 500);
  // Scheduled after
  bq.add_blocks(5, 5, uuid2(), na);
  // Filled not at blockchain height
  bq.add_blocks(10, make_bcel(3), uuid3(), na, 100.0f, 300);

  std::string overview = bq.get_overview(0);
  ASSERT_FALSE(overview.empty());
}

// --- get_next_span_if_scheduled ---

TEST(block_queue, get_next_span_if_scheduled_multiple_scheduled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 5, uuid1(), na);
  bq.add_blocks(5, 5, uuid2(), na);

  std::vector<crypto::hash> hashes;
  boost::uuids::uuid conn_id;
  boost::posix_time::ptime time;

  auto result = bq.get_next_span_if_scheduled(hashes, conn_id, time);
  ASSERT_EQ(result.first, 0u);
  ASSERT_EQ(result.second, 5u);
  ASSERT_EQ(conn_id, uuid1());
}

TEST(block_queue, get_next_span_if_scheduled_first_filled_second_scheduled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // First span is filled
  bq.add_blocks(0, make_bcel(5), uuid1(), na, 100.0f, 500);
  // Second span is scheduled
  bq.add_blocks(5, 5, uuid2(), na);

  std::vector<crypto::hash> hashes;
  boost::uuids::uuid conn_id;
  boost::posix_time::ptime time;

  // Should return the scheduled span (second one)
  auto result = bq.get_next_span_if_scheduled(hashes, conn_id, time);
  // First span is filled so it's skipped, get_next_span_if_scheduled only returns
  // if the FIRST span is scheduled. If first is filled, it returns 0,0.
  ASSERT_EQ(result.second, 0u);
}

// --- reset_next_span_time ---

TEST(block_queue, reset_next_span_time_scheduled_span)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 5, uuid1(), na);

  // reset_next_span_time sets the time on the first span
  boost::posix_time::ptime new_time = boost::posix_time::microsec_clock::universal_time();
  ASSERT_NO_THROW(bq.reset_next_span_time(new_time));
}

TEST(block_queue, reset_next_span_time_multiple_spans)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 5, uuid1(), na);
  bq.add_blocks(5, 5, uuid2(), na);

  // Reset should affect the first span only
  boost::posix_time::ptime new_time = boost::posix_time::microsec_clock::universal_time();
  ASSERT_NO_THROW(bq.reset_next_span_time(new_time));
}

// --- get_last_known_hash ---

TEST(block_queue, get_last_known_hash_multiple_spans_same_uuid)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 3, uuid1(), na);
  bq.add_blocks(3, 3, uuid1(), na);

  std::vector<crypto::hash> hashes1 = {make_hash(1), make_hash(2), make_hash(3)};
  bq.set_span_hashes(0, uuid1(), hashes1);

  std::vector<crypto::hash> hashes2 = {make_hash(4), make_hash(5), make_hash(6)};
  bq.set_span_hashes(3, uuid1(), hashes2);

  crypto::hash last = bq.get_last_known_hash(uuid1());
  // Should return the last hash from the highest span
  ASSERT_EQ(last, make_hash(6));
}

TEST(block_queue, get_last_known_hash_no_hashes_returns_null)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 5, uuid1(), na);
  // No hashes set
  crypto::hash last = bq.get_last_known_hash(uuid1());
  ASSERT_EQ(last, crypto::null_hash);
}

// --- has_spans ---

TEST(block_queue, has_spans_multiple_uuids)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  bq.add_blocks(10, 10, uuid2(), na);

  ASSERT_TRUE(bq.has_spans(uuid1()));
  ASSERT_TRUE(bq.has_spans(uuid2()));
  ASSERT_FALSE(bq.has_spans(uuid3()));

  bq.flush_spans(uuid1());
  ASSERT_FALSE(bq.has_spans(uuid1()));
  ASSERT_TRUE(bq.has_spans(uuid2()));
}

// --- remove_span with filled span ---

TEST(block_queue, remove_filled_span_reduces_data_size)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 100.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid2(), na, 100.0f, 700);
  ASSERT_EQ(bq.get_data_size(), 1200u);

  bq.remove_span(0);
  ASSERT_EQ(bq.get_data_size(), 700u);

  bq.remove_span(5);
  ASSERT_EQ(bq.get_data_size(), 0u);
}

// --- remove_spans with filled spans ---

TEST(block_queue, remove_spans_filled_by_connection)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 100.0f, 500);
  bq.add_blocks(5, make_bcel(5), uuid1(), na, 100.0f, 500);
  bq.add_blocks(10, make_bcel(5), uuid2(), na, 100.0f, 500);

  // Remove uuid1's spans at height <= 5
  bq.remove_spans(uuid1(), 5);

  // uuid1's span at 0 should be removed (start_block_height 0 < 5)
  // uuid1's span at 5 should be removed (start_block_height 5 <= 5)
  // uuid2's span at 10 remains
  ASSERT_TRUE(bq.has_spans(uuid2()));
}

// --- get_next_needed_height with filled spans ---

TEST(block_queue, get_next_needed_height_filled_spans)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Filled span at 0-4
  bq.add_blocks(0, make_bcel(5), uuid1(), na, 100.0f, 500);

  // Blockchain is at height 0 (the spans start there)
  uint64_t next = bq.get_next_needed_height(0);
  // Should need height 5 (after the filled span)
  ASSERT_EQ(next, 5u);
}

TEST(block_queue, get_next_needed_height_gap_in_filled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 100.0f, 500);
  // Gap: nothing at 5-9
  bq.add_blocks(10, make_bcel(5), uuid2(), na, 100.0f, 500);

  uint64_t next = bq.get_next_needed_height(0);
  // Should need 5 (the gap)
  ASSERT_EQ(next, 5u);
}

// --- get_next_span edge cases ---

TEST(block_queue, get_next_span_unfilled_false)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  // Only scheduled spans
  bq.add_blocks(0, 10, uuid1(), na);

  uint64_t height;
  std::vector<cryptonote::block_complete_entry> bcel;
  boost::uuids::uuid conn_id;
  epee::net_utils::network_address addr;

  // filled=true: should not find any
  ASSERT_FALSE(bq.get_next_span(height, bcel, conn_id, addr, true));

  // filled=false: should find the scheduled span
  ASSERT_TRUE(bq.get_next_span(height, bcel, conn_id, addr, false));
  ASSERT_EQ(height, 0u);
}

TEST(block_queue, get_next_span_multiple_filled_returns_lowest)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Add filled spans out of order
  bq.add_blocks(20, make_bcel(5), uuid3(), na, 100.0f, 500);
  bq.add_blocks(10, make_bcel(5), uuid2(), na, 100.0f, 500);
  bq.add_blocks(0, make_bcel(5), uuid1(), na, 100.0f, 500);

  uint64_t height;
  std::vector<cryptonote::block_complete_entry> bcel;
  boost::uuids::uuid conn_id;
  epee::net_utils::network_address addr;

  ASSERT_TRUE(bq.get_next_span(height, bcel, conn_id, addr, true));
  ASSERT_EQ(height, 0u);
  ASSERT_EQ(conn_id, uuid1());
}

// --- has_next_span (uuid overload) ---

TEST(block_queue, has_next_span_uuid_multiple_connections)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 5, uuid1(), na);
  bq.add_blocks(5, 5, uuid2(), na);

  bool filled;
  boost::posix_time::ptime time;

  // uuid1 has the next span
  ASSERT_TRUE(bq.has_next_span(uuid1(), filled, time));
  ASSERT_FALSE(filled); // scheduled

  // uuid2 does not have the NEXT span (uuid1 has it)
  ASSERT_FALSE(bq.has_next_span(uuid2(), filled, time));
}

// --- has_next_span (height overload) ---

TEST(block_queue, has_next_span_height_exact_match)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(100, 10, uuid1(), na);

  bool filled;
  boost::posix_time::ptime time;
  boost::uuids::uuid conn_id;

  ASSERT_TRUE(bq.has_next_span(100, filled, time, conn_id));
  ASSERT_FALSE(filled);
  ASSERT_EQ(conn_id, uuid1());
}

TEST(block_queue, has_next_span_height_no_match)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(100, 10, uuid1(), na);

  bool filled;
  boost::posix_time::ptime time;
  boost::uuids::uuid conn_id;

  // Height 50 has no span
  ASSERT_FALSE(bq.has_next_span(50, filled, time, conn_id));
}

// --- reserve_span edge cases ---

TEST(block_queue, reserve_span_all_hashes_requested)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Use heights 10-14 with last_block_height=14 (block_hashes.size() <= last_block_height)
  std::vector<std::pair<crypto::hash, uint64_t>> block_hashes;
  for (uint64_t i = 0; i < 5; ++i)
    block_hashes.push_back(std::make_pair(make_hash(static_cast<uint8_t>(i)), 10 + i));

  // Reserve all hashes
  auto r1 = bq.reserve_span(10, 14, 10, uuid1(), na, false, 0, 0, 100, block_hashes);
  ASSERT_EQ(r1.first, 10u);
  ASSERT_EQ(r1.second, 5u);

  // Try to reserve again - all already requested
  auto r2 = bq.reserve_span(10, 14, 10, uuid2(), na, false, 0, 0, 100, block_hashes);
  ASSERT_EQ(r2.second, 0u); // Nothing to reserve
}

TEST(block_queue, reserve_span_single_hash)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  std::vector<std::pair<crypto::hash, uint64_t>> block_hashes;
  block_hashes.push_back(std::make_pair(make_hash(42), 100));

  auto r = bq.reserve_span(100, 100, 10, uuid1(), na, false, 0, 0, 200, block_hashes);
  ASSERT_EQ(r.first, 100u);
  ASSERT_EQ(r.second, 1u);
}

// --- Comprehensive workflow tests ---

TEST(block_queue, add_schedule_fill_take_workflow)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Step 1: Schedule spans
  bq.add_blocks(0, 10, uuid1(), na);
  bq.add_blocks(10, 10, uuid2(), na);
  ASSERT_EQ(bq.get_max_block_height(), 19u);
  ASSERT_EQ(bq.get_num_filled_spans(), 0u);

  // Step 2: Set hashes on first span
  std::vector<crypto::hash> hashes;
  for (int i = 0; i < 10; ++i)
    hashes.push_back(make_hash(static_cast<uint8_t>(i)));
  bq.set_span_hashes(0, uuid1(), hashes);

  // Step 3: Fill the first span
  bq.add_blocks(0, make_bcel(10), uuid1(), na, 500.0f, 5000);
  ASSERT_EQ(bq.get_num_filled_spans(), 1u);
  ASSERT_EQ(bq.get_num_filled_spans_prefix(), 1u);
  ASSERT_EQ(bq.get_data_size(), 5000u);

  // Step 4: Get next span (should be the filled one)
  uint64_t height;
  std::vector<cryptonote::block_complete_entry> bcel;
  boost::uuids::uuid conn_id;
  epee::net_utils::network_address addr;
  ASSERT_TRUE(bq.get_next_span(height, bcel, conn_id, addr, true));
  ASSERT_EQ(height, 0u);
  ASSERT_EQ(bcel.size(), 10u);

  // Step 5: Remove the processed span
  bq.remove_span(0);
  ASSERT_EQ(bq.get_num_filled_spans(), 0u);
  ASSERT_EQ(bq.get_data_size(), 0u);

  // Step 6: Verify only uuid2's scheduled span remains
  ASSERT_TRUE(bq.has_spans(uuid2()));
  ASSERT_FALSE(bq.has_spans(uuid1()));
}

TEST(block_queue, full_lifecycle_three_connections)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Three connections schedule spans
  bq.add_blocks(0, 10, uuid1(), na);
  bq.add_blocks(10, 10, uuid2(), na);
  bq.add_blocks(20, 10, uuid3(), na);

  ASSERT_EQ(bq.get_max_block_height(), 29u);
  ASSERT_EQ(bq.get_num_filled_spans(), 0u);

  // Fill spans from connections 1 and 3
  bq.add_blocks(0, make_bcel(10), uuid1(), na, 100.0f, 1000);
  bq.add_blocks(20, make_bcel(10), uuid3(), na, 300.0f, 3000);

  ASSERT_EQ(bq.get_num_filled_spans(), 2u);
  ASSERT_EQ(bq.get_num_filled_spans_prefix(), 1u); // only span at 0 is contiguous prefix
  ASSERT_EQ(bq.get_data_size(), 4000u);

  // Speed checks
  ASSERT_GT(bq.get_speed(uuid1()), 0.0f);
  // get_speed returns 1.0f for connections with no filled spans (assumed good speed)
  ASSERT_EQ(bq.get_speed(uuid2()), 1.0f); // scheduled only, default
  ASSERT_GT(bq.get_speed(uuid3()), 0.0f);

  // Process first span
  bq.remove_span(0);
  ASSERT_EQ(bq.get_num_filled_spans(), 1u);

  // Flush connection 2 (disconnected)
  std::set<boost::uuids::uuid> live;
  live.insert(uuid1());
  live.insert(uuid3());
  bq.flush_stale_spans(live);

  // uuid2's scheduled span at 10 should be gone
  ASSERT_FALSE(bq.has_spans(uuid2()));

  // uuid3's filled span at 20 should remain
  ASSERT_TRUE(bq.has_spans(uuid3()));
  ASSERT_EQ(bq.get_data_size(), 3000u);
}

// --- print with various states ---

TEST(block_queue, print_with_mixed_scheduled_filled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(5), uuid1(), na, 100.0f, 500);
  bq.add_blocks(5, 10, uuid2(), na);
  bq.add_blocks(15, make_bcel(3), uuid3(), na, 200.0f, 300);

  // Just verify print does not crash
  ASSERT_NO_THROW(bq.print());
}

// --- get_max_block_height edge cases ---

TEST(block_queue, get_max_block_height_single_block_filled)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(1), uuid1(), na, 100.0f, 100);
  ASSERT_EQ(bq.get_max_block_height(), 0u);
}

TEST(block_queue, get_max_block_height_after_all_removed)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  bq.add_blocks(10, 10, uuid2(), na);
  bq.remove_span(0);
  bq.remove_span(10);
  ASSERT_EQ(bq.get_max_block_height(), 0u);
}

// --- add_blocks with time parameter ---

TEST(block_queue, add_blocks_scheduled_with_explicit_time)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::universal_time();
  bq.add_blocks(0, 10, uuid1(), na, t1);
  ASSERT_EQ(bq.get_max_block_height(), 9u);

  bool filled;
  boost::posix_time::ptime time;
  boost::uuids::uuid conn_id;
  ASSERT_TRUE(bq.has_next_span(0, filled, time, conn_id));
  ASSERT_FALSE(filled);
  ASSERT_EQ(time, t1);
}

// --- get_next_needed_height with complex layouts ---

TEST(block_queue, get_next_needed_height_blockchain_past_all_spans)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 10, uuid1(), na);
  bq.add_blocks(10, 10, uuid2(), na);

  // Blockchain is at height 100, past all spans
  uint64_t next = bq.get_next_needed_height(100);
  ASSERT_GE(next, 100u);
}

TEST(block_queue, get_next_needed_height_single_block_span)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(0, 1, uuid1(), na);
  uint64_t next = bq.get_next_needed_height(0);
  ASSERT_EQ(next, 1u);
}

// --- Multiple operations on same height ---

TEST(block_queue, replace_scheduled_with_filled_at_same_height)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  // Schedule a span
  bq.add_blocks(0, 5, uuid1(), na);
  ASSERT_EQ(bq.get_num_filled_spans(), 0u);

  // Fill it
  bq.add_blocks(0, make_bcel(5), uuid1(), na, 100.0f, 500);
  ASSERT_EQ(bq.get_num_filled_spans(), 1u);
  ASSERT_EQ(bq.get_data_size(), 500u);
}

// --- foreach with filled span data ---

TEST(block_queue, foreach_inspect_filled_span_details)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na = make_addr(0x01020304, 18080);

  bq.add_blocks(0, make_bcel(7), uuid1(), na, 256.0f, 1792);

  bool found = false;
  bq.foreach([&found](const cryptonote::block_queue::span &s) -> bool {
    if (s.start_block_height == 0 && s.nblocks == 7)
    {
      found = true;
      // Verify the span has filled data
      EXPECT_EQ(s.blocks.size(), 7u);
      EXPECT_FLOAT_EQ(s.rate, 256.0f);
      EXPECT_EQ(s.size, 1792u);
    }
    return true;
  });
  ASSERT_TRUE(found);
}

TEST(block_queue, foreach_inspect_scheduled_span_details)
{
  cryptonote::block_queue bq;
  epee::net_utils::network_address na;

  bq.add_blocks(50, 15, uuid2(), na);

  bool found = false;
  bq.foreach([&found](const cryptonote::block_queue::span &s) -> bool {
    if (s.start_block_height == 50 && s.nblocks == 15)
    {
      found = true;
      // Scheduled spans have empty blocks
      EXPECT_TRUE(s.blocks.empty());
      EXPECT_EQ(s.rate, 0.0f);
      EXPECT_EQ(s.size, 0u);
    }
    return true;
  });
  ASSERT_TRUE(found);
}
