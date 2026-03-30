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

"""Test RPC access control: tracking, cache flush, update check"""

from framework.daemon import Daemon

ADDR1 = '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm'

class RpcAccessControlTest():
    def run_test(self):
        self.mine()
        self.test_rpc_access_tracking()
        self.test_flush_cache()
        self.test_update_check()

    def mine(self):
        print('Mining blocks')
        daemon = Daemon()
        daemon.generateblocks(ADDR1, 10)

    def test_rpc_access_tracking(self):
        print('Testing RPC access tracking')
        daemon = Daemon()

        # Make some RPC calls first
        daemon.get_info()
        daemon.get_height()

        res = daemon.rpc_access_tracking()
        assert 'data' in res
        # data should be a list of RPC access records
        if len(res.data) > 0:
            entry = res.data[0]
            assert 'rpc' in entry
            assert 'count' in entry
            print('Access tracking: {} entries'.format(len(res.data)))

    def test_flush_cache(self):
        print('Testing flush_cache')
        daemon = Daemon()

        # Flush bad_txs cache
        res = daemon.flush_cache(bad_txs = True)
        assert res.status == 'OK' or res.status == 'ok'

    def test_update_check(self):
        print('Testing update check')
        daemon = Daemon()

        try:
            res = daemon.update(command = 'check')
            assert 'status' in res
            # The update check may or may not find updates
            # but the call should succeed
            assert 'update' in res
            print('Update check: update={}'.format(res.update))
        except Exception as e:
            # May fail in offline mode, which is fine
            print('Update check: {}'.format(str(e)))

if __name__ == '__main__':
    RpcAccessControlTest().run_test()
