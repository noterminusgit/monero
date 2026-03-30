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

"""Test wallet lifecycle: set_daemon, auto_refresh, file operations, change_password, languages"""

import util_resources
from framework.daemon import Daemon
from framework.wallet import Wallet

SEED1 = 'velvet lymph giddy number token physics poetry unquoted nibs useful sabotage limits benches lifestyle eden nitrogen anvil fewest avoid batch vials washing fences goat unquoted'
ADDR1 = '42ey1afDFnn4886T7196doS9GPMzexD9gXpsZJDwVjeRVdFCSoHnv7KPbBeGpzJBzHRCAs9UxqeoyFQMYbqSWYTfJJQAWDm'

class WalletDaemonSwitchingTest():
    def run_test(self):
        self.reset()
        self.test_wallet_file_operations()
        self.test_change_wallet_password()
        self.test_auto_refresh()
        self.test_get_languages()
        self.test_set_daemon()

    def reset(self):
        print('Resetting blockchain')
        daemon = Daemon()
        res = daemon.get_height()
        daemon.pop_blocks(res.height - 1)
        daemon.flush_txpool()
        daemon.generateblocks(ADDR1, 100)

    def test_wallet_file_operations(self):
        print('Testing wallet file operations')
        w = Wallet(idx = 0)
        try: w.close_wallet()
        except: pass

        # Create a new wallet
        filename = 'test_file_ops'
        util_resources.remove_wallet_files(filename)
        w.restore_deterministic_wallet(seed = SEED1, filename = filename)
        w.refresh()

        res = w.get_balance()
        balance = res.balance
        assert balance > 0

        # Store the wallet
        w.store()

        # Close and reopen
        w.close_wallet()
        w.open_wallet(filename = filename)
        w.refresh()

        # State should persist
        res = w.get_balance()
        assert res.balance == balance

        w.close_wallet()

    def test_change_wallet_password(self):
        print('Testing change wallet password')
        w = Wallet(idx = 0)
        try: w.close_wallet()
        except: pass

        filename = 'test_change_pw'
        util_resources.remove_wallet_files(filename)
        w.restore_deterministic_wallet(seed = SEED1, filename = filename, password = 'old_password')

        # Change password
        w.change_wallet_password(old_password = 'old_password', new_password = 'new_password')
        w.close_wallet()

        # Reopen with new password
        w.open_wallet(filename = filename, password = 'new_password')
        res = w.get_address()
        assert res.address == ADDR1

        w.close_wallet()

    def test_auto_refresh(self):
        print('Testing auto_refresh')
        daemon = Daemon()
        w = Wallet(idx = 0)
        try: w.close_wallet()
        except: pass

        w.restore_deterministic_wallet(seed = SEED1)

        # Enable auto refresh
        w.auto_refresh(enable = True, period = 1)

        # Disable auto refresh
        w.auto_refresh(enable = False)

        # Manual refresh should still work
        w.refresh()
        res = w.get_height()
        assert res.height > 0

    def test_get_languages(self):
        print('Testing get_languages')
        w = Wallet(idx = 0)
        try: w.close_wallet()
        except: pass

        w.restore_deterministic_wallet(seed = SEED1)

        res = w.get_languages()
        assert 'languages' in res
        assert len(res.languages) > 0
        # English should always be in the list
        found_english = False
        for lang in res.languages:
            if 'English' in lang:
                found_english = True
                break
        assert found_english, 'English not found in language list'
        print('Languages: {}'.format(', '.join(res.languages[:5])))

    def test_set_daemon(self):
        print('Testing set_daemon')
        w = Wallet(idx = 0)
        try: w.close_wallet()
        except: pass

        w.restore_deterministic_wallet(seed = SEED1)

        # Set daemon to the same address (should work)
        w.set_daemon(address = '127.0.0.1:18180')
        w.refresh()

        res = w.get_height()
        assert res.height > 0

if __name__ == '__main__':
    WalletDaemonSwitchingTest().run_test()
