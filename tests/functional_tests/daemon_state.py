#!/usr/bin/env python3

# Copyright (c) 2019-2024, The Monero Project
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are
# permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of
#    conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list
#    of conditions and the following disclaimer in the documentation and/or other
#    materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be
#    used to endorse or promote products derived from this software without specific
#    prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
# THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Test daemon RPC response contracts: sync_info, connections, net stats, version, miner data"""

from framework.daemon import Daemon

ADDR1 = '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm'

class DaemonStateTest():
    def run_test(self):
        self.mine()
        self.test_sync_info()
        self.test_get_connections()
        self.test_get_peer_list()
        self.test_get_net_stats()
        self.test_get_limit_set_limit()
        self.test_version()
        self.test_get_coinbase_tx_sum_ranges()
        self.test_get_miner_data()

    def mine(self):
        print('Mining blocks')
        daemon = Daemon()
        daemon.generateblocks(ADDR1, 50)

    def test_sync_info(self):
        print('Testing sync_info')
        daemon = Daemon()
        res = daemon.sync_info()
        assert 'height' in res
        assert res.height > 0
        assert 'target_height' in res
        # In offline mode, target_height may be 0

    def test_get_connections(self):
        print('Testing get_connections')
        daemon = Daemon()
        res = daemon.get_connections()
        # Offline daemon may have no connections
        if 'connections' in res and res.connections:
            for conn in res.connections:
                assert 'address' in conn
                assert 'state' in conn
                assert 'height' in conn

    def test_get_peer_list(self):
        print('Testing get_peer_list')
        daemon = Daemon()
        res = daemon.get_peer_list()
        # Verify the structure exists
        assert 'white_list' in res or 'gray_list' in res
        if 'white_list' in res:
            for peer in res.white_list:
                assert 'host' in peer or 'ip' in peer

    def test_get_net_stats(self):
        print('Testing get_net_stats')
        daemon = Daemon()
        res = daemon.get_net_stats()
        assert 'start_time' in res
        assert 'total_packets_in' in res
        assert 'total_bytes_in' in res
        assert 'total_packets_out' in res
        assert 'total_bytes_out' in res

    def test_get_limit_set_limit(self):
        print('Testing get_limit / set_limit')
        daemon = Daemon()

        # Get current limits
        res = daemon.get_limit()
        assert 'limit_down' in res
        assert 'limit_up' in res
        limit_down = res.limit_down
        limit_up = res.limit_up
        assert limit_down > 0
        assert limit_up > 0

        # Set new limits
        res = daemon.set_limit(limit_down = 1024, limit_up = 512)
        assert res.status == 'OK' or res.status == 'ok'

        # Verify new limits
        res = daemon.get_limit()
        assert res.limit_down == 1024
        assert res.limit_up == 512

        # Restore original limits
        daemon.set_limit(limit_down = limit_down, limit_up = limit_up)

    def test_version(self):
        print('Testing version')
        daemon = Daemon()
        res = daemon.get_version()
        assert 'version' in res
        assert res.version > 0
        # Version is packed as (major << 16) | minor
        print('Daemon version: {}'.format(res.version))

    def test_get_coinbase_tx_sum_ranges(self):
        print('Testing get_coinbase_tx_sum')
        daemon = Daemon()

        # Get coinbase sum for different ranges
        res = daemon.get_coinbase_tx_sum(height = 0, count = 10)
        assert 'emission_amount' in res
        assert 'fee_amount' in res
        sum1 = res.emission_amount

        res = daemon.get_coinbase_tx_sum(height = 0, count = 20)
        sum2 = res.emission_amount

        # Larger range should have >= emission
        assert sum2 >= sum1

        # Check a range starting from a later block
        res = daemon.get_coinbase_tx_sum(height = 10, count = 10)
        assert res.emission_amount > 0

    def test_get_miner_data(self):
        print('Testing get_miner_data')
        daemon = Daemon()
        res = daemon.get_miner_data()
        assert 'major_version' in res
        assert 'height' in res
        assert 'prev_id' in res
        assert 'seed_hash' in res
        assert 'difficulty' in res
        assert 'median_weight' in res
        assert res.height > 0
        assert res.major_version > 0
        assert res.prev_id != '' and res.prev_id != '0' * 64
        print('Miner data: height={}, version={}'.format(res.height, res.major_version))

if __name__ == '__main__':
    DaemonStateTest().run_test()
