// Copyright (c) YugaByte, Inc.
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

#pragma once

#include <cstdlib>

namespace yb {

void SetUpPackedRowTestFlags();

template <class Base>
class PackedRowTestBase : public Base {
  void SetUp() override {
    SetUpPackedRowTestFlags();
    Base::SetUp();
  }
};

class MiniCluster;

void CheckNumRecords(MiniCluster* cluster, size_t expected_num_records);

} // namespace yb
