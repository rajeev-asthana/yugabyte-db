//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//
// The following only applies to changes made to this file as part of YugaByte development.
//
// Portions Copyright (c) YugaByte, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
// in compliance with the License.  You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied.  See the License for the specific language governing permissions and limitations
// under the License.
//

#include <assert.h>

#include <memory>

#include <gtest/gtest.h>

#include "yb/rocksdb/cache.h"
#include "yb/rocksdb/db.h"
#include "yb/rocksdb/env.h"
#include "yb/rocksdb/merge_operator.h"
#include "yb/rocksdb/port/stack_trace.h"
#include "yb/rocksdb/util/coding.h"
#include "yb/rocksdb/util/testharness.h"
#include "yb/rocksdb/util/testutil.h"
#include "yb/rocksdb/utilities/merge_operators.h"

#include "yb/util/test_macros.h"

using std::unique_ptr;

DECLARE_bool(never_fsync);

namespace rocksdb {

namespace {
size_t num_merge_operator_calls;
void resetNumMergeOperatorCalls() { num_merge_operator_calls = 0; }

size_t num_partial_merge_calls;
void resetNumPartialMergeCalls() { num_partial_merge_calls = 0; }
}

class CountMergeOperator: public AssociativeMergeOperator {
 public:
  CountMergeOperator() {
    mergeOperator_ = MergeOperators::CreateUInt64AddOperator();
  }

  virtual bool Merge(const Slice& key,
                     const Slice* existing_value,
                     const Slice& value,
                     std::string* new_value,
                     Logger* logger) const override {
    assert(new_value->empty());
    ++num_merge_operator_calls;
    if (existing_value == nullptr) {
      new_value->assign(value.cdata(), value.size());
      return true;
    }

    return mergeOperator_->PartialMerge(
        key,
        *existing_value,
        value,
        new_value,
        logger);
  }

  virtual bool PartialMergeMulti(const Slice &key,
                                 const std::deque<Slice> &operand_list,
                                 std::string *new_value,
                                 Logger *logger) const override {
    assert(new_value->empty());
    ++num_partial_merge_calls;
    return mergeOperator_->PartialMergeMulti(key, operand_list, new_value,
        logger);
  }

  const char *Name() const override {
    return "UInt64AddOperator";
  }

 private:
  std::shared_ptr<MergeOperator> mergeOperator_;
};

namespace {
std::shared_ptr<DB> OpenDb(const std::string &dbname,
                           const size_t max_successive_merges = 0,
                           const uint32_t min_partial_merge_operands = 2) {
  DB *db;
  Options options;
  options.create_if_missing = true;
  options.merge_operator = std::make_shared<CountMergeOperator>();
  options.max_successive_merges = max_successive_merges;
  options.min_partial_merge_operands = min_partial_merge_operands;
  Status s;
  CHECK_OK(DestroyDB(dbname, Options()));
  s = DB::Open(options, dbname, &db);
  if (!s.ok()) {
    std::cerr << s.ToString() << std::endl;
    assert(false);
  }
  return std::shared_ptr<DB>(db);
}
}  // namespace

// Imagine we are maintaining a set of uint64 counters.
// Each counter has a distinct name. And we would like
// to support four high level operations:
// set, add, get and remove
// This is a quick implementation without a Merge operation.
class Counters {

 protected:
  std::shared_ptr<DB> db_;

  WriteOptions put_option_;
  ReadOptions get_option_;
  WriteOptions delete_option_;

  uint64_t default_;

 public:
  explicit Counters(std::shared_ptr<DB> db, uint64_t defaultCount = 0)
      : db_(db),
        put_option_(),
        get_option_(),
        delete_option_(),
        default_(defaultCount) {
    assert(db_);
  }

  virtual ~Counters() {}

  // public interface of Counters.
  // All four functions return false
  // if the underlying level db operation failed.

