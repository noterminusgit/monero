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

// Stub types that mimic the p2p interface for testing code that references
// node_server types. Since node_server is a complex template, we provide
// lightweight stubs for protocol handler and connection context.

#include <cstdint>
#include <vector>
#include <string>
#include "p2p/p2p_protocol_defs.h"
#include "net/net_utils_base.h"

namespace test
{
  // Minimal stub for a protocol handler payload object
  struct stub_protocol_handler
  {
    bool is_synchronized() const { return m_synchronized; }
    void set_synchronized(bool v) { m_synchronized = v; }

    bool m_synchronized = true;
  };

  // Helper struct for tracking peer list state in tests
  struct mock_peerlist
  {
    std::vector<nodetool::peerlist_entry> white_peers;
    std::vector<nodetool::peerlist_entry> gray_peers;
    std::vector<nodetool::peerlist_entry> anchor_peers;

    void add_white_peer(const nodetool::peerlist_entry& pe) { white_peers.push_back(pe); }
    void add_gray_peer(const nodetool::peerlist_entry& pe) { gray_peers.push_back(pe); }
    void add_anchor_peer(const nodetool::peerlist_entry& pe) { anchor_peers.push_back(pe); }
    void clear() { white_peers.clear(); gray_peers.clear(); anchor_peers.clear(); }
  };

  // Helper to create a peerlist_entry for testing
  inline nodetool::peerlist_entry make_test_peer(uint32_t ip, uint16_t port, uint64_t id = 0, uint64_t last_seen = 0)
  {
    nodetool::peerlist_entry pe{};
    pe.adr = epee::net_utils::ipv4_network_address(ip, port);
    pe.id = id;
    pe.last_seen = last_seen;
    return pe;
  }

} // namespace test
