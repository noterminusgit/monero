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
#include "wipeable_string.h"
#include "mnemonics/language_base.h"
#include "mnemonics/electrum-words.h"
#include "crypto/crypto.h"
#include <stdlib.h>
#include <vector>
#include <time.h>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include "mnemonics/chinese_simplified.h"
#include "mnemonics/english.h"
#include "mnemonics/spanish.h"
#include "mnemonics/portuguese.h"
#include "mnemonics/japanese.h"
#include "mnemonics/german.h"
#include "mnemonics/italian.h"
#include "mnemonics/russian.h"
#include "mnemonics/french.h"
#include "mnemonics/dutch.h"
#include "mnemonics/esperanto.h"
#include "mnemonics/lojban.h"
#include "mnemonics/english_old.h"
#include "mnemonics/singleton.h"

namespace
{
  /*!
   * \brief Compares vectors for equality
   * \param expected expected vector
   * \param present  current vector
   */
  void compare_vectors(const std::vector<std::string> &expected, const std::vector<std::string> &present)
  {
    std::vector<std::string>::const_iterator it1, it2;
    for (it1 = expected.begin(), it2 = present.begin(); it1 != expected.end() && it2 != present.end();
      it1++, it2++)
    {
      ASSERT_STREQ(it1->c_str(), it2->c_str());
    }
  }

  /*!
   * \brief Tests the given language mnemonics.
   * \param language A Language instance to test
   */
  void test_language(const Language::Base &language)
  {
    epee::wipeable_string w_seed = "", w_return_seed = "";
    std::string seed, return_seed;
    // Generate a random seed without checksum
    crypto::secret_key randkey;
    for (size_t ii = 0; ii < sizeof(randkey); ++ii)
    {
      randkey.data[ii] = crypto::rand<uint8_t>();
    }
    crypto::ElectrumWords::bytes_to_words(randkey, w_seed, language.get_language_name());
    seed = std::string(w_seed.data(), w_seed.size());
    // remove the checksum word
    const char *space = strrchr(seed.c_str(), ' ');
    ASSERT_TRUE(space != NULL);
    seed = std::string(seed.c_str(), space-seed.c_str());

    std::cout << "Test seed without checksum:\n";
    std::cout << seed << std::endl;

    crypto::secret_key key;
    std::string language_name;
    bool res;
    std::vector<std::string> seed_vector, return_seed_vector;
    std::string checksum_word;

    // Convert it to secret key
    res = crypto::ElectrumWords::words_to_bytes(seed, key, language_name);
    ASSERT_EQ(true, res);
    std::cout << "Detected language: " << language_name << std::endl;
    ASSERT_STREQ(language.get_language_name().c_str(), language_name.c_str());

    // Convert the secret key back to seed
    crypto::ElectrumWords::bytes_to_words(key, w_return_seed, language.get_language_name());
    return_seed = std::string(w_return_seed.data(), w_return_seed.size());
    ASSERT_EQ(true, res);
    std::cout << "Returned seed:\n";
    std::cout << return_seed << std::endl;
    boost::split(seed_vector, seed, boost::is_any_of(" "));
    boost::split(return_seed_vector, return_seed, boost::is_any_of(" "));

    // Extract the checksum word
    checksum_word = return_seed_vector.back();
    return_seed_vector.pop_back();
    ASSERT_EQ(seed_vector.size(), return_seed_vector.size());
    // Ensure that the rest of it is same
    compare_vectors(seed_vector, return_seed_vector);

    // Append the checksum word to repeat the entire process with a seed with checksum
    seed += (" " + checksum_word);
    std::cout << "Test seed with checksum:\n";
    std::cout << seed << std::endl;
    res = crypto::ElectrumWords::words_to_bytes(seed, key, language_name);
    ASSERT_EQ(true, res);
    std::cout << "Detected language: " << language_name << std::endl;
    ASSERT_STREQ(language.get_language_name().c_str(), language_name.c_str());

    w_return_seed = "";
    crypto::ElectrumWords::bytes_to_words(key, w_return_seed, language.get_language_name());
    return_seed = std::string(w_return_seed.data(), w_return_seed.size());
    ASSERT_EQ(true, res);
    std::cout << "Returned seed:\n";
    std::cout << return_seed << std::endl;

    seed_vector.clear();
    return_seed_vector.clear();
    boost::split(seed_vector, seed, boost::is_any_of(" "));
    boost::split(return_seed_vector, return_seed, boost::is_any_of(" "));
    ASSERT_EQ(seed_vector.size(), return_seed_vector.size());
    compare_vectors(seed_vector, return_seed_vector);
  }
}

