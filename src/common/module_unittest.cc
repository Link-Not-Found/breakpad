// Copyright 2010 Google LLC
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google LLC nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Original author: Jim Blandy <jimb@mozilla.com> <jimb@red-bean.com>

// module_unittest.cc: Unit tests for google_breakpad::Module.

#ifdef HAVE_CONFIG_H
#include <config.h>  // Must come first
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "breakpad_googletest_includes.h"
#include "common/module.h"
#include "common/using_std_string.h"

using google_breakpad::Module;
using google_breakpad::StringView;
using std::stringstream;
using std::vector;
using testing::ContainerEq;

static Module::Function* generate_duplicate_function(StringView name) {
  const Module::Address DUP_ADDRESS = 0xd35402aac7a7ad5cULL;
  const Module::Address DUP_SIZE = 0x200b26e605f99071ULL;
  const Module::Address DUP_PARAMETER_SIZE = 0xf14ac4fed48c4a99ULL;

  Module::Function* function = new Module::Function(name, DUP_ADDRESS);
  Module::Range range(DUP_ADDRESS, DUP_SIZE);
  function->ranges.push_back(range);
  function->parameter_size = DUP_PARAMETER_SIZE;
  return function;
}

#define MODULE_NAME "name with spaces"
#define MODULE_OS "os-name"
#define MODULE_ARCH "architecture"
#define MODULE_ID "id-string"
#define MODULE_CODE_ID "code-id-string"

TEST(Module, WriteHeader) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);
  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();
  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n",
               contents.c_str());
}

TEST(Module, WriteHeaderCodeId) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID, MODULE_CODE_ID);
  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();
  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n"
               "INFO CODE_ID code-id-string\n",
               contents.c_str());
}

TEST(Module, WriteOneLineFunc) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);

  Module::File* file = m.FindFile("file_name.cc");
  Module::Function* function = new Module::Function(
      "function_name", 0xe165bf8023b9d9abULL);
  Module::Range range(0xe165bf8023b9d9abULL, 0x1e4bb0eb1cbf5b09ULL);
  function->ranges.push_back(range);
  function->parameter_size = 0x772beee89114358aULL;
  Module::Line line = { 0xe165bf8023b9d9abULL, 0x1e4bb0eb1cbf5b09ULL,
                        file, 67519080 };
  function->lines.push_back(line);
  m.AddFunction(function);

  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();
  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n"
               "FILE 0 file_name.cc\n"
               "FUNC e165bf8023b9d9ab 1e4bb0eb1cbf5b09 772beee89114358a"
               " function_name\n"
               "e165bf8023b9d9ab 1e4bb0eb1cbf5b09 67519080 0\n",
               contents.c_str());
}

TEST(Module, WriteRelativeLoadAddress) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);

  // Some source files.  We will expect to see them in lexicographic order.
  Module::File* file1 = m.FindFile("filename-b.cc");
  Module::File* file2 = m.FindFile("filename-a.cc");

  // A function.
  Module::Function* function = new Module::Function(
      "A_FLIBBERTIJIBBET::a_will_o_the_wisp(a clown)", 0xbec774ea5dd935f3ULL);
  Module::Range range(0xbec774ea5dd935f3ULL, 0x2922088f98d3f6fcULL);
  function->ranges.push_back(range);
  function->parameter_size = 0xe5e9aa008bd5f0d0ULL;

  // Some source lines.  The module should not sort these.
  Module::Line line1 = { 0xbec774ea5dd935f3ULL, 0x1c2be6d6c5af2611ULL,
                         file1, 41676901 };
  Module::Line line2 = { 0xdaf35bc123885c04ULL, 0xcf621b8d324d0ebULL,
                         file2, 67519080 };
  function->lines.push_back(line2);
  function->lines.push_back(line1);

  m.AddFunction(function);

  // Some stack information.
  auto entry = std::make_unique<Module::StackFrameEntry>();
  entry->address = 0x30f9e5c83323973dULL;
  entry->size = 0x49fc9ca7c7c13dc2ULL;
  entry->initial_rules[".cfa"] = "he was a handsome man";
  entry->initial_rules["and"] = "what i want to know is";
  entry->rule_changes[0x30f9e5c83323973eULL]["how"] =
    "do you like your blueeyed boy";
  entry->rule_changes[0x30f9e5c83323973eULL]["Mister"] = "Death";
  m.AddStackFrameEntry(std::move(entry));

  // Set the load address.  Doing this after adding all the data to
  // the module must work fine.
  m.SetLoadAddress(0x2ab698b0b6407073ULL);

  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();
  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n"
               "FILE 0 filename-a.cc\n"
               "FILE 1 filename-b.cc\n"
               "FUNC 9410dc39a798c580 2922088f98d3f6fc e5e9aa008bd5f0d0"
               " A_FLIBBERTIJIBBET::a_will_o_the_wisp(a clown)\n"
               "b03cc3106d47eb91 cf621b8d324d0eb 67519080 0\n"
               "9410dc39a798c580 1c2be6d6c5af2611 41676901 1\n"
               "STACK CFI INIT 6434d177ce326ca 49fc9ca7c7c13dc2"
               " .cfa: he was a handsome man"
               " and: what i want to know is\n"
               "STACK CFI 6434d177ce326cb"
               " Mister: Death"
               " how: do you like your blueeyed boy\n",
               contents.c_str());
}

