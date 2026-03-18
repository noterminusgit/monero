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

#include "gtest/gtest.h"

#include "device/device.hpp"
#include "device/device_default.hpp"

TEST(device_registry, get_default_device)
{
  hw::device& dev = hw::get_device("default");
  EXPECT_EQ(dev.get_type(), hw::device::device_type::SOFTWARE);
}

TEST(device_registry, default_device_name)
{
  hw::device& dev = hw::get_device("default");
  EXPECT_FALSE(dev.get_name().empty());
}

TEST(device_registry, unknown_device_throws)
{
  EXPECT_ANY_THROW(hw::get_device("nonexistent_device_xyz_12345"));
}

TEST(device_registry, default_device_is_singleton)
{
  hw::device& dev1 = hw::get_device("default");
  hw::device& dev2 = hw::get_device("default");
  EXPECT_EQ(&dev1, &dev2);
}

TEST(device_default, register_all_populates)
{
  std::map<std::string, std::unique_ptr<hw::device>> registry;
  hw::core::register_all(registry);
  EXPECT_FALSE(registry.empty());
  // Should at least have "default" device
  EXPECT_NE(registry.find("default"), registry.end());
  // Release pointers before map destruction to avoid destroying
  // global singleton devices (register_all wraps them in unique_ptr)
  for (auto &entry : registry)
    entry.second.release();
}

TEST(device_default, software_type)
{
  hw::core::device_default dev;
  EXPECT_EQ(dev.get_type(), hw::device::device_type::SOFTWARE);
}

TEST(device_default, device_protocol)
{
  hw::core::device_default dev;
  EXPECT_EQ(dev.device_protocol(), hw::device::PROTOCOL_DEFAULT);
}

#ifdef WITH_DEVICE_LEDGER
#include "device/device_ledger.hpp"

TEST(device_ledger, register_all_populates)
{
  std::map<std::string, std::unique_ptr<hw::device>> registry;
  hw::ledger::register_all(registry);
  EXPECT_FALSE(registry.empty());
  // Release to avoid destroying global singleton
  for (auto &entry : registry)
    entry.second.release();
}

TEST(device_ledger, version_macros)
{
  unsigned int v = VERSION(1, 8, 0);
  EXPECT_EQ(VERSION_MAJOR(v), 1);
  EXPECT_EQ(VERSION_MINOR(v), 8);
  EXPECT_EQ(VERSION_MICRO(v), 0);
}

TEST(device_ledger, version_macros_roundtrip)
{
  for (unsigned int major = 0; major < 5; ++major)
    for (unsigned int minor = 0; minor < 10; ++minor)
      for (unsigned int micro = 0; micro < 10; ++micro)
      {
        unsigned int v = VERSION(major, minor, micro);
        EXPECT_EQ(VERSION_MAJOR(v), major);
        EXPECT_EQ(VERSION_MINOR(v), minor);
        EXPECT_EQ(VERSION_MICRO(v), micro);
      }
}

TEST(device_ledger, minimal_app_version)
{
  EXPECT_EQ(MINIMAL_APP_VERSION_MAJOR, 1);
  EXPECT_GE(MINIMAL_APP_VERSION_MINOR, 0);
  EXPECT_GE(MINIMAL_APP_VERSION_MICRO, 0);
}

TEST(device_ledger, status_codes)
{
  EXPECT_EQ(SW_OK, 0x9000);
  EXPECT_EQ(SW_WRONG_LENGTH, 0x6700);
  EXPECT_EQ(SW_DENY, 0x6982);
  EXPECT_EQ(SW_WRONG_DATA, 0x6984);
  EXPECT_EQ(SW_INS_NOT_SUPPORTED, 0x6d00);
}

TEST(device_ledger, abpkeys_default_construct)
{
  hw::ledger::ABPkeys keys;
  EXPECT_EQ(keys.index, 0u);
  EXPECT_FALSE(keys.is_subaddress);
  EXPECT_FALSE(keys.is_change_address);
  EXPECT_FALSE(keys.additional_key);
}

TEST(device_ledger, keymap_empty)
{
  hw::ledger::Keymap km;
  hw::ledger::ABPkeys result;
  rct::key test_key = rct::identity();
  EXPECT_FALSE(km.find(test_key, result));
}

TEST(device_ledger, keymap_add_find)
{
  hw::ledger::Keymap km;
  hw::ledger::ABPkeys keys;
  keys.Pout = rct::skGen();
  km.add(keys);

  hw::ledger::ABPkeys result;
  EXPECT_TRUE(km.find(keys.Pout, result));
}

TEST(device_ledger, keymap_clear)
{
  hw::ledger::Keymap km;
  hw::ledger::ABPkeys keys;
  keys.Pout = rct::skGen();
  km.add(keys);
  km.clear();

  hw::ledger::ABPkeys result;
  EXPECT_FALSE(km.find(keys.Pout, result));
}

TEST(device_ledger, hmacmap_add_find)
{
  hw::ledger::HMACmap hm;
  uint8_t sec[32] = {1, 2, 3};
  uint8_t hmac[32] = {4, 5, 6};
  hm.add_mac(sec, hmac);

  uint8_t found_hmac[32] = {};
  hm.find_mac(sec, found_hmac);
  EXPECT_EQ(memcmp(found_hmac, hmac, 32), 0);
}

TEST(device_ledger, hmacmap_clear)
{
  hw::ledger::HMACmap hm;
  uint8_t sec[32] = {1, 2, 3};
  uint8_t hmac[32] = {4, 5, 6};
  hm.add_mac(sec, hmac);
  hm.clear();
  // After clear, hmacs vector should be empty
  EXPECT_TRUE(hm.hmacs.empty());
}

#endif // WITH_DEVICE_LEDGER