TEST(mnemonics, consistency)
{
  try {
    std::vector<std::string> language_list;
    crypto::ElectrumWords::get_language_list(language_list);
  }
  catch(const std::exception &e)
  {
    std::cout << "Error initializing mnemonics: " << e.what() << std::endl;
    ASSERT_TRUE(false);
  }
}

TEST(mnemonics, all_languages)
{
  srand(time(NULL));
  std::vector<Language::Base*> languages({
    Language::Singleton<Language::Chinese_Simplified>::instance(),
    Language::Singleton<Language::English>::instance(),
    Language::Singleton<Language::Spanish>::instance(),
    Language::Singleton<Language::Portuguese>::instance(),
    Language::Singleton<Language::Japanese>::instance(),
    Language::Singleton<Language::German>::instance(),
    Language::Singleton<Language::Italian>::instance(),
    Language::Singleton<Language::Russian>::instance(),
    Language::Singleton<Language::French>::instance(),
    Language::Singleton<Language::Dutch>::instance(),
    Language::Singleton<Language::Esperanto>::instance(),
    Language::Singleton<Language::Lojban>::instance()
  });

  for (std::vector<Language::Base*>::iterator it = languages.begin(); it != languages.end(); it++)
  {
    try {
      test_language(*(*it));
    }
    catch (const std::exception &e) {
      std::cout << "Error testing " << (*it)->get_language_name() << " language: " << e.what() << std::endl;
      ASSERT_TRUE(false);
    }
  }
}

TEST(mnemonics, language_detection_with_bad_checksum)
{
    crypto::secret_key key;
    std::string language_name;
    bool res;

    // This Portuguese (4-prefix) seed has all its words with 3-prefix that's also present in English
    const std::string base_seed = "cinzento luxuriante leonardo gnostico digressao cupula fifa broxar iniquo louvor ovario dorsal ideologo besuntar decurso rosto susto lemure unheiro pagodeiro nitroglicerina eclusa mazurca bigorna";
    const std::string real_checksum = "gnostico";

    res = crypto::ElectrumWords::words_to_bytes(base_seed, key, language_name);
    ASSERT_EQ(true, res);
    ASSERT_STREQ(language_name.c_str(), "Português");

    res = crypto::ElectrumWords::words_to_bytes(base_seed + " " + real_checksum, key, language_name);
    ASSERT_EQ(true, res);
    ASSERT_STREQ(language_name.c_str(), "Português");
}

TEST(mnemonics, utf8prefix)
{
  ASSERT_TRUE(Language::utf8prefix(epee::wipeable_string("foo"), 0) == "");
  ASSERT_TRUE(Language::utf8prefix(epee::wipeable_string("foo"), 1) == "f");
  ASSERT_TRUE(Language::utf8prefix(epee::wipeable_string("foo"), 2) == "fo");
  ASSERT_TRUE(Language::utf8prefix(epee::wipeable_string("foo"), 3) == "foo");
  ASSERT_TRUE(Language::utf8prefix(epee::wipeable_string("foo"), 4) == "foo");
  ASSERT_TRUE(Language::utf8prefix(epee::wipeable_string("æon"), 0) == "");
  ASSERT_TRUE(Language::utf8prefix(epee::wipeable_string("æon"), 1) == "æ");
  ASSERT_TRUE(Language::utf8prefix(epee::wipeable_string("æon"), 2) == "æo");
  ASSERT_TRUE(Language::utf8prefix(epee::wipeable_string("æon"), 3) == "æon");
  ASSERT_TRUE(Language::utf8prefix(epee::wipeable_string("æon"), 4) == "æon");
}

TEST(mnemonics, case_tolerance)
{
    bool res;
    //
    crypto::secret_key key_1;
    std::string language_name_1;
    const std::string seed_1 = "Neubau umarmen Abart umarmen Turban feilen Brett Bargeld Episode Milchkuh Substanz Jahr Armband Maibaum Tand Grünalge Tabak erziehen Federboa Lobrede Tenor Leuchter Curry Diskurs Tenor";
    res = crypto::ElectrumWords::words_to_bytes(seed_1, key_1, language_name_1);
    ASSERT_EQ(true, res);
    ASSERT_STREQ(language_name_1.c_str(), "Deutsch");
    //
    crypto::secret_key key_2;
    std::string language_name_2;
    // neubau is capitalized in the word list, but the language detection code should be able to detect it as Deutsch
    std::string seed_2 = "neubau umarmen Abart umarmen Turban feilen Brett Bargeld Episode Milchkuh Substanz Jahr Armband Maibaum Tand Grünalge Tabak erziehen Federboa Lobrede Tenor Leuchter Curry Diskurs tenor";
    boost::algorithm::to_lower(seed_2);
    res = crypto::ElectrumWords::words_to_bytes(seed_2, key_2, language_name_2);
    ASSERT_EQ(true, res);
    ASSERT_STREQ(language_name_2.c_str(), "Deutsch");
    //
    ASSERT_TRUE(key_1 == key_2);
}