TEST(Module, WritePreserveLoadAddress) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);
  // Set the load address to something. Doesn't matter what.
  // The goal of this test is to demonstrate that the load
  // address does not impact any of the generated addresses
  // when the preserve_load_address option is equal to true.
  m.SetLoadAddress(0x1337ULL);

  Module::File* file = m.FindFile("filename-a.cc");
  Module::Function* function = new Module::Function(
      "do_stuff", 0x110ULL);
  Module::Range range(0x110ULL, 0x210ULL);
  function->ranges.push_back(range);
  function->parameter_size = 0x50ULL;
  Module::Line line1 = { 0x110ULL, 0x1ULL,
                         file, 20ULL };
  function->lines.push_back(line1);
  m.AddFunction(function);

  // Some stack information.
  auto entry = std::make_unique<Module::StackFrameEntry>();
  entry->address = 0x200ULL;
  entry->size = 0x55ULL;
  entry->initial_rules[".cfa"] = "some call frame info";
  entry->rule_changes[0x201ULL][".s0"] =
    "some rules change call frame info";
  m.AddStackFrameEntry(std::move(entry));

  bool preserve_load_address = true;
  m.Write(s, ALL_SYMBOL_DATA, preserve_load_address);
  string contents = s.str();
  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n"
               "FILE 0 filename-a.cc\n"
               "FUNC 110 210 50 do_stuff\n"
               "110 1 20 0\n"
               "STACK CFI INIT 200 55"
               " .cfa: some call frame info\n"
               "STACK CFI 201"
               " .s0: some rules change call frame info\n",
               contents.c_str());
}

TEST(Module, WriteOmitUnusedFiles) {
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);

  // Create some source files.
  Module::File* file1 = m.FindFile("filename1");
  m.FindFile("filename2");  // not used by any line
  Module::File* file3 = m.FindFile("filename3");

  // Create a function.
  Module::Function* function = new Module::Function(
      "function_name", 0x9b926d464f0b9384ULL);
  Module::Range range(0x9b926d464f0b9384ULL, 0x4f524a4ba795e6a6ULL);
  function->ranges.push_back(range);
  function->parameter_size = 0xbbe8133a6641c9b7ULL;

  // Source files that refer to some files, but not others.
  Module::Line line1 = { 0xab415089485e1a20ULL, 0x126e3124979291f2ULL,
                         file1, 137850127 };
  Module::Line line2 = { 0xb2675b5c3c2ed33fULL, 0x1df77f5551dbd68cULL,
                         file3, 28113549 };
  function->lines.push_back(line1);
  function->lines.push_back(line2);
  m.AddFunction(function);
  m.AssignSourceIds();

  vector<Module::File*> vec;
  m.GetFiles(&vec);
  EXPECT_EQ((size_t) 3, vec.size());
  EXPECT_STREQ("filename1", vec[0]->name.c_str());
  EXPECT_NE(-1, vec[0]->source_id);
  // Expect filename2 not to be used.
  EXPECT_STREQ("filename2", vec[1]->name.c_str());
  EXPECT_EQ(-1, vec[1]->source_id);
  EXPECT_STREQ("filename3", vec[2]->name.c_str());
  EXPECT_NE(-1, vec[2]->source_id);

  stringstream s;
  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();
  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n"
               "FILE 0 filename1\n"
               "FILE 1 filename3\n"
               "FUNC 9b926d464f0b9384 4f524a4ba795e6a6 bbe8133a6641c9b7"
               " function_name\n"
               "ab415089485e1a20 126e3124979291f2 137850127 0\n"
               "b2675b5c3c2ed33f 1df77f5551dbd68c 28113549 1\n",
               contents.c_str());
}