  // mapped to a levedb Put
  bool set(const std::string &key, uint64_t value) {
    // just treat the internal rep of int64 as the string
    Slice slice(reinterpret_cast<char *>(&value), sizeof(value));
    auto s = db_->Put(put_option_, key, slice);

    if (s.ok()) {
      return true;
    } else {
      std::cerr << s.ToString() << std::endl;
      return false;
    }
  }

  // mapped to a rocksdb Delete
  bool remove(const std::string &key) {
    auto s = db_->Delete(delete_option_, key);

    if (s.ok()) {
      return true;
    } else {
      std::cerr << s.ToString() << std::endl;
      return false;
    }
  }

  // mapped to a rocksdb Get
  bool get(const std::string &key, uint64_t *value) {
    std::string str;
    auto s = db_->Get(get_option_, key, &str);

    if (s.IsNotFound()) {
      // return default value if not found;
      *value = default_;
      return true;
    } else if (s.ok()) {
      // deserialization
      if (str.size() != sizeof(uint64_t)) {
        std::cerr << "value corruption\n";
        return false;
      }
      *value = DecodeFixed64(&str[0]);
      return true;
    } else {
      std::cerr << s.ToString() << std::endl;
      return false;
    }
  }

  // 'add' is implemented as get -> modify -> set
  // An alternative is a single merge operation, see MergeBasedCounters
  virtual bool add(const std::string &key, uint64_t value) {
    uint64_t base = default_;
    return get(key, &base) && set(key, base + value);
  }


  // convenience functions for testing
  void assert_set(const std::string &key, uint64_t value) {
    assert(set(key, value));
  }

  void assert_remove(const std::string &key) { assert(remove(key)); }

  uint64_t assert_get(const std::string &key) {
    uint64_t value = default_;
    int result = get(key, &value);
    assert(result);
    if (result == 0) exit(1); // Disable unused variable warning.
    return value;
  }

  void assert_add(const std::string &key, uint64_t value) {
    int result = add(key, value);
    assert(result);
    if (result == 0) exit(1); // Disable unused variable warning.
  }
};

// Implement 'add' directly with the new Merge operation
class MergeBasedCounters: public Counters {
 private:
  WriteOptions merge_option_; // for merge

 public:
  explicit MergeBasedCounters(std::shared_ptr<DB> db, uint64_t defaultCount = 0)
      : Counters(db, defaultCount),
        merge_option_() {
  }