TEST(mnemonics, partial_word_tolerance)
{
    bool res;
    //
    crypto::secret_key key_1;
    std::string language_name_1;
    const std::string seed_1 = "crim bam scamp gna limi woma wron tuit birth mundane donuts square cohesive dolphin titans narrate fue saved wrap aloof magic mirr toget upda wra";
    res = crypto::ElectrumWords::words_to_bytes(seed_1, key_1, language_name_1);
    ASSERT_EQ(true, res);
    ASSERT_STREQ(language_name_1.c_str(), "English");
}

TEST(mnemonics, get_language_list_non_empty)
{
    std::vector<std::string> languages;
    crypto::ElectrumWords::get_language_list(languages);
    ASSERT_FALSE(languages.empty());
    // Should have at least 12 languages (all the ones registered)
    ASSERT_GE(languages.size(), 12u);
}

TEST(mnemonics, get_language_list_english_names)
{
    std::vector<std::string> native_names;
    crypto::ElectrumWords::get_language_list(native_names, false);
    std::vector<std::string> english_names;
    crypto::ElectrumWords::get_language_list(english_names, true);
    ASSERT_EQ(native_names.size(), english_names.size());
    // The English entry should have the same native and English name
    bool found_english = false;
    for (size_t i = 0; i < english_names.size(); ++i)
    {
        if (english_names[i] == "English")
        {
            ASSERT_EQ(native_names[i], "English");
            found_english = true;
        }
    }
    ASSERT_TRUE(found_english);
}

TEST(mnemonics, is_valid_language_native_names)
{
    // Valid native language names
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("English"));
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("Deutsch"));
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("Español"));
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("Français"));
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("Italiano"));
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("Nederlands"));
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("Português"));
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("Esperanto"));
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("Lojban"));
}

TEST(mnemonics, is_valid_language_english_names)
{
    // Valid English language names
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("German"));
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("Spanish"));
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("French"));
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("Italian"));
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("Dutch"));
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("Portuguese"));
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("Russian"));
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("Japanese"));
    ASSERT_TRUE(crypto::ElectrumWords::is_valid_language("Chinese (simplified)"));
}

TEST(mnemonics, is_valid_language_invalid)
{
    ASSERT_FALSE(crypto::ElectrumWords::is_valid_language(""));
    ASSERT_FALSE(crypto::ElectrumWords::is_valid_language("Klingon"));
    ASSERT_FALSE(crypto::ElectrumWords::is_valid_language("english")); // case sensitive
    ASSERT_FALSE(crypto::ElectrumWords::is_valid_language("ENGLISH"));
    ASSERT_FALSE(crypto::ElectrumWords::is_valid_language("Latin"));
}

TEST(mnemonics, get_english_name_for_known_languages)
{
    ASSERT_EQ(crypto::ElectrumWords::get_english_name_for("Deutsch"), "German");
    ASSERT_EQ(crypto::ElectrumWords::get_english_name_for("English"), "English");
    ASSERT_EQ(crypto::ElectrumWords::get_english_name_for("Español"), "Spanish");
    ASSERT_EQ(crypto::ElectrumWords::get_english_name_for("Français"), "French");
    ASSERT_EQ(crypto::ElectrumWords::get_english_name_for("Italiano"), "Italian");
    ASSERT_EQ(crypto::ElectrumWords::get_english_name_for("Nederlands"), "Dutch");
    ASSERT_EQ(crypto::ElectrumWords::get_english_name_for("Português"), "Portuguese");
    ASSERT_EQ(crypto::ElectrumWords::get_english_name_for("Esperanto"), "Esperanto");
    ASSERT_EQ(crypto::ElectrumWords::get_english_name_for("Lojban"), "Lojban");
}

TEST(mnemonics, get_english_name_for_unknown_language)
{
    ASSERT_EQ(crypto::ElectrumWords::get_english_name_for("Klingon"), "<language not found>");
    ASSERT_EQ(crypto::ElectrumWords::get_english_name_for(""), "<language not found>");
}

