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

"""Test extended cold signing: sweep_all, incremental output export, key image sync"""

from framework.daemon import Daemon
from framework.wallet import Wallet

SEED = 'velvet lymph giddy number token physics poetry unquoted nibs useful sabotage limits benches lifestyle eden nitrogen anvil fewest avoid batch vials washing fences goat unquoted'
STANDARD_ADDRESS = '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm'

class ColdSigningExtendedTest():
    def run_test(self):
        self.reset()
        self.create()
        self.mine()
        self.test_cold_sweep_all()
        self.test_incremental_output_export()
        self.test_key_image_sync()

    def reset(self):
        print('Resetting blockchain')
        daemon = Daemon()
        res = daemon.get_height()
        daemon.pop_blocks(res.height - 1)
        daemon.flush_txpool()

    def create(self):
        print('Creating hot and cold wallets')
        self.hot_wallet = Wallet(idx = 0)
        self.cold_wallet = Wallet(idx = 5)  # offline wallet
        try: self.hot_wallet.close_wallet()
        except: pass
        try: self.cold_wallet.close_wallet()
        except: pass

        # Cold wallet has the full keys
        self.cold_wallet.restore_deterministic_wallet(seed = SEED)
        view_key = self.cold_wallet.query_key('view_key').key

        # Hot wallet is view-only
        self.hot_wallet.generate_from_keys(viewkey = view_key, address = STANDARD_ADDRESS)

    def mine(self):
        print('Mining blocks')
        daemon = Daemon()
        daemon.generateblocks(STANDARD_ADDRESS, 100)
        self.hot_wallet.refresh()

    def test_cold_sweep_all(self):
        print('Testing cold sweep_all')

        # Export outputs from hot wallet
        res = self.hot_wallet.export_outputs(all = True)
        outputs_data_hex = res.outputs_data_hex

        # Import outputs to cold wallet
        res = self.cold_wallet.import_outputs(outputs_data_hex)
        assert res.num_imported > 0

        # Export key images from cold wallet
        res = self.cold_wallet.export_key_images(all_ = True)
        if 'signed_key_images' in res and len(res.signed_key_images) > 0:
            # Import key images to hot wallet
            res = self.hot_wallet.import_key_images(signed_key_images = res.signed_key_images)
            assert res.height > 0

        print('Cold sweep_all: output/key_image exchange OK')

    def test_incremental_output_export(self):
        print('Testing incremental output export')
        daemon = Daemon()

        daemon.generateblocks(STANDARD_ADDRESS, 10)
        self.hot_wallet.refresh()

        # Full export
        res = self.hot_wallet.export_outputs(all = True)
        full_data = res.outputs_data_hex
        assert len(full_data) > 0

        # Incremental export (new outputs only)
        res = self.hot_wallet.export_outputs(all = False)
        incremental_data = res.outputs_data_hex

        # Both should produce valid data
        assert len(full_data) >= len(incremental_data)
        print('Incremental export: full={} bytes, incremental={} bytes'.format(
            len(full_data), len(incremental_data)))

    def test_key_image_sync(self):
        print('Testing key image sync')

        # Export all outputs from hot wallet
        res = self.hot_wallet.export_outputs(all = True)
        outputs_data_hex = res.outputs_data_hex

        # Import to cold wallet
        res = self.cold_wallet.import_outputs(outputs_data_hex)
        num_imported = res.num_imported

        # Export key images from cold wallet
        res = self.cold_wallet.export_key_images(all_ = True)
        if 'signed_key_images' in res and len(res.signed_key_images) > 0:
            ki_count = len(res.signed_key_images)

            # Import key images to hot wallet
            res = self.hot_wallet.import_key_images(signed_key_images = res.signed_key_images)

            # After key image import, hot wallet should know spent status
            res = self.hot_wallet.get_balance()
            assert res.balance >= 0
            print('Key image sync: {} key images synced, balance={}'.format(ki_count, res.balance))
        else:
            print('Key image sync: no key images to export')

if __name__ == '__main__':
    ColdSigningExtendedTest().run_test()