TEST(Module, WriteNoCFI) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);

  // Some source files.  We will expect to see them in lexicographic order.
  Module::File* file1 = m.FindFile("filename.cc");

  // A function.
  Module::Function* function = new Module::Function(
      "A_FLIBBERTIJIBBET::a_will_o_the_wisp(a clown)", 0xbec774ea5dd935f3ULL);
  Module::Range range(0xbec774ea5dd935f3ULL, 0x2922088f98d3f6fcULL);
  function->ranges.push_back(range);
  function->parameter_size = 0xe5e9aa008bd5f0d0ULL;

  // Some source lines.  The module should not sort these.
  Module::Line line1 = { 0xbec774ea5dd935f3ULL, 0x1c2be6d6c5af2611ULL,
                         file1, 41676901 };
  function->lines.push_back(line1);

  m.AddFunction(function);

  // Some stack information.
  auto entry = std::make_unique<Module::StackFrameEntry>();
  entry->address = 0x30f9e5c83323973dULL;
  entry->size = 0x49fc9ca7c7c13dc2ULL;
  entry->initial_rules[".cfa"] = "he was a handsome man";
  entry->initial_rules["and"] = "what i want to know is";
  entry->rule_changes[0x30f9e5c83323973eULL]["how"] =
    "do you like your blueeyed boy";
  entry->rule_changes[0x30f9e5c83323973eULL]["Mister"] = "Death";
  m.AddStackFrameEntry(std::move(entry));

  // Set the load address.  Doing this after adding all the data to
  // the module must work fine.
  m.SetLoadAddress(0x2ab698b0b6407073ULL);

  m.Write(s, SYMBOLS_AND_FILES | INLINES);
  string contents = s.str();
  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n"
               "FILE 0 filename.cc\n"
               "FUNC 9410dc39a798c580 2922088f98d3f6fc e5e9aa008bd5f0d0"
               " A_FLIBBERTIJIBBET::a_will_o_the_wisp(a clown)\n"
               "9410dc39a798c580 1c2be6d6c5af2611 41676901 0\n",
               contents.c_str());
}

TEST(Module, ConstructAddFunction) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);

  // Two functions.
  Module::Function* function1 = new Module::Function(
      "_without_form", 0xd35024aa7ca7da5cULL);
  Module::Range r1(0xd35024aa7ca7da5cULL, 0x200b26e605f99071ULL);
  function1->ranges.push_back(r1);
  function1->parameter_size = 0xf14ac4fed48c4a99ULL;

  Module::Function* function2 = new Module::Function(
      "_and_void", 0x2987743d0b35b13fULL);
  Module::Range r2(0x2987743d0b35b13fULL, 0xb369db048deb3010ULL);
  function2->ranges.push_back(r2);
  function2->parameter_size = 0x938e556cb5a79988ULL;

  // Put them in a vector.
  vector<Module::Function*> vec;
  vec.push_back(function1);
  vec.push_back(function2);

  for (Module::Function* func: vec)
    m.AddFunction(func);

  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();
  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n"
               "FUNC 2987743d0b35b13f b369db048deb3010 938e556cb5a79988"
               " _and_void\n"
               "FUNC d35024aa7ca7da5c 200b26e605f99071 f14ac4fed48c4a99"
               " _without_form\n",
               contents.c_str());

  // Check that m.GetFunctions returns the functions we expect.
  vec.clear();
  m.GetFunctions(&vec, vec.end());
  EXPECT_TRUE(vec.end() != find(vec.begin(), vec.end(), function1));
  EXPECT_TRUE(vec.end() != find(vec.begin(), vec.end(), function2));
  EXPECT_EQ((size_t) 2, vec.size());
}