TEST(mnemonics, get_is_old_style_seed_new_style)
{
    // A new style seed has exactly 25 words (24 + 1 checksum).
    // Generate a proper 25-word seed
    crypto::secret_key randkey;
    for (size_t i = 0; i < sizeof(randkey); ++i)
        randkey.data[i] = crypto::rand<uint8_t>();

    epee::wipeable_string words;
    crypto::ElectrumWords::bytes_to_words(randkey, words, "English");
    // A new seed should have 25 words
    ASSERT_FALSE(crypto::ElectrumWords::get_is_old_style_seed(words));
}

TEST(mnemonics, get_is_old_style_seed_old_style)
{
    // Old style seeds have != 25 words (e.g., 24 words without checksum, or 13 words)
    // A 24-word seed (no checksum) is considered old style
    epee::wipeable_string old_seed("word1 word2 word3 word4 word5 word6 word7 word8 word9 word10 word11 word12 word13 word14 word15 word16 word17 word18 word19 word20 word21 word22 word23 word24");
    ASSERT_TRUE(crypto::ElectrumWords::get_is_old_style_seed(old_seed));

    // A 13-word seed is also old style
    epee::wipeable_string short_seed("word1 word2 word3 word4 word5 word6 word7 word8 word9 word10 word11 word12 word13");
    ASSERT_TRUE(crypto::ElectrumWords::get_is_old_style_seed(short_seed));
}

TEST(mnemonics, roundtrip_known_key_english)
{
    // Create a deterministic key and verify roundtrip
    crypto::secret_key key1;
    memset(key1.data, 0, sizeof(key1));
    key1.data[0] = 0x01; // minimal non-zero key

    epee::wipeable_string words;
    bool res = crypto::ElectrumWords::bytes_to_words(key1, words, "English");
    ASSERT_TRUE(res);

    // Words should not be empty
    std::string words_str(words.data(), words.size());
    ASSERT_FALSE(words_str.empty());

    // Convert back
    crypto::secret_key key2;
    std::string language_name;
    res = crypto::ElectrumWords::words_to_bytes(words_str, key2, language_name);
    ASSERT_TRUE(res);
    ASSERT_STREQ(language_name.c_str(), "English");
    ASSERT_TRUE(key1 == key2);
}

TEST(mnemonics, roundtrip_known_key_german)
{
    crypto::secret_key key1;
    for (size_t i = 0; i < sizeof(key1); ++i)
        key1.data[i] = static_cast<char>(i);

    epee::wipeable_string words;
    bool res = crypto::ElectrumWords::bytes_to_words(key1, words, "Deutsch");
    ASSERT_TRUE(res);

    crypto::secret_key key2;
    std::string language_name;
    res = crypto::ElectrumWords::words_to_bytes(
        std::string(words.data(), words.size()), key2, language_name);
    ASSERT_TRUE(res);
    ASSERT_STREQ(language_name.c_str(), "Deutsch");
    ASSERT_TRUE(key1 == key2);
}

TEST(mnemonics, words_to_bytes_empty_string_fails)
{
    crypto::secret_key key;
    std::string language_name;
    ASSERT_FALSE(crypto::ElectrumWords::words_to_bytes("", key, language_name));
}

TEST(mnemonics, words_to_bytes_garbage_fails)
{
    crypto::secret_key key;
    std::string language_name;
    ASSERT_FALSE(crypto::ElectrumWords::words_to_bytes("not real words at all here foo bar baz", key, language_name));
}

TEST(mnemonics, words_to_bytes_wrong_word_count_fails)
{
    crypto::secret_key key;
    std::string language_name;
    // Only 2 words, not a valid seed
    ASSERT_FALSE(crypto::ElectrumWords::words_to_bytes("abbey abducts", key, language_name));
}

TEST(mnemonics, bytes_to_words_invalid_language_fails)
{
    crypto::secret_key key;
    memset(key.data, 0x42, sizeof(key));
    epee::wipeable_string words;
    bool res = crypto::ElectrumWords::bytes_to_words(key, words, "Klingon");
    ASSERT_FALSE(res);
}

TEST(mnemonics, seed_length_constant)
{
    ASSERT_EQ(crypto::ElectrumWords::seed_length, 24);
}

TEST(mnemonics, old_language_name_constant)
{
    ASSERT_EQ(crypto::ElectrumWords::old_language_name, "EnglishOld");
}
