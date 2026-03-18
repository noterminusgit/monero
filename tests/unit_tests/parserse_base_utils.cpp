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

#include "storages/parserse_base_utils.h"
#include <boost/utility/string_ref.hpp>

using namespace epee::misc_utils::parse;

// ---------- isspace / isdigit LUT tests ----------

TEST(parserse_base_utils, isspace_whitespace_chars)
{
  EXPECT_TRUE(isspace(' '));
  EXPECT_TRUE(isspace('\t'));
  EXPECT_TRUE(isspace('\n'));
  EXPECT_TRUE(isspace('\r'));
  EXPECT_TRUE(isspace('\v'));
  EXPECT_TRUE(isspace('\f'));
}

TEST(parserse_base_utils, isspace_non_whitespace)
{
  EXPECT_FALSE(isspace('a'));
  EXPECT_FALSE(isspace('0'));
  EXPECT_FALSE(isspace('{'));
  EXPECT_FALSE(isspace('\0'));
  EXPECT_FALSE(isspace('Z'));
}

TEST(parserse_base_utils, isdigit_digits)
{
  for (char c = '0'; c <= '9'; ++c)
    EXPECT_TRUE(isdigit(c));
}

TEST(parserse_base_utils, isdigit_non_digits)
{
  EXPECT_FALSE(isdigit('a'));
  EXPECT_FALSE(isdigit('Z'));
  EXPECT_FALSE(isdigit(' '));
  EXPECT_FALSE(isdigit('.'));
  EXPECT_FALSE(isdigit('-'));
}

// ---------- transform_to_escape_sequence ----------

TEST(parserse_base_utils, escape_no_special_chars)
{
  std::string input = "hello world 123";
  EXPECT_EQ(transform_to_escape_sequence(input), input);
}

TEST(parserse_base_utils, escape_newline)
{
  EXPECT_EQ(transform_to_escape_sequence("line1\nline2"), "line1\\nline2");
}

TEST(parserse_base_utils, escape_tab)
{
  EXPECT_EQ(transform_to_escape_sequence("col1\tcol2"), "col1\\tcol2");
}

TEST(parserse_base_utils, escape_backslash)
{
  EXPECT_EQ(transform_to_escape_sequence("path\\file"), "path\\\\file");
}

TEST(parserse_base_utils, escape_double_quote)
{
  EXPECT_EQ(transform_to_escape_sequence("say \"hi\""), "say \\\"hi\\\"");
}

TEST(parserse_base_utils, escape_slash)
{
  EXPECT_EQ(transform_to_escape_sequence("a/b"), "a\\/b");
}

TEST(parserse_base_utils, escape_carriage_return)
{
  EXPECT_EQ(transform_to_escape_sequence("a\rb"), "a\\rb");
}

TEST(parserse_base_utils, escape_backspace_formfeed)
{
  EXPECT_EQ(transform_to_escape_sequence("\b\f"), "\\b\\f");
}

TEST(parserse_base_utils, escape_vertical_tab)
{
  EXPECT_EQ(transform_to_escape_sequence("a\vb"), "a\\vb");
}

TEST(parserse_base_utils, escape_multiple_specials)
{
  EXPECT_EQ(transform_to_escape_sequence("\n\t\"\\"), "\\n\\t\\\"\\\\");
}

TEST(parserse_base_utils, escape_empty_string)
{
  EXPECT_EQ(transform_to_escape_sequence(""), "");
}

// ---------- match_string2 ----------

TEST(parserse_base_utils, match_string_simple)
{
  std::string input = "\"hello\"";
  auto it = input.cbegin();
  std::string val;
  match_string2(it, input.cend(), val);
  EXPECT_EQ(val, "hello");
  EXPECT_EQ(*it, '"');
}

TEST(parserse_base_utils, match_string_with_escape)
{
  std::string input = "\"hello\\nworld\"";
  auto it = input.cbegin();
  std::string val;
  match_string2(it, input.cend(), val);
  EXPECT_EQ(val, "hello\nworld");
}

TEST(parserse_base_utils, match_string_escaped_quote)
{
  std::string input = "\"say \\\"hi\\\"\"";
  auto it = input.cbegin();
  std::string val;
  match_string2(it, input.cend(), val);
  EXPECT_EQ(val, "say \"hi\"");
}

TEST(parserse_base_utils, match_string_escaped_backslash)
{
  std::string input = "\"a\\\\b\"";
  auto it = input.cbegin();
  std::string val;
  match_string2(it, input.cend(), val);
  EXPECT_EQ(val, "a\\b");
}

TEST(parserse_base_utils, match_string_unicode_ascii)
{
  // \u0041 = 'A'
  std::string input = "\"\\u0041\"";
  auto it = input.cbegin();
  std::string val;
  match_string2(it, input.cend(), val);
  EXPECT_EQ(val, "A");
}