TEST(Module, WriteOutOfRangeAddresses) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);

  // Specify an allowed address range, representing a PT_LOAD segment in a
  // module.
  vector<Module::Range> address_ranges = {
    Module::Range(0x2000ULL, 0x1000ULL),
  };
  m.SetAddressRanges(address_ranges);

  // Add three stack frames (one lower, one in, and one higher than the allowed
  // address range).  Only the middle frame should be captured.
  auto entry1 = std::make_unique<Module::StackFrameEntry>();
  entry1->address = 0x1000ULL;
  entry1->size = 0x100ULL;
  m.AddStackFrameEntry(std::move(entry1));
  auto entry2 = std::make_unique<Module::StackFrameEntry>();
  entry2->address = 0x2000ULL;
  entry2->size = 0x100ULL;
  m.AddStackFrameEntry(std::move(entry2));
  auto entry3 = std::make_unique<Module::StackFrameEntry>();
  entry3->address = 0x3000ULL;
  entry3->size = 0x100ULL;
  m.AddStackFrameEntry(std::move(entry3));

  // Add a function outside the allowed range.
  Module::File* file = m.FindFile("file_name.cc");
  Module::Function* function = new Module::Function(
      "function_name", 0x4000ULL);
  Module::Range range(0x4000ULL, 0x1000ULL);
  function->ranges.push_back(range);
  function->parameter_size = 0x100ULL;
  Module::Line line = { 0x4000ULL, 0x100ULL, file, 67519080 };
  function->lines.push_back(line);
  m.AddFunction(function);

  // Add an extern outside the allowed range.
  auto extern1 = std::make_unique<Module::Extern>(0x5000ULL);
  extern1->name = "_xyz";
  m.AddExtern(std::move(extern1));

  m.Write(s, ALL_SYMBOL_DATA);

  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n"
               "STACK CFI INIT 2000 100 \n",
               s.str().c_str());

  // Cleanup - Prevent Memory Leak errors.
  delete (function);
}