  // mapped to a rocksdb Merge operation
  bool add(const std::string &key, uint64_t value) override {
    char encoded[sizeof(uint64_t)];
    EncodeFixed64(encoded, value);
    Slice slice(encoded, sizeof(uint64_t));
    auto s = db_->Merge(merge_option_, key, slice);

    if (s.ok()) {
      return true;
    } else {
      std::cerr << s.ToString() << std::endl;
      return false;
    }
  }
};

namespace {
void dumpDb(DB *db) {
  auto it = unique_ptr<Iterator>(db->NewIterator(ReadOptions()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    uint64_t value = DecodeFixed64(it->value().data());
    std::cout << it->key().ToString() << ": " << value << std::endl;
  }
  assert(it->status().ok());  // Check for any errors found during the scan
}

void testCounters(Counters* counters_ptr, DB *db, bool test_compaction) {
  Counters& counters = *counters_ptr;

  FlushOptions o;
  o.wait = true;

  counters.assert_set("a", 1);

  if (test_compaction) {
    ASSERT_OK(db->Flush(o));
  }

  assert(counters.assert_get("a") == 1);

  counters.assert_remove("b");

  // defaut value is 0 if non-existent
  assert(counters.assert_get("b") == 0);

  counters.assert_add("a", 2);

  if (test_compaction) {
    ASSERT_OK(db->Flush(o));
  }

  // 1+2 = 3
  assert(counters.assert_get("a") == 3);

  dumpDb(db);

  std::cout << "1\n";

  // 1+...+49 = ?
  uint64_t sum __attribute__((__unused__)) = 0;
  for (int i = 1; i < 50; i++) {
    counters.assert_add("b", i);
    sum += i;
  }
  assert(counters.assert_get("b") == sum);

  std::cout << "2\n";
  dumpDb(db);

  std::cout << "3\n";

  if (test_compaction) {
    ASSERT_OK(db->Flush(o));

    std::cout << "Compaction started ...\n";
    ASSERT_OK(db->CompactRange(CompactRangeOptions(), nullptr, nullptr));
    std::cout << "Compaction ended\n";

    dumpDb(db);

    DCHECK_EQ(counters.assert_get("a"), 3);
    DCHECK(counters.assert_get("b") == sum);
  }
}

void testSuccessiveMerge(Counters *counters_ptr, size_t max_num_merges,
                         size_t num_merges) {
  Counters& counters = *counters_ptr;

  counters.assert_remove("z");
  uint64_t sum __attribute__((__unused__)) = 0;

  for (size_t i = 1; i <= num_merges; ++i) {
    resetNumMergeOperatorCalls();
    counters.assert_add("z", i);
    sum += i;

    if (i % (max_num_merges + 1) == 0) {
      assert(num_merge_operator_calls == max_num_merges + 1);
    } else {
      assert(num_merge_operator_calls == 0);
    }

    resetNumMergeOperatorCalls();
    DCHECK(counters.assert_get("z") == sum);
    DCHECK(num_merge_operator_calls == i % (max_num_merges + 1));
  }
}

void testPartialMerge(Counters *counters, DB *db, size_t max_merge,
                      size_t min_merge, size_t count) {
  FlushOptions o;
  o.wait = true;

  // Test case 1: partial merge should be called when the number of merge
  //              operands exceeds the threshold.
  uint64_t tmp_sum = 0;
  resetNumPartialMergeCalls();
  for (size_t i = 1; i <= count; i++) {
    counters->assert_add("b", i);
    tmp_sum += i;
  }
  ASSERT_OK(db->Flush(o));
  ASSERT_OK(db->CompactRange(CompactRangeOptions(), nullptr, nullptr));
  ASSERT_EQ(tmp_sum, counters->assert_get("b"));
  if (count > max_merge) {
    // in this case, FullMerge should be called instead.
    ASSERT_EQ(num_partial_merge_calls, 0U);
  } else {
    // if count >= min_merge, then partial merge should be called once.
    ASSERT_EQ((count >= min_merge), (num_partial_merge_calls == 1));
  }

  // Test case 2: partial merge should not be called when a put is found.
  resetNumPartialMergeCalls();
  tmp_sum = 0;
  ASSERT_OK(db->Put(rocksdb::WriteOptions(), "c", "10"));
  for (size_t i = 1; i <= count; i++) {
    counters->assert_add("c", i);
    tmp_sum += i;
  }
  ASSERT_OK(db->Flush(o));
  ASSERT_OK(db->CompactRange(CompactRangeOptions(), nullptr, nullptr));
  ASSERT_EQ(tmp_sum, counters->assert_get("c"));
  ASSERT_EQ(num_partial_merge_calls, 0U);
}

void testSingleBatchSuccessiveMerge(DB *db, size_t max_num_merges,
                                    size_t num_merges) {
  assert(num_merges > max_num_merges);

  Slice key("BatchSuccessiveMerge");
  uint64_t merge_value = 1;
  Slice merge_value_slice(reinterpret_cast<char*>(&merge_value), sizeof(merge_value));

  // Create the batch
  WriteBatch batch;
  for (size_t i = 0; i < num_merges; ++i) {
    batch.Merge(key, merge_value_slice);
  }

  // Apply to memtable and count the number of merges
  resetNumMergeOperatorCalls();
  {
    Status s = db->Write(WriteOptions(), &batch);
    assert(s.ok());
  }
  ASSERT_EQ(
      num_merge_operator_calls,
      static_cast<size_t>(num_merges - (num_merges % (max_num_merges + 1))));

  // Get the value
  resetNumMergeOperatorCalls();
  std::string get_value_str;
  {
    Status s = db->Get(ReadOptions(), key, &get_value_str);
    assert(s.ok());
  }
  assert(get_value_str.size() == sizeof(uint64_t));
  uint64_t get_value = DecodeFixed64(&get_value_str[0]);
  ASSERT_EQ(get_value, num_merges * merge_value);
  ASSERT_EQ(num_merge_operator_calls,
      static_cast<size_t>((num_merges % (max_num_merges + 1))));
}

void runTest(int argc, const std::string &dbname) {
  bool compact = false;
  if (argc > 1) {
    compact = true;
    std::cout << "Turn on Compaction\n";
  }

  {
    auto db = OpenDb(dbname);

    {
      std::cout << "Test read-modify-write counters... \n";
      Counters counters(db, 0);
      testCounters(&counters, db.get(), true);
    }

    {
      std::cout << "Test merge-based counters... \n";
      MergeBasedCounters counters(db, 0);
      testCounters(&counters, db.get(), compact);
    }
  }

  ASSERT_OK(DestroyDB(dbname, Options()));

  {
    std::cout << "Test merge in memtable... \n";
    size_t max_merge = 5;
    auto db = OpenDb(dbname, max_merge);
    MergeBasedCounters counters(db, 0);
    testCounters(&counters, db.get(), compact);
    testSuccessiveMerge(&counters, max_merge, max_merge * 2);
    testSingleBatchSuccessiveMerge(db.get(), 5, 7);
    ASSERT_OK(DestroyDB(dbname, Options()));
  }

  {
    std::cout << "Test Partial-Merge\n";
    size_t max_merge = 100;
    for (uint32_t min_merge = 5; min_merge < 25; min_merge += 5) {
      for (uint32_t count = min_merge - 1; count <= min_merge + 1; count++) {
        auto db = OpenDb(dbname, max_merge, min_merge);
        MergeBasedCounters counters(db, 0);
        testPartialMerge(&counters, db.get(), max_merge, min_merge, count);
        ASSERT_OK(DestroyDB(dbname, Options()));
      }
      {
        auto db = OpenDb(dbname, max_merge, min_merge);
        MergeBasedCounters counters(db, 0);
        testPartialMerge(&counters, db.get(), max_merge, min_merge,
            min_merge * 10);
        ASSERT_OK(DestroyDB(dbname, Options()));
      }
    }
  }

  {
    std::cout << "Test merge-operator not set after reopen\n";
    {
      auto db = OpenDb(dbname);
      MergeBasedCounters counters(db, 0);
      counters.add("test-key", 1);
      counters.add("test-key", 1);
      counters.add("test-key", 1);
      ASSERT_OK(db->CompactRange(CompactRangeOptions(), nullptr, nullptr));
    }

    DB *reopen_db;
    ASSERT_OK(DB::Open(Options(), dbname, &reopen_db));
    std::string value;
    ASSERT_TRUE(!(reopen_db->Get(ReadOptions(), "test-key", &value).ok()));
    delete reopen_db;
    ASSERT_OK(DestroyDB(dbname, Options()));
  }

  /* Temporary remove this test
  {
    std::cout << "Test merge-operator not set after reopen (recovery case)\n";
    {
      auto db = OpenDb(dbname);
      MergeBasedCounters counters(db, 0);
      counters.add("test-key", 1);
      counters.add("test-key", 1);
      counters.add("test-key", 1);
    }

    DB* reopen_db;
    ASSERT_TRUE(DB::Open(Options(), dbname, &reopen_db).IsInvalidArgument());
  }
  */
}
}  // namespace
} // namespace rocksdb

int main(int argc, char *argv[]) {
  // TODO: Make this test like a general rocksdb unit-test
  // This is set by default in RocksDBTest and YBTest, and can be removed once this test is a
  // general rocksdb unit-test
  ANNOTATE_UNPROTECTED_WRITE(FLAGS_never_fsync) = true;
  rocksdb::port::InstallStackTraceHandler();
  rocksdb::runTest(argc, rocksdb::test::TmpDir() + "/merge_testdb");
  printf("Passed all tests!\n");
  return 0;
}