TEST(parserse_base_utils, match_string_unicode_2byte)
{
  // \u00e9 = 'é' (UTF-8: 0xc3 0xa9)
  std::string input = "\"\\u00e9\"";
  auto it = input.cbegin();
  std::string val;
  match_string2(it, input.cend(), val);
  ASSERT_EQ(val.size(), 2u);
  EXPECT_EQ((unsigned char)val[0], 0xc3);
  EXPECT_EQ((unsigned char)val[1], 0xa9);
}

TEST(parserse_base_utils, match_string_unicode_3byte)
{
  // \u4e16 = '世' (UTF-8: 0xe4 0xb8 0x96)
  std::string input = "\"\\u4e16\"";
  auto it = input.cbegin();
  std::string val;
  match_string2(it, input.cend(), val);
  ASSERT_EQ(val.size(), 3u);
  EXPECT_EQ((unsigned char)val[0], 0xe4);
  EXPECT_EQ((unsigned char)val[1], 0xb8);
  EXPECT_EQ((unsigned char)val[2], 0x96);
}

TEST(parserse_base_utils, match_string_empty)
{
  std::string input = "\"\"";
  auto it = input.cbegin();
  std::string val;
  match_string2(it, input.cend(), val);
  EXPECT_TRUE(val.empty());
}

TEST(parserse_base_utils, match_string_unterminated_throws)
{
  std::string input = "\"no closing quote";
  auto it = input.cbegin();
  std::string val;
  EXPECT_ANY_THROW(match_string2(it, input.cend(), val));
}

// ---------- match_number2 ----------

TEST(parserse_base_utils, match_number_positive_int)
{
  std::string input = "12345,";
  auto it = input.cbegin();
  boost::string_ref val;
  bool is_float = false, is_signed = false;
  match_number2(it, input.cend(), val, is_float, is_signed);
  EXPECT_EQ(std::string(val.data(), val.size()), "12345");
  EXPECT_FALSE(is_float);
  EXPECT_FALSE(is_signed);
}

TEST(parserse_base_utils, match_number_negative_int)
{
  std::string input = "-42}";
  auto it = input.cbegin();
  boost::string_ref val;
  bool is_float = false, is_signed = false;
  match_number2(it, input.cend(), val, is_float, is_signed);
  EXPECT_EQ(std::string(val.data(), val.size()), "-42");
  EXPECT_FALSE(is_float);
  EXPECT_TRUE(is_signed);
}

TEST(parserse_base_utils, match_number_float)
{
  std::string input = "3.14,";
  auto it = input.cbegin();
  boost::string_ref val;
  bool is_float = false, is_signed = false;
  match_number2(it, input.cend(), val, is_float, is_signed);
  EXPECT_EQ(std::string(val.data(), val.size()), "3.14");
  EXPECT_TRUE(is_float);
  EXPECT_FALSE(is_signed);
}

TEST(parserse_base_utils, match_number_negative_float)
{
  std::string input = "-1.5}";
  auto it = input.cbegin();
  boost::string_ref val;
  bool is_float = false, is_signed = false;
  match_number2(it, input.cend(), val, is_float, is_signed);
  EXPECT_EQ(std::string(val.data(), val.size()), "-1.5");
  EXPECT_TRUE(is_float);
  EXPECT_TRUE(is_signed);
}

TEST(parserse_base_utils, match_number_zero)
{
  std::string input = "0,";
  auto it = input.cbegin();
  boost::string_ref val;
  bool is_float = false, is_signed = false;
  match_number2(it, input.cend(), val, is_float, is_signed);
  EXPECT_EQ(std::string(val.data(), val.size()), "0");
  EXPECT_FALSE(is_float);
  EXPECT_FALSE(is_signed);
}

// ---------- match_word2 ----------

TEST(parserse_base_utils, match_word_true)
{
  std::string input = "true,";
  auto it = input.cbegin();
  boost::string_ref val;
  match_word2(it, input.cend(), val);
  EXPECT_EQ(std::string(val.data(), val.size()), "true");
}

TEST(parserse_base_utils, match_word_false)
{
  std::string input = "false}";
  auto it = input.cbegin();
  boost::string_ref val;
  match_word2(it, input.cend(), val);
  EXPECT_EQ(std::string(val.data(), val.size()), "false");
}

TEST(parserse_base_utils, match_word_null)
{
  std::string input = "null ";
  auto it = input.cbegin();
  boost::string_ref val;
  match_word2(it, input.cend(), val);
  EXPECT_EQ(std::string(val.data(), val.size()), "null");
}

// ---------- isx hex lookup table ----------

