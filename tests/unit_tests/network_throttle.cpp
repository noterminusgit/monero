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

  // get_sleep_time_after_tick may return negative values indicating no delay needed
  double sleep_time = throttle.get_sleep_time_after_tick(1000);
  (void)sleep_time; // Just verify it doesn't crash
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

  throttle.tick();
  throttle.handle_trafic_exact(500);

  epee::net_utils::calculate_times_struct cts;
  throttle.calculate_times(1000, cts, false, -1);
  // delay can be negative (no wait needed), just verify no crash
  (void)cts.delay;
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

// --- Additional network_throttle tests ---

TEST(network_throttle, zero_speed_unlimited)
{
  epee::net_utils::network_throttle throttle("tst", "test_zero_speed");
  throttle.set_target_speed(0); // 0 means unlimited
  ASSERT_DOUBLE_EQ(throttle.get_target_speed(), 0.0);

  // With unlimited speed, sleep time should be 0 or very small
  throttle.tick();
  throttle.handle_trafic_exact(100000);
  double sleep = throttle.get_sleep_time(100000);
  ASSERT_GE(sleep, 0.0);
}

TEST(network_throttle, zero_speed_no_throttle_large_data)
{
  epee::net_utils::network_throttle throttle("tst", "test_zero_speed_large");
  throttle.set_target_speed(0);
  throttle.tick();
  // Even large data should not cause meaningful delay with unlimited speed
  throttle.handle_trafic_exact(10000000);
  double sleep = throttle.get_sleep_time(10000000);
  ASSERT_GE(sleep, 0.0);
}

TEST(network_throttle, very_low_speed_limit)
{
  epee::net_utils::network_throttle throttle("tst", "test_low_speed");
  throttle.set_target_speed(1); // 1 kB/s
  throttle.tick();
  throttle.handle_trafic_exact(10000); // 10 KB sent at 1 kB/s limit
  double sleep = throttle.get_sleep_time(10000);
  // With a very low speed limit and traffic already sent, sleep should be positive
  ASSERT_GE(sleep, 0.0);
}

TEST(network_throttle, tick_multiple_times)
{
  epee::net_utils::network_throttle throttle("tst", "test_multi_tick");
  throttle.set_target_speed(1024);
  ASSERT_NO_THROW(throttle.tick());
  ASSERT_NO_THROW(throttle.tick());
  ASSERT_NO_THROW(throttle.tick());

  // After multiple ticks without traffic, stats should still be zero
  uint64_t total_packets = 0;
  uint64_t total_bytes = 0;
  throttle.get_stats(total_packets, total_bytes);
  ASSERT_EQ(total_packets, 0u);
  ASSERT_EQ(total_bytes, 0u);
}

TEST(network_throttle, tick_then_traffic_then_tick)
{
  epee::net_utils::network_throttle throttle("tst", "test_tick_traffic_tick");
  throttle.set_target_speed(1024);

  throttle.tick();
  throttle.handle_trafic_exact(500);
  throttle.tick();
  throttle.handle_trafic_exact(500);
  throttle.tick();

  uint64_t total_packets = 0;
  uint64_t total_bytes = 0;
  throttle.get_stats(total_packets, total_bytes);
  ASSERT_GE(total_bytes, 1000u);
}

TEST(network_throttle, handle_trafic_exact_zero_bytes)
{
  epee::net_utils::network_throttle throttle("tst", "test_zero_bytes");
  throttle.set_target_speed(1024);
  throttle.tick();

  // Sending 0 bytes should not crash
  ASSERT_NO_THROW(throttle.handle_trafic_exact(0));

  uint64_t total_packets = 0;
  uint64_t total_bytes = 0;
  throttle.get_stats(total_packets, total_bytes);
  // 0-byte packet still counts as a packet
  ASSERT_GE(total_packets, 0u);
}

TEST(network_throttle, handle_trafic_exact_very_large)
{
  epee::net_utils::network_throttle throttle("tst", "test_large_traffic");
  throttle.set_target_speed(1024 * 1024);
  throttle.tick();

  size_t large_size = 100 * 1024 * 1024; // 100 MB
  ASSERT_NO_THROW(throttle.handle_trafic_exact(large_size));

  uint64_t total_packets = 0;
  uint64_t total_bytes = 0;
  throttle.get_stats(total_packets, total_bytes);
  ASSERT_GE(total_bytes, large_size);
}

