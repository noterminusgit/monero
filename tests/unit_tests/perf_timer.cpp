// Copyright (c) 2016-2024, The Monero Project
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

#include "common/perf_timer.h"
#include <thread>
#include <chrono>

TEST(perf_timer, default_construct)
{
  tools::PerformanceTimer timer;
  // Timer should be running and have a non-negative value
  uint64_t v = timer.value();
  // Value is in nanoseconds, should be >= 0
  ASSERT_GE(v, 0u);
}

TEST(perf_timer, paused_construct)
{
  tools::PerformanceTimer timer(true);
  // Paused timer should report 0
  ASSERT_EQ(timer.value(), 0u);
}

TEST(perf_timer, pause_resume)
{
  tools::PerformanceTimer timer;
  // Let some time pass
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  timer.pause();
  uint64_t paused_val = timer.value();
  ASSERT_GT(paused_val, 0u);

  // Value should not change while paused
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ASSERT_EQ(timer.value(), paused_val);

  // After resume, value should increase again
  timer.resume();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  ASSERT_GT(timer.value(), paused_val);
}

TEST(perf_timer, reset_running)
{
  tools::PerformanceTimer timer;
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  uint64_t before_reset = timer.value();
  ASSERT_GT(before_reset, 0u);

  timer.reset();
  // After reset, value should be small (close to 0)
  uint64_t after_reset = timer.value();
  ASSERT_LT(after_reset, before_reset);
}

TEST(perf_timer, reset_paused)
{
  tools::PerformanceTimer timer(true);
  // Already paused, value is 0
  ASSERT_EQ(timer.value(), 0u);
  timer.reset();
  ASSERT_EQ(timer.value(), 0u);
}

TEST(perf_timer, double_pause_is_noop)
{
  tools::PerformanceTimer timer;
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  timer.pause();
  uint64_t v1 = timer.value();
  timer.pause(); // should be no-op
  uint64_t v2 = timer.value();
  ASSERT_EQ(v1, v2);
}

TEST(perf_timer, double_resume_is_noop)
{
  tools::PerformanceTimer timer;
  timer.resume(); // already running, should be no-op
  // Should not crash or change state
  ASSERT_GT(timer.value(), 0u);
}

TEST(perf_timer, implicit_uint64_conversion)
{
  tools::PerformanceTimer timer;
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  uint64_t v = timer; // implicit conversion
  ASSERT_GT(v, 0u);
}

TEST(perf_timer, get_tick_count_increases)
{
  uint64_t t1 = tools::get_tick_count();
  // Small busy wait
  volatile int x = 0;
  for (int i = 0; i < 1000; ++i) x += i;
  uint64_t t2 = tools::get_tick_count();
  ASSERT_GT(t2, t1);
}

TEST(perf_timer, ticks_to_ns_nonzero)
{
  uint64_t ns = tools::ticks_to_ns(1000000);
  ASSERT_GT(ns, 0u);
}

TEST(perf_timer, set_log_level)
{
  // Should not throw for valid levels
  tools::set_performance_timer_log_level(el::Level::Debug);
  tools::set_performance_timer_log_level(el::Level::Info);
  tools::set_performance_timer_log_level(el::Level::Warning);
  // Invalid level should fall back to Info (logged error but no crash)
  tools::set_performance_timer_log_level(el::Level::Unknown);
}