TEST(parserse_base_utils, isx_hex_digits)
{
  EXPECT_EQ(isx[(unsigned char)'0'], 0);
  EXPECT_EQ(isx[(unsigned char)'9'], 9);
  EXPECT_EQ(isx[(unsigned char)'a'], 10);
  EXPECT_EQ(isx[(unsigned char)'f'], 15);
  EXPECT_EQ(isx[(unsigned char)'A'], 10);
  EXPECT_EQ(isx[(unsigned char)'F'], 15);
}

TEST(parserse_base_utils, isx_non_hex)
{
  EXPECT_EQ(isx[(unsigned char)'g'], 0xff);
  EXPECT_EQ(isx[(unsigned char)'G'], 0xff);
  EXPECT_EQ(isx[(unsigned char)' '], 0xff);
  EXPECT_EQ(isx[(unsigned char)'\0'], 0xff);
}

// ---------- LUT classification ----------

TEST(parserse_base_utils, lut_alpha_classification)
{
  // alpha flag = 4
  for (char c = 'a'; c <= 'z'; ++c)
    EXPECT_TRUE(lut[(uint8_t)c] & 4) << "char: " << c;
  for (char c = 'A'; c <= 'Z'; ++c)
    EXPECT_TRUE(lut[(uint8_t)c] & 4) << "char: " << c;
}

TEST(parserse_base_utils, lut_digit_classification)
{
  // digit flag = 1, and digits also have flag 16 (allowed in float)
  for (char c = '0'; c <= '9'; ++c)
  {
    EXPECT_TRUE(lut[(uint8_t)c] & 1) << "char: " << c;
    EXPECT_TRUE(lut[(uint8_t)c] & 16) << "char: " << c;
  }
}

TEST(parserse_base_utils, lut_special_chars)
{
  // backslash and double-quote have flag 32
  EXPECT_TRUE(lut[(uint8_t)'"'] & 32);
  EXPECT_TRUE(lut[(uint8_t)'\\'] & 32);
}

// ---------- Additional isspace/isdigit coverage ----------

TEST(parserse_base_utils, isspace_all_ascii_non_whitespace)
{
  int whitespace_count = 0;
  for (int c = 0; c < 128; ++c)
  {
    if (isspace(static_cast<char>(c)))
      ++whitespace_count;
  }
  // Standard whitespace: space, tab, newline, carriage return, vertical tab, form feed = 6
  EXPECT_EQ(whitespace_count, 6);
}

TEST(parserse_base_utils, isdigit_count)
{
  int digit_count = 0;
  for (int c = 0; c < 128; ++c)
  {
    if (isdigit(static_cast<char>(c)))
      ++digit_count;
  }
  EXPECT_EQ(digit_count, 10);
}

// ---------- Additional escape sequence tests ----------

TEST(parserse_base_utils, escape_all_special_chars_combined)
{
  std::string input = "\b\f\n\r\t\v\\\"/";
  std::string result = transform_to_escape_sequence(input);
  EXPECT_NE(result.find("\\b"), std::string::npos);
  EXPECT_NE(result.find("\\f"), std::string::npos);
  EXPECT_NE(result.find("\\n"), std::string::npos);
  EXPECT_NE(result.find("\\r"), std::string::npos);
  EXPECT_NE(result.find("\\t"), std::string::npos);
  EXPECT_NE(result.find("\\v"), std::string::npos);
  EXPECT_NE(result.find("\\\\"), std::string::npos);
}

TEST(parserse_base_utils, escape_long_string)
{
  std::string input(10000, 'a');
  std::string result = transform_to_escape_sequence(input);
  EXPECT_EQ(result, input); // No special chars, should be identical
}

TEST(parserse_base_utils, escape_only_special_chars)
{
  std::string input = "\n\n\n";
  std::string result = transform_to_escape_sequence(input);
  EXPECT_EQ(result, "\\n\\n\\n");
}

// ---------- Additional match_string2 tests ----------

TEST(parserse_base_utils, match_string_all_escaped_chars)
{
  std::string input = "\"\\b\\f\\n\\r\\t\\\\\\/\\\"\"";
  auto it = input.cbegin();
  std::string val;
  match_string2(it, input.cend(), val);
  EXPECT_NE(val.find('\b'), std::string::npos);
  EXPECT_NE(val.find('\n'), std::string::npos);
  EXPECT_NE(val.find('\\'), std::string::npos);
}

TEST(parserse_base_utils, match_string_long_content)
{
  std::string content(5000, 'x');
  std::string input = "\"" + content + "\"";
  auto it = input.cbegin();
  std::string val;
  match_string2(it, input.cend(), val);
  EXPECT_EQ(val.size(), 5000u);
}

TEST(parserse_base_utils, match_string_single_char)
{
  std::string input = "\"a\"";
  auto it = input.cbegin();
  std::string val;
  match_string2(it, input.cend(), val);
  EXPECT_EQ(val, "a");
}

