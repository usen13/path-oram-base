#include "gtest/gtest.h"
#include "../src/server.h"

class ServerTest : public ::testing::Test {
protected:
    Server* server;

    void SetUp() override {
        server = new Server();
    }

    void TearDown() override {
        delete server;
    }
};

TEST_F(ServerTest, StartServer) {
    ASSERT_NO_THROW(server->start());
    // Additional checks can be added here to verify server state
}

TEST_F(ServerTest, StopServer) {
    server->start();
    ASSERT_NO_THROW(server->stop());
    // Additional checks can be added here to verify server state
}

TEST_F(ServerTest, HandleClient) {
    server->start();
    std::string query = "SELECT * FROM test_table;";
    ASSERT_NO_THROW(server->handleClient(query));
    // Additional checks can be added here to verify query handling
}