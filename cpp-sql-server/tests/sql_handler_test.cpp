#include "sql_handler.h"
#include <gtest/gtest.h>

class SQLHandlerTest : public ::testing::Test {
protected:
    SQLHandler sqlHandler;

    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

TEST_F(SQLHandlerTest, ExecuteValidQuery) {
    std::string query = "SELECT * FROM users;";
    ASSERT_NO_THROW(sqlHandler.executeQuery(query));
    auto results = sqlHandler.getResults();
    ASSERT_FALSE(results.empty());
}

TEST_F(SQLHandlerTest, ExecuteInvalidQuery) {
    std::string query = "INVALID SQL QUERY";
    ASSERT_THROW(sqlHandler.executeQuery(query), std::runtime_error);
}

TEST_F(SQLHandlerTest, ExecuteInsertQuery) {
    std::string query = "INSERT INTO users (name, age) VALUES ('Alice', 30);";
    ASSERT_NO_THROW(sqlHandler.executeQuery(query));
    // Verify the insertion
    std::string selectQuery = "SELECT * FROM users WHERE name = 'Alice';";
    ASSERT_NO_THROW(sqlHandler.executeQuery(selectQuery));
    auto results = sqlHandler.getResults();
    ASSERT_EQ(results.size(), 1);
    ASSERT_EQ(results[0]["name"], "Alice");
    ASSERT_EQ(results[0]["age"], 30);
}

TEST_F(SQLHandlerTest, GetResultsAfterExecution) {
    std::string query = "SELECT * FROM users;";
    sqlHandler.executeQuery(query);
    auto results = sqlHandler.getResults();
    ASSERT_FALSE(results.empty());
}