TEST(Module, ConstructAddFrames) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);

  // First STACK CFI entry, with no initial rules or deltas.
  auto entry1 = std::make_unique<Module::StackFrameEntry>();
  entry1->address = 0xddb5f41285aa7757ULL;
  entry1->size = 0x1486493370dc5073ULL;
  m.AddStackFrameEntry(std::move(entry1));

  // Second STACK CFI entry, with initial rules but no deltas.
  auto entry2 = std::make_unique<Module::StackFrameEntry>();
  entry2->address = 0x8064f3af5e067e38ULL;
  entry2->size = 0x0de2a5ee55509407ULL;
  entry2->initial_rules[".cfa"] = "I think that I shall never see";
  entry2->initial_rules["stromboli"] = "a poem lovely as a tree";
  entry2->initial_rules["cannoli"] = "a tree whose hungry mouth is prest";
  m.AddStackFrameEntry(std::move(entry2));

  // Third STACK CFI entry, with initial rules and deltas.
  auto entry3 = std::make_unique<Module::StackFrameEntry>();
  entry3->address = 0x5e8d0db0a7075c6cULL;
  entry3->size = 0x1c7edb12a7aea229ULL;
  entry3->initial_rules[".cfa"] = "Whose woods are these";
  entry3->rule_changes[0x47ceb0f63c269d7fULL]["calzone"] =
    "the village though";
  entry3->rule_changes[0x47ceb0f63c269d7fULL]["cannoli"] =
    "he will not see me stopping here";
  entry3->rule_changes[0x36682fad3763ffffULL]["stromboli"] =
    "his house is in";
  entry3->rule_changes[0x36682fad3763ffffULL][".cfa"] =
    "I think I know";
  m.AddStackFrameEntry(std::move(entry3));

  // Check that Write writes STACK CFI records properly.
  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();
  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n"
               "STACK CFI INIT ddb5f41285aa7757 1486493370dc5073 \n"
               "STACK CFI INIT 8064f3af5e067e38 de2a5ee55509407"
               " .cfa: I think that I shall never see"
               " cannoli: a tree whose hungry mouth is prest"
               " stromboli: a poem lovely as a tree\n"
               "STACK CFI INIT 5e8d0db0a7075c6c 1c7edb12a7aea229"
               " .cfa: Whose woods are these\n"
               "STACK CFI 36682fad3763ffff"
               " .cfa: I think I know"
               " stromboli: his house is in\n"
               "STACK CFI 47ceb0f63c269d7f"
               " calzone: the village though"
               " cannoli: he will not see me stopping here\n",
               contents.c_str());

  // Check that GetStackFrameEntries works.
  vector<Module::StackFrameEntry*> entries;
  m.GetStackFrameEntries(&entries);
  ASSERT_EQ(3U, entries.size());
  // Check first entry.
  EXPECT_EQ(0xddb5f41285aa7757ULL, entries[0]->address);
  EXPECT_EQ(0x1486493370dc5073ULL, entries[0]->size);
  ASSERT_EQ(0U, entries[0]->initial_rules.size());
  ASSERT_EQ(0U, entries[0]->rule_changes.size());
  // Check second entry.
  EXPECT_EQ(0x8064f3af5e067e38ULL, entries[1]->address);
  EXPECT_EQ(0x0de2a5ee55509407ULL, entries[1]->size);
  ASSERT_EQ(3U, entries[1]->initial_rules.size());
  Module::RuleMap entry2_initial;
  entry2_initial[".cfa"] = "I think that I shall never see";
  entry2_initial["stromboli"] = "a poem lovely as a tree";
  entry2_initial["cannoli"] = "a tree whose hungry mouth is prest";
  EXPECT_THAT(entries[1]->initial_rules, ContainerEq(entry2_initial));
  ASSERT_EQ(0U, entries[1]->rule_changes.size());
  // Check third entry.
  EXPECT_EQ(0x5e8d0db0a7075c6cULL, entries[2]->address);
  EXPECT_EQ(0x1c7edb12a7aea229ULL, entries[2]->size);
  Module::RuleMap entry3_initial;
  entry3_initial[".cfa"] = "Whose woods are these";
  EXPECT_THAT(entries[2]->initial_rules, ContainerEq(entry3_initial));
  Module::RuleChangeMap entry3_changes;
  entry3_changes[0x36682fad3763ffffULL][".cfa"] = "I think I know";
  entry3_changes[0x36682fad3763ffffULL]["stromboli"] = "his house is in";
  entry3_changes[0x47ceb0f63c269d7fULL]["calzone"] = "the village though";
  entry3_changes[0x47ceb0f63c269d7fULL]["cannoli"] =
    "he will not see me stopping here";
  EXPECT_THAT(entries[2]->rule_changes, ContainerEq(entry3_changes));
}

TEST(Module, ConstructUniqueFiles) {
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);
  Module::File* file1 = m.FindFile("foo");
  Module::File* file2 = m.FindFile(string("bar"));
  Module::File* file3 = m.FindFile(string("foo"));
  Module::File* file4 = m.FindFile("bar");
  EXPECT_NE(file1, file2);
  EXPECT_EQ(file1, file3);
  EXPECT_EQ(file2, file4);
  EXPECT_EQ(file1, m.FindExistingFile("foo"));
  EXPECT_TRUE(m.FindExistingFile("baz") == nullptr);
}

TEST(Module, ConstructDuplicateFunctions) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);

  // Two functions.
  Module::Function* function1 = generate_duplicate_function("_without_form");
  Module::Function* function2 = generate_duplicate_function("_without_form");

  m.AddFunction(function1);
  // If this succeeds, we'll have a double-free with the `delete` below. Avoid
  // that.
  ASSERT_FALSE(m.AddFunction(function2));
  delete function2;

  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();
  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n"
               "FUNC d35402aac7a7ad5c 200b26e605f99071 f14ac4fed48c4a99"
               " _without_form\n",
               contents.c_str());
}