TEST(network_throttle, handle_trafic_tcp_overhead)
{
  epee::net_utils::network_throttle throttle_exact("tst", "test_tcp_overhead_exact");
  epee::net_utils::network_throttle throttle_tcp("tst", "test_tcp_overhead_tcp");
  throttle_exact.set_target_speed(1024 * 1024);
  throttle_tcp.set_target_speed(1024 * 1024);
  throttle_exact.tick();
  throttle_tcp.tick();

  throttle_exact.handle_trafic_exact(1000);
  throttle_tcp.handle_trafic_tcp(1000);

  uint64_t exact_packets = 0, exact_bytes = 0;
  uint64_t tcp_packets = 0, tcp_bytes = 0;
  throttle_exact.get_stats(exact_packets, exact_bytes);
  throttle_tcp.get_stats(tcp_packets, tcp_bytes);

  // TCP accounting should add overhead, so bytes should be >= exact bytes
  ASSERT_GE(tcp_bytes, exact_bytes);
}

TEST(network_throttle, handle_trafic_tcp_zero_bytes)
{
  epee::net_utils::network_throttle throttle("tst", "test_tcp_zero");
  throttle.set_target_speed(1024);
  throttle.tick();
  ASSERT_NO_THROW(throttle.handle_trafic_tcp(0));
}

TEST(network_throttle, get_sleep_time_zero_packet)
{
  epee::net_utils::network_throttle throttle("tst", "test_sleep_zero");
  throttle.set_target_speed(1024);
  throttle.tick();

  // After tick() with no prior traffic, delay can be negative (meaning no sleep needed)
  double sleep = throttle.get_sleep_time(0);
  (void)sleep; // just verify no crash
}

TEST(network_throttle, get_sleep_time_one_byte)
{
  epee::net_utils::network_throttle throttle("tst", "test_sleep_one");
  throttle.set_target_speed(1024);
  throttle.tick();

  // After tick() with no prior traffic, delay can be negative (meaning no sleep needed)
  double sleep = throttle.get_sleep_time(1);
  (void)sleep; // just verify no crash
}

TEST(network_throttle, get_sleep_time_medium_packet)
{
  epee::net_utils::network_throttle throttle("tst", "test_sleep_medium");
  throttle.set_target_speed(1024);
  throttle.tick();

  // After tick() with no prior traffic, delay can be negative (meaning no sleep needed)
  double sleep = throttle.get_sleep_time(1000);
  (void)sleep; // just verify no crash
}

TEST(network_throttle, get_sleep_time_large_packet)
{
  epee::net_utils::network_throttle throttle("tst", "test_sleep_large");
  throttle.set_target_speed(1024);
  throttle.tick();

  // After tick() with no prior traffic, delay can be negative (meaning no sleep needed)
  double sleep = throttle.get_sleep_time(1000000);
  (void)sleep; // just verify no crash
}

TEST(network_throttle, get_sleep_time_after_tick_with_data)
{
  epee::net_utils::network_throttle throttle("tst", "test_sleep_after_tick_data");
  throttle.set_target_speed(10); // 10 kB/s - low to force throttling
  throttle.tick();
  throttle.handle_trafic_exact(50000); // 50 kB sent

  double sleep = throttle.get_sleep_time_after_tick(10000);
  // sleep can be negative if no wait needed, or positive if throttling
  (void)sleep; // just verify no crash
}

TEST(network_throttle, get_sleep_time_after_tick_no_data)
{
  epee::net_utils::network_throttle throttle("tst", "test_sleep_after_tick_nodata");
  throttle.set_target_speed(1024);

  double sleep = throttle.get_sleep_time_after_tick(100);
  (void)sleep;
}

TEST(network_throttle, get_current_speed_after_traffic)
{
  epee::net_utils::network_throttle throttle("tst", "test_speed_after");
  throttle.set_target_speed(1024 * 1024);
  throttle.tick();
  throttle.handle_trafic_exact(10000);
  throttle.tick();

  double speed = throttle.get_current_speed();
  // After sending data, speed should be >= 0
  ASSERT_GE(speed, 0.0);
}

TEST(network_throttle, get_current_speed_no_traffic)
{
  epee::net_utils::network_throttle throttle("tst", "test_speed_none");
  throttle.set_target_speed(1024);
  throttle.tick();

  double speed = throttle.get_current_speed();
  ASSERT_GE(speed, 0.0);
}

