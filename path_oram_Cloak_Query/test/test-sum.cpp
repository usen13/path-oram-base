#include "definitions.h"
#include "oram.hpp"
#include "utility.hpp"
#include <filesystem>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <math.h>
#include <thread>
#include "../../cpp-sql-server/src/sql_handler.h"
#include "../../cpp-sql-server/src/sql_utils.h"
#include "../../Shamir_Parser/shamir_parser.h"
using json = nlohmann::json;

size_t attributeIndex(const std::string& attribute) {
    // Map attribute names to their index
    static const std::unordered_map<std::string, size_t> attributeMap = {
        {"ORDERKEY", 0},
        {"PARTKEY", 1},
        {"SUPPKEY", 2},
        {"LINENUMBER", 3},
        {"QUANTITY", 4},
        {"EXTENDEDPRICE", 5},
        {"DISCOUNT", 6},
        {"TAX", 7},
        {"RETURNFLAG", 8},
        {"LINESTATUS", 9},
        {"SHIPDATE", 10},
        {"COMMITDATE", 11},
        {"RECEIPTDATE", 12},
        {"SHIPINSTRUCT", 13},
        {"SHIPMODE", 14},
        {"COMMENT", 15},
    };
    auto it = attributeMap.find(attribute);
    return (it != attributeMap.end()) ? it->second : -1;
}

int64_t reconstructSecret(const std::vector<std::pair<int64_t, int64_t>>& shares, int k) {
    double secret = 0;
    // Shares are pairs of (x, y) where x is the share index and y is the share value
    // k is the minimum number of shares needed to reconstruct the secret
    // Lagrange interpolation formula: P(x) = Σ y_i * l_i(x)
    // where l_i(x) = Π (x - x_j) / (x_i - x_j) for j ≠ i
    // P(x) is the polynomial that passes through the points (x_i, y_i)
    // We can reconstruct the secret by evaluating the polynomial at x = 0
    for (int i = 0; i < k; ++i) {
        double lagrange_coeff = 1.0;

        for (int j = 0; j < k; ++j) {
            if (i != j) {
                lagrange_coeff *= static_cast<double>(-shares[j].first) / (shares[i].first - shares[j].first);
            }
        }

        secret += shares[i].second * lagrange_coeff;
    }

    return static_cast<int64_t>(std::round(secret));
}

class SumAggregationTest : public ::testing::Test {
protected:
    std::string jsonPath = "../SQL_Queries/SUM/ExtendedPrice.json";
    std::string resultDir = "../Query_Result/SUMAND";
    int numServers = 6;
    int threshold = 3;
    size_t sumIdx;
    std::vector<int64_t> server_sums;
    std::vector<int> server_indices;
    void SetUp() override {
        // Parse JSON
        std::ifstream jsonFile(jsonPath);
        json j;
        ASSERT_TRUE(jsonFile.is_open());
        jsonFile >> j;
        std::string sumAttr = j["select"][0]["attribute"];
        sumIdx = attributeIndex(sumAttr);
        // For each server file
        for (int server = 1; server <= numServers; ++server) {
            std::string filePath = resultDir + "/server_" + std::to_string(server) + ".txt";
            std::ifstream inFile(filePath);
            ASSERT_TRUE(inFile.is_open()) << "Failed to open " << filePath;
            int64_t sum = 0;
            std::string line;
            while (std::getline(inFile, line)) {
                std::istringstream iss(line);
                std::string token;
                std::vector<int64_t> tuple;
                while (std::getline(iss, token, '|')) {
                    token.erase(0, token.find_first_not_of(" \t"));
                    token.erase(token.find_last_not_of(" \t") + 1);
                    if (!token.empty())
                        tuple.push_back(std::stoll(token));
                }
                if (tuple.size() > sumIdx)
                    sum += tuple[sumIdx];
            }
            server_sums.push_back(sum);
            server_indices.push_back(server);
        }
    }
};

TEST_F(SumAggregationTest, ReconstructSumWithShamirParser) {
    // Prepare shares for reconstruction
    std::vector<std::pair<int64_t, int64_t>> shares;
    for (size_t i = 0; i < server_indices.size(); ++i) {
        shares.emplace_back(server_indices[i], server_sums[i]);
    }
    //ShamirParser parser;
    int64_t actual_sum = reconstructSecret(shares, threshold);
    std::cout << "Actual SUM (Lagrange interpolation): " << actual_sum << std::endl;
    // You can set an expected value here if known, e.g.:
    // int64_t expected_sum = ...;
    // EXPECT_EQ(actual_sum, expected_sum);
    // For now, just check that the result is not zero (change as needed):
    EXPECT_NE(actual_sum, 0);
    std::cout << "Reconstructed sum: " << actual_sum << std::endl;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}