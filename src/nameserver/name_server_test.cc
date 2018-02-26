//
// name_server_test.cc
// Copyright (C) 2017 4paradigm.com
// Author denglong 
// Date 2017-09-07
//

#include "gtest/gtest.h"
#include "logging.h"
#include "timer.h"
#include <gflags/gflags.h>
#include <sched.h>
#include <unistd.h>
#include "tablet/tablet_impl.h"
#include "proto/tablet.pb.h"
#include "proto/name_server.pb.h"
#include "name_server_impl.h"
#include "rpc/rpc_client.h"
#include <brpc/server.h>
#include "base/file_util.h"
#include "client/ns_client.h"

DECLARE_string(endpoint);
DECLARE_string(db_root_path);
DECLARE_string(zk_cluster);
DECLARE_string(zk_root_path);
DECLARE_int32(zk_session_timeout);
DECLARE_int32(zk_keep_alive_check_interval);

using ::rtidb::zk::ZkClient;


namespace rtidb {
namespace nameserver {

inline std::string GenRand() {
    return std::to_string(rand() % 10000000 + 1);
}

class MockClosure : public ::google::protobuf::Closure {

public:
    MockClosure() {}
    ~MockClosure() {}
    void Run() {}

};
class NameServerImplTest : public ::testing::Test {

public:
    NameServerImplTest() {}
    ~NameServerImplTest() {}
};

TEST_F(NameServerImplTest, MakesnapshotTask) {
    FLAGS_zk_cluster="127.0.0.1:22181";
    FLAGS_zk_root_path="/rtidb3" + GenRand();

    FLAGS_endpoint = "127.0.0.1:9631";
    NameServerImpl* nameserver = new NameServerImpl();
    bool ok = nameserver->Init();
    ASSERT_TRUE(ok);
    sleep(4);
    brpc::ServerOptions options;
	brpc::Server server;
	if (server.AddService(nameserver, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        PDLOG(WARNING, "Fail to add service");
        exit(1);
    }
    if (server.Start(FLAGS_endpoint.c_str(), &options) != 0) {
        PDLOG(WARNING, "Fail to start server");
        exit(1);
    }
    ::rtidb::RpcClient<::rtidb::nameserver::NameServer_Stub> name_server_client(FLAGS_endpoint);
    name_server_client.Init();

    FLAGS_endpoint="127.0.0.1:9530";
    ::rtidb::tablet::TabletImpl* tablet = new ::rtidb::tablet::TabletImpl();
    ok = tablet->Init();
    ASSERT_TRUE(ok);
    sleep(2);

    brpc::ServerOptions options1;
	brpc::Server server1;
	if (server1.AddService(tablet, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        PDLOG(WARNING, "Fail to add service");
        exit(1);
    }
    if (server1.Start(FLAGS_endpoint.c_str(), &options1) != 0) {
        PDLOG(WARNING, "Fail to start server");
        exit(1);
    }

    sleep(2);
    
    CreateTableRequest request;
    GeneralResponse response;
    TableInfo *table_info = request.mutable_table_info();
    std::string name = "test" + GenRand();
    table_info->set_name(name);
    TablePartition* partion = table_info->add_table_partition();
    partion->set_pid(0);
    PartitionMeta* meta = partion->add_partition_meta();
    meta->set_endpoint("127.0.0.1:9530");
    meta->set_is_leader(true);
    ok = name_server_client.SendRequest(&::rtidb::nameserver::NameServer_Stub::CreateTable,
            &request, &response, 12, 1);
    ASSERT_TRUE(ok);
    ASSERT_EQ(0, response.code());

    MakeSnapshotNSRequest m_request;
    m_request.set_name(name);
    m_request.set_pid(0);
    ok = name_server_client.SendRequest(&::rtidb::nameserver::NameServer_Stub::MakeSnapshotNS,
            &m_request, &response, 12, 1);
    ASSERT_TRUE(ok);

    sleep(5);

    ZkClient zk_client(FLAGS_zk_cluster, 1000, FLAGS_endpoint, FLAGS_zk_root_path);
    ok = zk_client.Init();
    ASSERT_TRUE(ok);
    std::string op_index_node = FLAGS_zk_root_path + "/op/op_index";
    std::string value;
    ok = zk_client.GetNodeValue(op_index_node, value);
    ASSERT_TRUE(ok);
    std::string op_node = FLAGS_zk_root_path + "/op/op_data/" + value;
    ok = zk_client.GetNodeValue(op_node, value);
    ASSERT_FALSE(ok);

    value.clear();
    std::string table_index_node = FLAGS_zk_root_path + "/table/table_index";
    ok = zk_client.GetNodeValue(table_index_node, value);
    ASSERT_TRUE(ok);
    std::string snapshot_path = FLAGS_db_root_path + "/" + value + "_0/snapshot/";
	std::vector<std::string> vec;
    int cnt = ::rtidb::base::GetFileName(snapshot_path, vec);
    ASSERT_EQ(0, cnt);
    ASSERT_EQ(2, vec.size());

    std::string table_data_node = FLAGS_zk_root_path + "/table/table_data/" + name; 
    ok = zk_client.GetNodeValue(table_data_node, value);
    ASSERT_TRUE(ok);
    ::rtidb::nameserver::TableInfo table_info1;
    table_info1.ParseFromString(value);
    ASSERT_STREQ(table_info->name().c_str(), table_info1.name().c_str());
    ASSERT_EQ(table_info->table_partition_size(), table_info1.table_partition_size());

    // check drop table
    DropTableRequest drop_request;
    drop_request.set_name(name);
    response.Clear();
    ok = name_server_client.SendRequest(&::rtidb::nameserver::NameServer_Stub::DropTable,
            &drop_request, &response, 12, 1);
    ASSERT_TRUE(ok);
    ASSERT_EQ(0, response.code());
    ok = zk_client.GetNodeValue(table_data_node, value);
    ASSERT_FALSE(ok);
}

TEST_F(NameServerImplTest, ConfigGetAndSet) {
    FLAGS_zk_cluster="127.0.0.1:22181";
    FLAGS_zk_root_path="/rtidb3" + GenRand();

    FLAGS_endpoint = "127.0.0.1:9631";
    NameServerImpl* nameserver = new NameServerImpl();
    bool ok = nameserver->Init();
    ASSERT_TRUE(ok);
    sleep(4);
    brpc::ServerOptions options;
	brpc::Server server;
	if (server.AddService(nameserver, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        PDLOG(WARNING, "Fail to add service");
        exit(1);
    }
    if (server.Start(FLAGS_endpoint.c_str(), &options) != 0) {
        PDLOG(WARNING, "Fail to start server");
        exit(1);
    }
    ::rtidb::client::NsClient name_server_client(FLAGS_endpoint);
    name_server_client.Init();
    std::string key = "auto_failover";
    std::string msg;
    std::map<std::string, std::string> conf_map;
    bool ret = name_server_client.ConfGet(key, conf_map, msg);
    ASSERT_TRUE(ret);
    printf("value %s\n", conf_map[key].c_str());
    ASSERT_STREQ(conf_map[key].c_str(), "false");
    ret = name_server_client.ConfSet(key, "true", msg);
    ASSERT_TRUE(ret);
    conf_map.clear();
    ret = name_server_client.ConfGet(key, conf_map, msg);
    ASSERT_TRUE(ret);
    ASSERT_STREQ(conf_map[key].c_str(), "true");
}    

TEST_F(NameServerImplTest, CreateTable) {
    FLAGS_zk_cluster="127.0.0.1:22181";
    FLAGS_zk_root_path="/rtidb3" + GenRand();

    FLAGS_endpoint = "127.0.0.1:9632";
    NameServerImpl* nameserver = new NameServerImpl();
    bool ok = nameserver->Init();
    ASSERT_TRUE(ok);
    sleep(4);
    brpc::ServerOptions options;
	brpc::Server server;
	if (server.AddService(nameserver, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        PDLOG(WARNING, "Fail to add service");
        exit(1);
    }
    if (server.Start(FLAGS_endpoint.c_str(), &options) != 0) {
        PDLOG(WARNING, "Fail to start server");
        exit(1);
    }
    ::rtidb::RpcClient<::rtidb::nameserver::NameServer_Stub> name_server_client(FLAGS_endpoint);
    name_server_client.Init();

    FLAGS_endpoint="127.0.0.1:9531";
    ::rtidb::tablet::TabletImpl* tablet = new ::rtidb::tablet::TabletImpl();
    ok = tablet->Init();
    ASSERT_TRUE(ok);
    sleep(2);

    brpc::ServerOptions options1;
	brpc::Server server1;
	if (server1.AddService(tablet, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        PDLOG(WARNING, "Fail to add service");
        exit(1);
    }
    if (server1.Start(FLAGS_endpoint.c_str(), &options1) != 0) {
        PDLOG(WARNING, "Fail to start server");
        exit(1);
    }

    sleep(2);
    
    CreateTableRequest request;
    GeneralResponse response;
    TableInfo *table_info = request.mutable_table_info();
    std::string name = "test" + GenRand();
    table_info->set_name(name);
    TablePartition* partion = table_info->add_table_partition();
    partion->set_pid(1);
    PartitionMeta* meta = partion->add_partition_meta();
    meta->set_endpoint("127.0.0.1:9531");
    meta->set_is_leader(true);
    TablePartition* partion1 = table_info->add_table_partition();
    partion1->set_pid(2);
    PartitionMeta* meta1 = partion1->add_partition_meta();
    meta1->set_endpoint("127.0.0.1:9531");
    meta1->set_is_leader(true);
    ok = name_server_client.SendRequest(&::rtidb::nameserver::NameServer_Stub::CreateTable,
            &request, &response, 12, 1);
    ASSERT_TRUE(ok);
    ASSERT_EQ(-1, response.code());

    TablePartition* partion2 = table_info->add_table_partition();
    partion2->set_pid(0);
    PartitionMeta* meta2 = partion2->add_partition_meta();
    meta2->set_endpoint("127.0.0.1:9531");
    meta2->set_is_leader(true);
    ok = name_server_client.SendRequest(&::rtidb::nameserver::NameServer_Stub::CreateTable,
            &request, &response, 12, 1);
    ASSERT_TRUE(ok);
    ASSERT_EQ(0, response.code());
}    

}
}

int main(int argc, char** argv) {

    ::testing::InitGoogleTest(&argc, argv);
    srand (time(NULL));
    ::baidu::common::SetLogLevel(::baidu::common::DEBUG);
    ::google::ParseCommandLineFlags(&argc, &argv, true);
    FLAGS_db_root_path = "/tmp/" + ::rtidb::nameserver::GenRand();
    return RUN_ALL_TESTS();
}



