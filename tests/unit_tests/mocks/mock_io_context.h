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

// Helpers for testing timer-dependent and async code.
// Provides a mock steady timer that can be manually advanced for testing
// levin_notify, network_throttle, and other timer-based components.

#include <chrono>
#include <functional>
#include <queue>
#include <vector>

namespace test
{
  // A mock clock that can be manually advanced
  struct mock_clock
  {
    using time_point = std::chrono::steady_clock::time_point;
    using duration = std::chrono::steady_clock::duration;

    mock_clock() : m_now(std::chrono::steady_clock::now()) {}

    time_point now() const { return m_now; }

    void advance(duration d) { m_now += d; }

    void advance_ms(uint64_t ms)
    {
      m_now += std::chrono::milliseconds(ms);
    }

    void advance_seconds(uint64_t s)
    {
      m_now += std::chrono::seconds(s);
    }

  private:
    time_point m_now;
  };

  // Timer callback queue for testing async patterns synchronously
  struct timer_queue
  {
    struct pending_timer {
      std::chrono::steady_clock::time_point deadline;
      std::function<void()> callback;
      bool operator>(const pending_timer& other) const { return deadline > other.deadline; }
    };

    void schedule(std::chrono::steady_clock::time_point deadline, std::function<void()> cb)
    {
      m_queue.push({deadline, std::move(cb)});
    }

    void schedule_after(std::chrono::steady_clock::duration delay,
                        std::chrono::steady_clock::time_point now,
                        std::function<void()> cb)
    {
      m_queue.push({now + delay, std::move(cb)});
    }

    // Fire all timers whose deadline <= the given time point
    size_t fire_expired(std::chrono::steady_clock::time_point now)
    {
      size_t fired = 0;
      while (!m_queue.empty() && m_queue.top().deadline <= now)
      {
        auto cb = m_queue.top().callback;
        m_queue.pop();
        if (cb) cb();
        ++fired;
      }
      return fired;
    }

    size_t pending_count() const { return m_queue.size(); }
    bool empty() const { return m_queue.empty(); }

    void clear()
    {
      while (!m_queue.empty()) m_queue.pop();
    }

  private:
    std::priority_queue<pending_timer, std::vector<pending_timer>, std::greater<pending_timer>> m_queue;
  };

} // namespace test