TEST(Module, ConstructFunctionsWithSameAddress) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);

  // Two functions.
  Module::Function* function1 = generate_duplicate_function("_without_form");
  Module::Function* function2 = generate_duplicate_function("_and_void");

  m.AddFunction(function1);
  m.AddFunction(function2);

  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();
  EXPECT_STREQ("MODULE os-name architecture id-string name with spaces\n"
               "FUNC d35402aac7a7ad5c 200b26e605f99071 f14ac4fed48c4a99"
               " _and_void\n"
               "FUNC d35402aac7a7ad5c 200b26e605f99071 f14ac4fed48c4a99"
               " _without_form\n",
               contents.c_str());
}

// If multiple fields are enabled, only one function is included per address.
// The entry will be tagged with `m` to show that there are multiple symbols
// at that address.
// TODO(lgrey): Remove the non-multiple versions of these tests and remove the
// suffixes from the suffxed ones when removing `enable_multiple_field_`.
TEST(Module, ConstructFunctionsWithSameAddressMultiple) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID, "", true);

  // Two functions.
  Module::Function* function1 = generate_duplicate_function("_without_form");
  Module::Function* function2 = generate_duplicate_function("_and_void");

  m.AddFunction(function1);
  // If this succeeds, we'll have a double-free with the `delete` below. Avoid
  // that.
  ASSERT_FALSE(m.AddFunction(function2));
  delete function2;

  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();
  EXPECT_STREQ(
      "MODULE os-name architecture id-string name with spaces\n"
      "FUNC m d35402aac7a7ad5c 200b26e605f99071 f14ac4fed48c4a99"
      " _without_form\n",
      contents.c_str());
}

// Externs should be written out as PUBLIC records, sorted by
// address.
TEST(Module, ConstructExterns) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);

  // Two externs.
  auto extern1 = std::make_unique<Module::Extern>(0xffff);
  extern1->name = "_abc";
  auto extern2 = std::make_unique<Module::Extern>(0xaaaa);
  extern2->name = "_xyz";

  m.AddExtern(std::move(extern1));
  m.AddExtern(std::move(extern2));

  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();

  EXPECT_STREQ("MODULE " MODULE_OS " " MODULE_ARCH " "
               MODULE_ID " " MODULE_NAME "\n"
               "PUBLIC aaaa 0 _xyz\n"
               "PUBLIC ffff 0 _abc\n",
               contents.c_str());
}

// Externs with the same address should only keep the first entry
// added.
TEST(Module, ConstructDuplicateExterns) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);

  // Two externs.
  auto extern1 = std::make_unique<Module::Extern>(0xffff);
  extern1->name = "_xyz";
  auto extern2 = std::make_unique<Module::Extern>(0xffff);
  extern2->name = "_abc";

  m.AddExtern(std::move(extern1));
  m.AddExtern(std::move(extern2));

  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();

  EXPECT_STREQ("MODULE " MODULE_OS " " MODULE_ARCH " "
               MODULE_ID " " MODULE_NAME "\n"
               "PUBLIC ffff 0 _xyz\n",
               contents.c_str());
}
// Externs with the same address  have the `m` tag if the multiple field are
// enabled.
TEST(Module, ConstructDuplicateExternsMultiple) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID, "", true);

  // Two externs.
  auto extern1 = std::make_unique<Module::Extern>(0xffff);
  extern1->name = "_xyz";
  auto extern2 = std::make_unique<Module::Extern>(0xffff);
  extern2->name = "_abc";

  m.AddExtern(std::move(extern1));
  m.AddExtern(std::move(extern2));

  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();

  EXPECT_STREQ("MODULE " MODULE_OS " " MODULE_ARCH " " MODULE_ID " " MODULE_NAME
               "\n"
               "PUBLIC m ffff 0 _xyz\n",
               contents.c_str());
}

// If there exists an extern and a function at the same address, only write
// out the FUNC entry.
TEST(Module, ConstructFunctionsAndExternsWithSameAddress) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID);

  // Two externs.
  auto extern1 = std::make_unique<Module::Extern>(0xabc0);
  extern1->name = "abc";
  auto extern2 = std::make_unique<Module::Extern>(0xfff0);
  extern2->name = "xyz";

  m.AddExtern(std::move(extern1));
  m.AddExtern(std::move(extern2));

  Module::Function* function = new Module::Function("_xyz", 0xfff0);
  Module::Range range(0xfff0, 0x10);
  function->ranges.push_back(range);
  function->parameter_size = 0;
  m.AddFunction(function);

  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();

  EXPECT_STREQ("MODULE " MODULE_OS " " MODULE_ARCH " "
               MODULE_ID " " MODULE_NAME "\n"
               "FUNC fff0 10 0 _xyz\n"
               "PUBLIC abc0 0 abc\n",
               contents.c_str());
}

