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

"""Test output/key image query contracts: is_key_image_spent, get_outs, histogram, distribution, freeze/thaw"""

from framework.daemon import Daemon
from framework.wallet import Wallet

SEED1 = 'velvet lymph giddy number token physics poetry unquoted nibs useful sabotage limits benches lifestyle eden nitrogen anvil fewest avoid batch vials washing fences goat unquoted'
SEED2 = 'peeled mixture ionic radar utopia puddle buying illness nuns gadget river spout cavernous bounced paradise drunk looking cottage jump tequila melting went winter adjust spout'
ADDR1 = '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm'

class KeyImageOutputQueriesTest():
    def run_test(self):
        self.reset()
        self.create()
        self.mine()
        self.test_is_key_image_spent_lifecycle()
        self.test_get_outs_details()
        self.test_get_output_histogram()
        self.test_get_output_distribution_ranges()
        self.test_freeze_thaw()

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
        print('Mining initial blocks')
        daemon = Daemon()
        daemon.generateblocks(ADDR1, 100)
        for w in self.wallet:
            w.refresh()

    def test_is_key_image_spent_lifecycle(self):
        print('Testing is_key_image_spent lifecycle')
        daemon = Daemon()
        w0 = self.wallet[0]
        w1 = self.wallet[1]

        # Get a key image from an available output
        res = w0.incoming_transfers(transfer_type = 'available')
        assert 'transfers' in res and len(res.transfers) > 0
        ki = res.transfers[0].key_image

        # Should be unspent (0)
        res = daemon.is_key_image_spent([ki])
        assert res.spent_status[0] == 0  # 0 = unspent

        # Spend it by transferring
        res = w1.get_address()
        dst = res.address
        daemon.generateblocks(ADDR1, 10)
        w0.refresh()
        res = w0.sweep_single(address = dst, key_image = ki)
        tx_hash = res.tx_hash

        # Should be in pool (2)
        res = daemon.is_key_image_spent([ki])
        assert res.spent_status[0] == 2  # 2 = spent in pool

        # Mine to confirm
        daemon.generateblocks(ADDR1, 1)

        # Should be spent in chain (1)
        res = daemon.is_key_image_spent([ki])
        assert res.spent_status[0] == 1  # 1 = spent in chain

        w0.refresh()
        w1.refresh()

    def test_get_outs_details(self):
        print('Testing get_outs details')
        daemon = Daemon()

        # Get outputs by index
        res = daemon.get_outs([{'amount': 0, 'index': 0}, {'amount': 0, 'index': 1}], get_txid = True)
        assert 'outs' in res
        assert len(res.outs) == 2
        for out in res.outs:
            assert 'key' in out
            assert 'mask' in out
            assert 'height' in out
            assert 'txid' in out
            assert out.key != '' and out.key != '0' * 64

    def test_get_output_histogram(self):
        print('Testing get_output_histogram')
        daemon = Daemon()

        # Get histogram for RCT outputs (amount 0)
        res = daemon.get_output_histogram([0])
        assert 'histogram' in res
        assert len(res.histogram) > 0
        entry = res.histogram[0]
        assert entry.amount == 0
        assert entry.total_instances > 0
        print('Output histogram: {} RCT outputs'.format(entry.total_instances))

    def test_get_output_distribution_ranges(self):
        print('Testing get_output_distribution')
        daemon = Daemon()

        res = daemon.get_output_distribution([0], from_height = 0, to_height = 0, cumulative = True)
        assert 'distributions' in res
        assert len(res.distributions) > 0
        dist = res.distributions[0]
        assert dist.amount == 0
        assert 'distribution' in dist
        assert len(dist.distribution) > 0

        # Cumulative distribution should be monotonically increasing
        prev = 0
        for val in dist.distribution:
            assert val >= prev, 'Distribution not monotonically increasing: {} < {}'.format(val, prev)
            prev = val

    def test_freeze_thaw(self):
        print('Testing freeze/thaw')
        w0 = self.wallet[0]
        daemon = Daemon()

        daemon.generateblocks(ADDR1, 10)
        w0.refresh()

        res = w0.get_balance()
        balance_before = res.unlocked_balance

        # Get a key image to freeze
        res = w0.incoming_transfers(transfer_type = 'available')
        assert 'transfers' in res and len(res.transfers) > 0
        ki = res.transfers[0].key_image
        frozen_amount = res.transfers[0].amount

        # Freeze it
        w0.freeze(key_image = ki)

        # Check frozen status
        res = w0.frozen(key_image = ki)
        assert res.frozen == True

        # Balance should decrease
        res = w0.get_balance()
        assert res.unlocked_balance < balance_before

        # Thaw it
        w0.thaw(key_image = ki)

        # Check unfrozen
        res = w0.frozen(key_image = ki)
        assert res.frozen == False

        # Balance should restore
        res = w0.get_balance()
        assert res.unlocked_balance == balance_before

if __name__ == '__main__':
    KeyImageOutputQueriesTest().run_test()
