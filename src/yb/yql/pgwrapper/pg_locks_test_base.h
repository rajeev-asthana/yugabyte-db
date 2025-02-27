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

#include "yb/client/tablet_server.h"
#include "yb/common/transaction.h"

#include "yb/tserver/mini_tablet_server.h"
#include "yb/tserver/tablet_server.h"
#include "yb/tserver/tserver_service.proxy.h"

#include "yb/yql/pgwrapper/geo_transactions_test_base.h"
#include "yb/yql/pgwrapper/pg_mini_test_base.h"

DECLARE_int32(transaction_table_num_tablets);

using namespace std::literals;
using std::string;

namespace yb {
namespace pgwrapper {

using tserver::TabletServerServiceProxy;

class PgLocksTestBase : public client::GeoTransactionsTestBase {
 protected:
  void SetUp() override;

  virtual void InitTSProxies();

  size_t NumTabletServers() override {
    return 1;
  }

  size_t NumRegions() override {
    return 1;
  }

  void OverrideMiniClusterOptions(MiniClusterOptions* options) override {
    options->transaction_table_num_tablets = 1;
  }

  Result<TabletId> GetSingularTabletOfTable(const string& table_name);

  Result<TabletId> GetSingularStatusTablet();

  Result<TabletId> CreateTableAndGetTabletId(const string& table_name);

  Result<tserver::GetLockStatusResponsePB> GetLockStatus(
      const TabletId& tablet_id,
      const std::vector<TransactionId>& transactions_ids = {});

  Result<TransactionId> GetSingularTransactionOnTablet(const TabletId& tablet_id);

  Result<TransactionId> OpenTransaction(
      const std::shared_ptr<PGConn>& conn, const string& table_name, const TabletId& tablet_id,
      const std::string& key);

  struct TestSession {
    std::shared_ptr<PGConn> conn;
    TabletId first_involved_tablet;
    string table_name;
    TransactionId txn_id = TransactionId::Nil();
  };

  Result<TestSession> Init(
      const string& table_name, const std::string& key_to_lock, const bool create_table = true);

  std::vector<TabletServerServiceProxy*> get_ts_proxies(const std::string& ts_uuid = "");

  Result<std::future<Status>> ExpectBlockedAsync(pgwrapper::PGConn* conn, const std::string& query);

  static constexpr int kTimeoutMs = 2000;

 private:
  std::vector<std::unique_ptr<TabletServerServiceProxy>> ts_proxies_;
};

} // namespace pgwrapper
} // namespace yb
