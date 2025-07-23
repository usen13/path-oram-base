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

double reconstructSecretFloat(const std::vector<std::pair<int64_t, int64_t>>& shares, int k) {
    double secret = 0;

    for (int i = 0; i < k; ++i) {
        double lagrange_coeff = 1.0;

        for (int j = 0; j < k; ++j) {
            if (i != j) {
                lagrange_coeff *= static_cast<double>(-shares[j].first) / (shares[i].first - shares[j].first);
            }
        }

        secret = secret + shares[i].second * lagrange_coeff;
    }

    return static_cast<double>((secret ) / 100.0); // Scale back to floating point
}

// Convert integer to string
std::string ShamirParser::intToString(int64_t value) {
    std::string result;
    while (value > 0) {
        result = static_cast<char>(value % 256) + result;
        value /= 256;
    }
    return result;
}

std::string timestampToDate(int64_t timestamp) {
    std::time_t time = timestamp;
    std::tm* tm = std::localtime(&time);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%d");
    return ss.str();
}

// Convert integer to string
std::string intToString(int64_t value) {
    std::string result;
    while (value > 0) {
        result = static_cast<char>(value % 256) + result;
        value /= 256;
    }
    return result;
}

std::vector<std::vector<std::vector<int64_t>>> loadAllShares(int n, const std::string& jsonPath) {
    std::vector<std::vector<std::vector<int64_t>>> allShares(n);
// The format of allShares is as follows:
// Inner most vector contains the shares for a single tuple, in total 16 shares attributes
// Middle vector contains all the tuples for a single server, in total n tuples 
// Outer most vector must contain shares of total n servers, in our case n = 6
std::string baseDir = "../Query_Result/MAXOR"; // Relative path to the shares directory

    for (int serverIndex = 1; serverIndex < n; ++serverIndex) {
        std::string filePath = baseDir + "/server_" + std::to_string(serverIndex) + ".txt";
        std::ifstream file(filePath, std::ios::in);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                std::istringstream iss(line);
                std::string y_str;
                std::vector<int64_t> tupleShares;
                while (std::getline(iss, y_str, '|')) {
                    // Trim leading and trailing whitespace
                    y_str.erase(0, y_str.find_first_not_of(" \t\n\r"));
                    y_str.erase(y_str.find_last_not_of(" \t\n\r") + 1);

                    if (!y_str.empty()) {
                        tupleShares.push_back(std::stoll(y_str));
                    }
                }
                allShares[serverIndex - 1].push_back({tupleShares});
            }
            file.close();
        } else {
            std::cerr << "Error opening file: server_" << serverIndex << ".txt" << std::endl;
        }
    }

    return allShares;
}

std::vector<std::vector<std::vector<std::pair<int64_t, int64_t>>>> transformShares(const std::vector<std::vector<std::vector<int64_t>>>& allShares) {
    std::vector<std::vector<std::vector<std::pair<int64_t, int64_t>>>> transformedShares;

    if (allShares.empty()) {
        std::cerr << "Error: No shares to transform." << std::endl;
        return transformedShares;
    }
    
    size_t numServers = allShares.size(); // The size of allShares.size is 6, which is the number of servers
    size_t numTuples = allShares[0].size(); // The size of allShares[0].size() is n, which is the number of tuples
    size_t numAttributes = allShares[0][0].size(); // Number of attributes, should be 16

    transformedShares.resize(numAttributes);

    for (size_t attributeIndex = 0; attributeIndex < numAttributes; ++attributeIndex) {
        transformedShares[attributeIndex].resize(numTuples);
        for (size_t tupleIndex = 0; tupleIndex < numTuples; ++tupleIndex) {
            for (size_t serverIndex = 0; serverIndex < numServers; ++serverIndex) {
                if (serverIndex < allShares.size() && tupleIndex < allShares[serverIndex].size() && attributeIndex < allShares[serverIndex][tupleIndex].size()) {
                    transformedShares[attributeIndex][tupleIndex].emplace_back(serverIndex + 1, allShares[serverIndex][tupleIndex][attributeIndex]);
                }
            }
        }
    }    

    return transformedShares;
}

double getMaxFromItems(const std::vector<LineItem>& items, const std::string& attrName) {
    double maxValue = std::numeric_limits<double>::lowest();
    for (const auto& item : items) {
        if (attrName == "EXTENDEDPRICE") {
            if (item.L_EXTENDEDPRICE > maxValue) maxValue = item.L_EXTENDEDPRICE;
        } else if (attrName == "QUANTITY") {
            if (item.L_QUANTITY > maxValue) maxValue = item.L_QUANTITY;
        } else if (attrName == "DISCOUNT") {
            if (item.L_DISCOUNT > maxValue) maxValue = item.L_DISCOUNT;
        } else if (attrName == "TAX") {
            if (item.L_TAX > maxValue) maxValue = item.L_TAX;
        }
        // Add more attributes as needed
    }
    return maxValue;
}