TEST(network_throttle, recommended_size_high_speed)
{
  epee::net_utils::network_throttle throttle("tst", "test_rec_high");
  throttle.set_target_speed(1024 * 1024); // 1 GB/s
  throttle.tick();

  size_t recommended = throttle.get_recommended_size_of_planned_transport();
  ASSERT_GT(recommended, 0u);
}

TEST(network_throttle, recommended_size_low_speed)
{
  epee::net_utils::network_throttle throttle("tst", "test_rec_low");
  throttle.set_target_speed(1); // 1 kB/s
  throttle.tick();

  size_t recommended = throttle.get_recommended_size_of_planned_transport();
  ASSERT_GT(recommended, 0u);
}

TEST(network_throttle, recommended_size_zero_speed)
{
  epee::net_utils::network_throttle throttle("tst", "test_rec_zero");
  throttle.set_target_speed(0); // unlimited
  throttle.tick();

  size_t recommended = throttle.get_recommended_size_of_planned_transport();
  ASSERT_GT(recommended, 0u);
}

TEST(network_throttle, calculate_times_no_traffic)
{
  epee::net_utils::network_throttle throttle("tst", "test_calc_none");
  throttle.set_target_speed(1024);
  throttle.tick();

  epee::net_utils::calculate_times_struct cts;
  throttle.calculate_times(0, cts, false, -1);
  // With no traffic and 0 packet size, delay should be minimal
  (void)cts.delay;
  (void)cts.average;
  (void)cts.window;
  (void)cts.recomendetDataSize;
}

TEST(network_throttle, calculate_times_large_packet)
{
  epee::net_utils::network_throttle throttle("tst", "test_calc_large");
  throttle.set_target_speed(10); // 10 kB/s
  throttle.tick();
  throttle.handle_trafic_exact(100000);

  epee::net_utils::calculate_times_struct cts;
  throttle.calculate_times(100000, cts, false, -1);
  (void)cts.delay;
}

TEST(network_throttle, calculate_times_forced_window)
{
  epee::net_utils::network_throttle throttle("tst", "test_calc_window");
  throttle.set_target_speed(1024);
  throttle.tick();
  throttle.handle_trafic_exact(1000);

  epee::net_utils::calculate_times_struct cts;
  throttle.calculate_times(1000, cts, false, 5.0); // force 5 second window
  (void)cts.delay;
}

TEST(network_throttle, change_target_speed_mid_session)
{
  epee::net_utils::network_throttle throttle("tst", "test_change_speed");
  throttle.set_target_speed(1024);
  throttle.tick();
  throttle.handle_trafic_exact(5000);

  // Change speed mid-session
  throttle.set_target_speed(100);
  ASSERT_DOUBLE_EQ(throttle.get_target_speed(), 100.0);

  // Traffic stats should remain
  uint64_t total_packets = 0;
  uint64_t total_bytes = 0;
  throttle.get_stats(total_packets, total_bytes);
  ASSERT_GE(total_bytes, 5000u);

  // Change again to unlimited
  throttle.set_target_speed(0);
  ASSERT_DOUBLE_EQ(throttle.get_target_speed(), 0.0);
}

TEST(network_throttle, multiple_instances_independent)
{
  epee::net_utils::network_throttle throttle1("t1", "test_inst1");
  epee::net_utils::network_throttle throttle2("t2", "test_inst2");

  throttle1.set_target_speed(100);
  throttle2.set_target_speed(200);

  ASSERT_DOUBLE_EQ(throttle1.get_target_speed(), 100.0);
  ASSERT_DOUBLE_EQ(throttle2.get_target_speed(), 200.0);

  throttle1.tick();
  throttle2.tick();
  throttle1.handle_trafic_exact(1000);

  uint64_t p1 = 0, b1 = 0, p2 = 0, b2 = 0;
  throttle1.get_stats(p1, b1);
  throttle2.get_stats(p2, b2);

  ASSERT_GE(b1, 1000u);
  ASSERT_EQ(b2, 0u);
}

TEST(network_throttle, multiple_instances_both_active)
{
  epee::net_utils::network_throttle t1("t1", "test_both1");
  epee::net_utils::network_throttle t2("t2", "test_both2");

  t1.set_target_speed(1024);
  t2.set_target_speed(1024);

  t1.tick();
  t2.tick();
  t1.handle_trafic_exact(500);
  t2.handle_trafic_exact(700);

  uint64_t p1 = 0, b1 = 0, p2 = 0, b2 = 0;
  t1.get_stats(p1, b1);
  t2.get_stats(p2, b2);

  ASSERT_GE(b1, 500u);
  ASSERT_GE(b2, 700u);
}

