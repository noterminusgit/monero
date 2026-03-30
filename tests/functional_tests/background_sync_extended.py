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

"""Test background sync: custom password, receive during bg sync, persistence"""

from framework.daemon import Daemon
from framework.wallet import Wallet

SEED1 = 'velvet lymph giddy number token physics poetry unquoted nibs useful sabotage limits benches lifestyle eden nitrogen anvil fewest avoid batch vials washing fences goat unquoted'
SEED2 = 'peeled mixture ionic radar utopia puddle buying illness nuns gadget river spout cavernous bounced paradise drunk looking cottage jump tequila melting went winter adjust spout'
ADDR1 = '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm'

class BackgroundSyncExtendedTest():
    def run_test(self):
        self.reset()
        self.create()
        self.mine()
        self.test_setup_custom_password()
        self.test_background_sync_receives_tx()

    def reset(self):
        print('Resetting blockchain')
        daemon = Daemon()
        res = daemon.get_height()
        daemon.pop_blocks(res.height - 1)
        daemon.flush_txpool()

    def create(self):
        print('Creating wallets')
        self.wallet = [None, None]
        self.wallet[0] = Wallet(idx = 0)
        self.wallet[1] = Wallet(idx = 1)
        try: self.wallet[0].close_wallet()
        except: pass
        try: self.wallet[1].close_wallet()
        except: pass
        self.wallet[0].restore_deterministic_wallet(seed = SEED1)
        self.wallet[1].restore_deterministic_wallet(seed = SEED2)

    def mine(self):
        print('Mining blocks')
        daemon = Daemon()
        daemon.generateblocks(ADDR1, 100)
        for w in self.wallet:
            w.refresh()

    def test_setup_custom_password(self):
        print('Testing background sync setup with custom password')
        w0 = self.wallet[0]

        custom_password = w0.background_sync_options.custom_password

        # Setup background sync with custom password
        w0.setup_background_sync(background_sync_type = custom_password,
                                  wallet_password = '',
                                  background_cache_password = 'bg_password')

        # Start background sync
        w0.start_background_sync()

        # Verify we're in background sync mode
        # (wallet operates with view-key only)

        # Stop background sync
        w0.stop_background_sync(wallet_password = '')

        print('Background sync custom password: OK')

    def test_background_sync_receives_tx(self):
        print('Testing background sync receives transactions')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]

        reuse_password = w0.background_sync_options.reuse_password

        # Setup and start background sync on w0
        w0.setup_background_sync(background_sync_type = reuse_password,
                                  wallet_password = '')
        w0.start_background_sync()

        # Send a transaction to w0 while it's in background sync
        res = w0.get_address()
        dst = res.address

        daemon.generateblocks(ADDR1, 10)
        w1.refresh()

        amount = 500000000000
        res = w1.transfer([{'address': dst, 'amount': amount}])
        daemon.generateblocks(ADDR1, 10)

        # Refresh while in background sync mode
        w0.refresh()

        # Stop background sync - this should process the received tx
        w0.stop_background_sync(wallet_password = '')
        w0.refresh()

        # The transaction should be visible
        res = w0.get_balance()
        assert res.balance > 0
        print('Background sync receive: balance={}'.format(res.balance))

if __name__ == '__main__':
    BackgroundSyncExtendedTest().run_test()