// If there exists an extern and a function at the same address, only write
// out the FUNC entry.
TEST(Module, ConstructFunctionsAndExternsWithSameAddressPreferExternName) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID, "", false, true);

  // Two externs.
  auto extern1 = std::make_unique<Module::Extern>(0xabc0);
  extern1->name = "extern1";
  auto extern2 = std::make_unique<Module::Extern>(0xfff0);
  extern2->name = "extern2";

  m.AddExtern(std::move(extern1));
  m.AddExtern(std::move(extern2));

  Module::Function* function = new Module::Function("function2", 0xfff0);
  Module::Range range(0xfff0, 0x10);
  function->ranges.push_back(range);
  function->parameter_size = 0;
  m.AddFunction(function);

  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();

  EXPECT_STREQ("MODULE " MODULE_OS " " MODULE_ARCH " " MODULE_ID " " MODULE_NAME
               "\n"
               "FUNC fff0 10 0 extern2\n"
               "PUBLIC abc0 0 extern1\n",
               contents.c_str());
}

// If there exists an extern and a function at the same address, only write
// out the FUNC entry, and mark it with `m` if the multiple field is enabled.
TEST(Module, ConstructFunctionsAndExternsWithSameAddressMultiple) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, MODULE_ARCH, MODULE_ID, "", true);

  // Two externs.
  auto extern1 = std::make_unique<Module::Extern>(0xabc0);
  extern1->name = "abc";
  auto extern2 = std::make_unique<Module::Extern>(0xfff0);
  extern2->name = "xyz";

  m.AddExtern(std::move(extern1));
  m.AddExtern(std::move(extern2));

  Module::Function* function = new Module::Function("_xyz", 0xfff0);
  Module::Range range(0xfff0, 0x10);
  function->ranges.push_back(range);
  function->parameter_size = 0;
  m.AddFunction(function);

  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();

  EXPECT_STREQ("MODULE " MODULE_OS " " MODULE_ARCH " " MODULE_ID " " MODULE_NAME
               "\n"
               "FUNC m fff0 10 0 _xyz\n"
               "PUBLIC abc0 0 abc\n",
               contents.c_str());
}

// If there exists an extern and a function at the same address, only write
// out the FUNC entry. For ARM THUMB, the extern that comes from the ELF
// symbol section has bit 0 set.
TEST(Module, ConstructFunctionsAndThumbExternsWithSameAddress) {
  stringstream s;
  Module m(MODULE_NAME, MODULE_OS, "arm", MODULE_ID);

  // Two THUMB externs.
  auto thumb_extern1 = std::make_unique<Module::Extern>(0xabc1);
  thumb_extern1->name = "thumb_abc";
  auto thumb_extern2 = std::make_unique<Module::Extern>(0xfff1);
  thumb_extern2->name = "thumb_xyz";

  auto arm_extern1 = std::make_unique<Module::Extern>(0xcc00);
  arm_extern1->name = "arm_func";

  m.AddExtern(std::move(thumb_extern1));
  m.AddExtern(std::move(thumb_extern2));
  m.AddExtern(std::move(arm_extern1));

  // The corresponding function from the DWARF debug data have the actual
  // address.
  Module::Function* function = new Module::Function("_thumb_xyz", 0xfff0);
  Module::Range range(0xfff0, 0x10);
  function->ranges.push_back(range);
  function->parameter_size = 0;
  m.AddFunction(function);

  m.Write(s, ALL_SYMBOL_DATA);
  string contents = s.str();

  EXPECT_STREQ("MODULE " MODULE_OS " arm "
               MODULE_ID " " MODULE_NAME "\n"
               "FUNC fff0 10 0 _thumb_xyz\n"
               "PUBLIC abc1 0 thumb_abc\n"
               "PUBLIC cc00 0 arm_func\n",
               contents.c_str());
}