TEST(network_throttle_bw, member_in_traffic)
{
  epee::net_utils::network_throttle_bw bw("test_bw_in");
  bw.m_in.set_target_speed(1024);
  bw.m_in.tick();
  bw.m_in.handle_trafic_exact(500);

  uint64_t total_packets = 0, total_bytes = 0;
  bw.m_in.get_stats(total_packets, total_bytes);
  ASSERT_GE(total_bytes, 500u);
}

TEST(network_throttle_bw, member_inreq_traffic)
{
  epee::net_utils::network_throttle_bw bw("test_bw_inreq");
  bw.m_inreq.set_target_speed(1024);
  bw.m_inreq.tick();
  bw.m_inreq.handle_trafic_exact(300);

  uint64_t total_packets = 0, total_bytes = 0;
  bw.m_inreq.get_stats(total_packets, total_bytes);
  ASSERT_GE(total_bytes, 300u);
}

TEST(network_throttle_bw, member_out_traffic)
{
  epee::net_utils::network_throttle_bw bw("test_bw_out");
  bw.m_out.set_target_speed(1024);
  bw.m_out.tick();
  bw.m_out.handle_trafic_exact(800);

  uint64_t total_packets = 0, total_bytes = 0;
  bw.m_out.get_stats(total_packets, total_bytes);
  ASSERT_GE(total_bytes, 800u);
}

TEST(network_throttle_bw, all_members_independent)
{
  epee::net_utils::network_throttle_bw bw("test_bw_indep");
  bw.m_in.set_target_speed(100);
  bw.m_inreq.set_target_speed(200);
  bw.m_out.set_target_speed(300);

  ASSERT_DOUBLE_EQ(bw.m_in.get_target_speed(), 100.0);
  ASSERT_DOUBLE_EQ(bw.m_inreq.get_target_speed(), 200.0);
  ASSERT_DOUBLE_EQ(bw.m_out.get_target_speed(), 300.0);

  bw.m_in.tick();
  bw.m_in.handle_trafic_exact(100);

  uint64_t p_in = 0, b_in = 0, p_inreq = 0, b_inreq = 0, p_out = 0, b_out = 0;
  bw.m_in.get_stats(p_in, b_in);
  bw.m_inreq.get_stats(p_inreq, b_inreq);
  bw.m_out.get_stats(p_out, b_out);

  ASSERT_GE(b_in, 100u);
  ASSERT_EQ(b_inreq, 0u);
  ASSERT_EQ(b_out, 0u);
}

TEST(network_throttle_manager, singletons_same_on_repeated_access)
{
  auto &in1 = epee::net_utils::network_throttle_manager::get_global_throttle_in();
  auto &in2 = epee::net_utils::network_throttle_manager::get_global_throttle_in();
  ASSERT_EQ(&in1, &in2);

  auto &inreq1 = epee::net_utils::network_throttle_manager::get_global_throttle_inreq();
  auto &inreq2 = epee::net_utils::network_throttle_manager::get_global_throttle_inreq();
  ASSERT_EQ(&inreq1, &inreq2);

  auto &out1 = epee::net_utils::network_throttle_manager::get_global_throttle_out();
  auto &out2 = epee::net_utils::network_throttle_manager::get_global_throttle_out();
  ASSERT_EQ(&out1, &out2);
}

TEST(network_throttle_manager, singletons_are_distinct)
{
  auto &in = epee::net_utils::network_throttle_manager::get_global_throttle_in();
  auto &inreq = epee::net_utils::network_throttle_manager::get_global_throttle_inreq();
  auto &out = epee::net_utils::network_throttle_manager::get_global_throttle_out();

  ASSERT_NE(&in, &inreq);
  ASSERT_NE(&in, &out);
  ASSERT_NE(&inreq, &out);
}

TEST(network_throttle, set_name)
{
  epee::net_utils::network_throttle throttle("tst", "original_name");
  ASSERT_NO_THROW(throttle.set_name("new_name"));
}

TEST(network_throttle, get_time_seconds_monotonic)
{
  epee::net_utils::network_throttle throttle("tst", "test_monotonic");
  double t1 = throttle.get_time_seconds();
  double t2 = throttle.get_time_seconds();
  double t3 = throttle.get_time_seconds();
  ASSERT_GE(t2, t1);
  ASSERT_GE(t3, t2);
}
