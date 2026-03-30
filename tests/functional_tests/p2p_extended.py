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

"""Test P2P sync contracts: peer sync, block propagation between connected daemons"""

import time
from framework.daemon import Daemon

STANDARD_ADDRESS = '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm'

class P2PExtendedTest():
    def run_test(self):
        self.test_peer_sync()
        self.test_block_propagation_timing()
        self.test_concurrent_block_generation()

    def test_peer_sync(self):
        print('Testing peer sync between connected daemons')
        # Daemons 2 and 3 are connected to each other via --add-exclusive-node
        daemon2 = Daemon(idx = 2)
        daemon3 = Daemon(idx = 3)

        # Mine blocks on daemon2
        daemon2.generateblocks(STANDARD_ADDRESS, 10)

        # Wait for propagation
        time.sleep(3)

        # daemon3 should have synced
        res2 = daemon2.get_info()
        res3 = daemon3.get_info()
        assert res3.height == res2.height, \
            'Height mismatch after sync: daemon2={}, daemon3={}'.format(res2.height, res3.height)
        print('Peer sync: both daemons at height {}'.format(res2.height))

    def test_block_propagation_timing(self):
        print('Testing block propagation timing')
        daemon2 = Daemon(idx = 2)
        daemon3 = Daemon(idx = 3)

        res = daemon2.get_info()
        start_height = res.height

        # Mine a block on daemon2
        daemon2.generateblocks(STANDARD_ADDRESS, 1)

        # Poll daemon3 for the new block (should propagate quickly)
        synced = False
        for _ in range(30):  # 3 seconds max
            res = daemon3.get_info()
            if res.height == start_height + 1:
                synced = True
                break
            time.sleep(0.1)

        assert synced, 'Block did not propagate within 3 seconds'
        print('Block propagation: OK')

    def test_concurrent_block_generation(self):
        print('Testing concurrent block generation')
        daemon2 = Daemon(idx = 2)
        daemon3 = Daemon(idx = 3)

        # Mine blocks on both daemons - the network should converge
        daemon2.generateblocks(STANDARD_ADDRESS, 5)
        time.sleep(1)
        daemon3.generateblocks(STANDARD_ADDRESS, 3)
        time.sleep(3)

        # Both should converge to the same height
        res2 = daemon2.get_info()
        res3 = daemon3.get_info()
        assert res2.height == res3.height, \
            'Heights diverged: daemon2={}, daemon3={}'.format(res2.height, res3.height)
        print('Concurrent blocks: converged at height {}'.format(res2.height))

if __name__ == '__main__':
    P2PExtendedTest().run_test()