TEST(parserse_base_utils, match_string_unicode_null)
{
  // \u0000 = null character
  std::string input = "\"\\u0000\"";
  auto it = input.cbegin();
  std::string val;
  match_string2(it, input.cend(), val);
  EXPECT_EQ(val.size(), 1u);
  EXPECT_EQ(val[0], '\0');
}

// ---------- Additional match_number2 tests ----------

TEST(parserse_base_utils, match_number_large_int)
{
  std::string input = "18446744073709551615,"; // UINT64_MAX
  auto it = input.cbegin();
  boost::string_ref val;
  bool is_float = false, is_signed = false;
  match_number2(it, input.cend(), val, is_float, is_signed);
  EXPECT_EQ(std::string(val.data(), val.size()), "18446744073709551615");
  EXPECT_FALSE(is_float);
}

TEST(parserse_base_utils, match_number_scientific_notation)
{
  std::string input = "1e10,";
  auto it = input.cbegin();
  boost::string_ref val;
  bool is_float = false, is_signed = false;
  match_number2(it, input.cend(), val, is_float, is_signed);
  std::string valstr(val.data(), val.size());
  // Should parse at least the '1' part
  EXPECT_FALSE(valstr.empty());
}

TEST(parserse_base_utils, match_number_negative_zero)
{
  std::string input = "-0,";
  auto it = input.cbegin();
  boost::string_ref val;
  bool is_float = false, is_signed = false;
  match_number2(it, input.cend(), val, is_float, is_signed);
  EXPECT_EQ(std::string(val.data(), val.size()), "-0");
  EXPECT_TRUE(is_signed);
}

TEST(parserse_base_utils, match_number_decimal_only)
{
  std::string input = "0.0,";
  auto it = input.cbegin();
  boost::string_ref val;
  bool is_float = false, is_signed = false;
  match_number2(it, input.cend(), val, is_float, is_signed);
  EXPECT_TRUE(is_float);
}

// ---------- Additional match_word2 tests ----------

TEST(parserse_base_utils, match_word_mixed_case)
{
  std::string input = "True,";
  auto it = input.cbegin();
  boost::string_ref val;
  match_word2(it, input.cend(), val);
  EXPECT_EQ(std::string(val.data(), val.size()), "True");
}

TEST(parserse_base_utils, match_word_identifier)
{
  // match_word2 only matches alpha characters (lut flag 4),
  // so digits stop the match. "myVar123" matches only "myVar".
  std::string input = "myVar123 ";
  auto it = input.cbegin();
  boost::string_ref val;
  match_word2(it, input.cend(), val);
  EXPECT_EQ(std::string(val.data(), val.size()), "myVar");
}

// ---------- Additional hex lookup tests ----------

TEST(parserse_base_utils, isx_all_hex_chars)
{
  // 0-9
  for (char c = '0'; c <= '9'; ++c)
    EXPECT_EQ(isx[(unsigned char)c], c - '0');
  // a-f
  for (char c = 'a'; c <= 'f'; ++c)
    EXPECT_EQ(isx[(unsigned char)c], 10 + c - 'a');
  // A-F
  for (char c = 'A'; c <= 'F'; ++c)
    EXPECT_EQ(isx[(unsigned char)c], 10 + c - 'A');
}

TEST(parserse_base_utils, isx_boundary_values)
{
  EXPECT_EQ(isx[(unsigned char)'0'], 0);
  EXPECT_EQ(isx[(unsigned char)'9'], 9);
  EXPECT_EQ(isx[(unsigned char)'a'], 10);
  EXPECT_EQ(isx[(unsigned char)'f'], 15);
  EXPECT_NE(isx[(unsigned char)'g'], 16); // g is not hex
}

// ---------- LUT additional tests ----------

TEST(parserse_base_utils, lut_underscore)
{
  // underscore should have some flags (it's allowed in identifiers)
  (void)lut[(uint8_t)'_']; // Just verify no crash
}

TEST(parserse_base_utils, lut_whitespace_flags)
{
  // whitespace flag = 8 (per the LUT definition in parserse_base_utils.h)
  EXPECT_TRUE(lut[(uint8_t)' '] & 8);
  EXPECT_TRUE(lut[(uint8_t)'\t'] & 8);
  EXPECT_TRUE(lut[(uint8_t)'\n'] & 8);
  EXPECT_TRUE(lut[(uint8_t)'\r'] & 8);
}

TEST(parserse_base_utils, lut_zero_byte)
{
  // NUL byte should have no meaningful flags
  uint8_t flags = lut[0];
  EXPECT_FALSE(flags & 1); // not a digit
  EXPECT_FALSE(flags & 4); // not alpha
}