class MaxAggregationTest : public ::testing::TestWithParam<std::tuple<std::string, std::string>> {
protected:
    std::string jsonPath;
    std::string resultDir;
    int numServers = 6;
    int threshold = 3;
    int totalTuples = 0;
    std::string attrName; // Attribute name to be used for max aggregation
    size_t sumIdx;
    std::vector<int64_t> server_sums;
    std::vector<int> server_indices;
    void SetUp() override {
        std::tie(jsonPath, resultDir) = GetParam();
        // Parse JSON
        std::ifstream jsonFile(jsonPath);
        json j;
        ASSERT_TRUE(jsonFile.is_open());
        jsonFile >> j;
        attrName = j["select"][0]["attribute"];
        sumIdx = attributeIndex(attrName);
        // For each server file
        for (int server = 1; server <= numServers; ++server) {
            std::string filePath = resultDir + "/server_" + std::to_string(server) + ".txt";
            std::ifstream inFile(filePath);
            ASSERT_TRUE(inFile.is_open()) << "Failed to open " << filePath;
            int64_t sum = 0;
            std::string line;
            while (std::getline(inFile, line)) {
                std::istringstream iss(line);
                totalTuples++;
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
        std::cout << "Total tuples processed: " << totalTuples << std::endl;
    }
};


INSTANTIATE_TEST_SUITE_P(
    AvgDirs,
    MaxAggregationTest,
    ::testing::Values(
        std::make_tuple("../SQL_Queries/MAX/ExtendedPrice_Max.json", "../Query_Result/MAXOR"),
        std::make_tuple("../SQL_Queries/MAX/Tax_Max.json", "../Query_Result/MAXAND")
    )
);

TEST_P(MaxAggregationTest, ReconstructMAXWithShamirParser) {
    // Prepare shares for reconstruction
    std::vector<std::pair<int64_t, int64_t>> shares;
    for (size_t i = 0; i < server_indices.size(); ++i) {
        shares.emplace_back(server_indices[i], server_sums[i]);
    }

    std::string filename = "reconstructed_max.txt";

    std::vector<LineItem> reconstructedItems;

    auto tempShares = loadAllShares(6, jsonPath);
    auto allShares = transformShares(tempShares);

    for (size_t i = 0; i < tempShares[0].size(); ++i) {
            LineItem item;
            item.L_ORDERKEY = reconstructSecret(allShares[0][i], 3);
            item.L_PARTKEY = reconstructSecret(allShares[1][i], 3);
            item.L_SUPPKEY = reconstructSecret(allShares[2][i], 3);
            item.L_LINENUMBER = reconstructSecret(allShares[3][i], 3);
            item.L_QUANTITY = reconstructSecret(allShares[4][i], 3);
            item.L_EXTENDEDPRICE = reconstructSecret(allShares[5][i], 3);
            item.L_DISCOUNT = reconstructSecretFloat(allShares[6][i], 3);
            item.L_TAX = reconstructSecretFloat(allShares[7][i], 3);
            item.L_RETURNFLAG = static_cast<char>(reconstructSecret(allShares[8][i], 3));
            item.L_LINESTATUS = static_cast<char>(reconstructSecret(allShares[9][i], 3));
            item.L_SHIPDATE = timestampToDate(reconstructSecret(allShares[10][i], 3));
            item.L_COMMITDATE = timestampToDate(reconstructSecret(allShares[11][i], 3));
            item.L_RECEIPTDATE = timestampToDate(reconstructSecret(allShares[12][i], 3));
            item.L_SHIPINSTRUCT = intToString(reconstructSecret(allShares[13][i], 3));
            item.L_SHIPMODE = intToString(reconstructSecret(allShares[14][i], 3));
            item.L_COMMENT = intToString(reconstructSecret(allShares[15][i], 3));

            reconstructedItems.push_back(item);
        }

        std::ofstream outputFile(resultDir + "/" + filename);
        if (outputFile.is_open()) {
            for (const auto& item : reconstructedItems) {
                outputFile << item.L_ORDERKEY << "|" << item.L_PARTKEY << "|" << item.L_SUPPKEY << "|" << item.L_LINENUMBER << "|"
                           << item.L_QUANTITY << "|" << item.L_EXTENDEDPRICE << "|" << item.L_DISCOUNT << "|" << item.L_TAX << "|"
                           << item.L_RETURNFLAG << "|" << item.L_LINESTATUS << "|" << item.L_SHIPDATE << "|" << item.L_COMMITDATE << "|"
                           << item.L_RECEIPTDATE << "|" << item.L_SHIPINSTRUCT << "|" << item.L_SHIPMODE << "|" << item.L_COMMENT << "\n";
            }
            outputFile.close();
        } else {
            std::cerr << "Error opening output file: " << filename << std::endl;
        }
        // Usage:
        double maxValue = getMaxFromItems(reconstructedItems, attrName);
        std::cout << "Max value for " << attrName << ": " << maxValue << std::endl;
        std::cout << "Reconstructed tuples written to: " << filename << std::endl;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}