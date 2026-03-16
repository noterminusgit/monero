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

#include "gtest/gtest.h"

#include "net/network_throttle-detail.hpp"

TEST(network_throttle, construct)
{
  epee::net_utils::network_throttle throttle("tst", "test_throttle");
  throttle.set_target_speed(1024); // 1024 kB/s
}

TEST(network_throttle, set_target_speed)
{
  epee::net_utils::network_throttle throttle("tst", "test_speed");
  throttle.set_target_speed(0); // unlimited
  ASSERT_DOUBLE_EQ(throttle.get_target_speed(), 0.0);
  throttle.set_target_speed(1024);
  ASSERT_DOUBLE_EQ(throttle.get_target_speed(), 1024.0);
}

TEST(network_throttle, get_stats_initial)
{
  epee::net_utils::network_throttle throttle("tst", "test_stats");
  throttle.set_target_speed(1024);

  uint64_t total_packets = 0;
  uint64_t total_bytes = 0;
  throttle.get_stats(total_packets, total_bytes);
  ASSERT_EQ(total_packets, 0u);
  ASSERT_EQ(total_bytes, 0u);
}

TEST(network_throttle, handle_trafic_exact)
{
  epee::net_utils::network_throttle throttle("tst", "test_traffic");
  throttle.set_target_speed(1024 * 1024); // high enough not to throttle

  throttle.handle_trafic_exact(1000);

  uint64_t total_packets = 0;
  uint64_t total_bytes = 0;
  throttle.get_stats(total_packets, total_bytes);
  ASSERT_GT(total_bytes, 0u);
}

TEST(network_throttle, handle_trafic_tcp)
{
  epee::net_utils::network_throttle throttle("tst", "test_tcp");
  throttle.set_target_speed(1024 * 1024);

  throttle.handle_trafic_tcp(1000);

  uint64_t total_packets = 0;
  uint64_t total_bytes = 0;
  throttle.get_stats(total_packets, total_bytes);
  ASSERT_GT(total_bytes, 0u);
}

TEST(network_throttle, multiple_packets)
{
  epee::net_utils::network_throttle throttle("tst", "test_multi");
  throttle.set_target_speed(1024 * 1024);

  throttle.handle_trafic_exact(100);
  throttle.handle_trafic_exact(200);
  throttle.handle_trafic_exact(300);

  uint64_t total_packets = 0;
  uint64_t total_bytes = 0;
  throttle.get_stats(total_packets, total_bytes);
  ASSERT_GE(total_bytes, 600u);
}

TEST(network_throttle, get_sleep_time)
{
  epee::net_utils::network_throttle throttle("tst", "test_sleep");
  throttle.set_target_speed(1024 * 1024); // high speed

  double sleep_time = throttle.get_sleep_time(1000);
  // With high speed limit and no prior traffic, sleep should be minimal
  ASSERT_GE(sleep_time, 0.0);
}

TEST(network_throttle, get_sleep_time_after_tick)
{
  epee::net_utils::network_throttle throttle("tst", "test_tick");
  throttle.set_target_speed(1024 * 1024);

  double sleep_time = throttle.get_sleep_time_after_tick(1000);
  ASSERT_GE(sleep_time, 0.0);
}

TEST(network_throttle, get_time_seconds)
{
  epee::net_utils::network_throttle throttle("tst", "test_time");
  double t1 = throttle.get_time_seconds();
  ASSERT_GT(t1, 0.0);
  double t2 = throttle.get_time_seconds();
  ASSERT_GE(t2, t1);
}

TEST(network_throttle, get_recommended_size)
{
  epee::net_utils::network_throttle throttle("tst", "test_recommend");
  throttle.set_target_speed(1024); // 1024 kB/s

  size_t recommended = throttle.get_recommended_size_of_planned_transport();
  // Should recommend some positive size
  ASSERT_GT(recommended, 0u);
}

TEST(network_throttle, get_current_speed_initial)
{
  epee::net_utils::network_throttle throttle("tst", "test_current_speed");
  throttle.set_target_speed(1024);

  double speed = throttle.get_current_speed();
  // No traffic sent yet, speed should be 0
  ASSERT_GE(speed, 0.0);
}

TEST(network_throttle, calculate_times)
{
  epee::net_utils::network_throttle throttle("tst", "test_calc");
  throttle.set_target_speed(1024);

  epee::net_utils::calculate_times_struct cts;
  throttle.calculate_times(1000, cts, false, -1);
  ASSERT_GE(cts.delay, 0.0);
  ASSERT_GT(cts.window, 0.0);
}

TEST(network_throttle, tick)
{
  epee::net_utils::network_throttle throttle("tst", "test_tick2");
  throttle.set_target_speed(1024);

  // Tick should not crash even without traffic
  ASSERT_NO_THROW(throttle.tick());
}

TEST(network_throttle_bw, construct)
{
  epee::net_utils::network_throttle_bw bw("test_bw");
  // Should construct with in/inreq/out throttles
  uint64_t total_packets = 0;
  uint64_t total_bytes = 0;
  bw.m_in.get_stats(total_packets, total_bytes);
  ASSERT_EQ(total_packets, 0u);
  bw.m_inreq.get_stats(total_packets, total_bytes);
  ASSERT_EQ(total_packets, 0u);
  bw.m_out.get_stats(total_packets, total_bytes);
  ASSERT_EQ(total_packets, 0u);
}

TEST(network_throttle_manager, get_global_throttle_inreq)
{
  auto &throttle = epee::net_utils::network_throttle_manager::get_global_throttle_inreq();
  uint64_t total_packets = 0;
  uint64_t total_bytes = 0;
  throttle.get_stats(total_packets, total_bytes);
}

TEST(network_throttle_manager, get_global_throttle_out)
{
  auto &throttle = epee::net_utils::network_throttle_manager::get_global_throttle_out();
  uint64_t total_packets = 0;
  uint64_t total_bytes = 0;
  throttle.get_stats(total_packets, total_bytes);
}

TEST(network_throttle_manager, get_global_throttle_in)
{
  auto &throttle = epee::net_utils::network_throttle_manager::get_global_throttle_in();
  uint64_t total_packets = 0;
  uint64_t total_bytes = 0;
  throttle.get_stats(total_packets, total_bytes);
}
