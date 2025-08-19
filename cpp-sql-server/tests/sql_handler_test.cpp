#include "../src/sql_handler.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
//#define TEST_SEED 0x13

// Copy the main logic into a function
int run_sql_handler_main_logic() {
    SQLHandler sqlHandler;
    namespace fs = std::filesystem;
    std::vector<int64_t> conditionals;
    std::string outputDir = "../Shamir_Search_Results";
    std::filesystem::create_directory(outputDir);
    std::vector<std::pair<int64_t, int64_t>> secretShares;
    std::pair<Utils::SelectItem, std::vector<Utils::FilterItem>> combinedItems;
    std::vector<std::pair<Utils::SelectItem, std::vector<Utils::FilterItem>>> allItemsList;

    try {
        std::string queriesRoot = "../SQL_Queries";
        for (const auto& dirEntry : fs::recursive_directory_iterator(queriesRoot)) {
            if (dirEntry.is_regular_file() && dirEntry.path().extension() == ".json") {
                std::vector<Utils::SelectItem> selectItems;
                std::vector<Utils::FilterItem> filterItems;
                Utils::parseQueryJson(dirEntry.path().string(), selectItems, filterItems, combinedItems);

                //std::cout << "Parsed: " << dirEntry.path() << std::endl;
                allItemsList.emplace_back(combinedItems);
            }
        }
        for (const auto& item : allItemsList) {
            secretShares.clear();
            conditionals.clear();
            const auto& selectItem = item.first;
            //std::cout << "Select: " << selectItem.query_type << ", " << selectItem.attribute << ", " << selectItem.variable << std::endl;
            for (const auto& filter : item.second) {
                //std::cout << "Filter: " << filter.attribute << " = " << filter.condition << ", whereClause: " << filter.whereClause << std::endl;
                if (!filter.attribute.empty()) {
                    sqlHandler.setAttributeSecrets(filter.attribute);
                }
                if (!filter.condition.empty()) {
                    sqlHandler.setConditionSecrets(static_cast<int64_t>(sqlHandler.stringToInt(filter.condition)));
                    conditionals.emplace_back(static_cast<int64_t>(sqlHandler.stringToInt(filter.condition)));
                }
            }
            for (auto cond : conditionals) {
                //std::cout << "Condition secret: " << cond << std::endl;
                auto shares = sqlHandler.shamirSecretSharing(cond, 6, 3);
                secretShares.insert(secretShares.end(), shares.begin(), shares.end());
            }
            //std::cout << "Count query for attribute: " << selectItem.attribute << std::endl;
            std::ofstream file(outputDir + "/Shares_" + selectItem.query_type + ".txt", std::ios::app);
            if (file.is_open()) {
                for (const auto& share : secretShares) {
                    file << share.first << "|" << share.second << "\n";
                }
                file.close();
            } else {
                std::cerr << "Error opening output file." << std::endl;
                return 1;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

int run_sql_appender_logic() {
    SQLHandler sqlHandler;
    namespace fs = std::filesystem;
    std::string queriesRoot = "../SQL_Queries";

    try {
        for (const auto& dirEntry : fs::recursive_directory_iterator(queriesRoot)) {
            if (dirEntry.is_regular_file() && dirEntry.path().extension() == ".json") {
                // Parse the JSON file
                std::ifstream inFile(dirEntry.path());
                nlohmann::json j;
                inFile >> j;
                inFile.close();

                // Prepare attributeShares for this file
                std::map<std::string, std::vector<std::pair<int64_t, int64_t>>> attributeShares;

                // Use parseQueryJson to get filterItems
                std::vector<Utils::SelectItem> selectItems;
                std::vector<Utils::FilterItem> filterItems;
                std::pair<Utils::SelectItem, std::vector<Utils::FilterItem>> combinedItems;
                Utils::parseQueryJson(dirEntry.path().string(), selectItems, filterItems, combinedItems);

                // For each filter, generate shares if attribute and condition are present
                for (const auto& filter : filterItems) {
                    if (!filter.attribute.empty() && !filter.condition.empty()) {
                        int64_t cond = static_cast<int64_t>(sqlHandler.stringToInt(filter.condition));
                        auto shares = sqlHandler.shamirSecretSharing(cond, 6, 3);
                        attributeShares[filter.attribute] = shares;
                    }
                }

                // Update filters in the JSON object
                for (auto& filter : j["filters"]) {
                    if (filter.contains("attribute") && filter.contains("condition")) {
                        std::string attr = filter["attribute"];
                        if (attributeShares.count(attr)) {
                            nlohmann::json shareID;
                            int idx = 0;
                            for (const auto& share : attributeShares[attr]) {
                                shareID["id_" + std::to_string(idx)] = share.second;
                                idx++;
                            }
                            filter["shareID"] = shareID;
                        }
                    }
                }

                // Write back to file
                std::ofstream outFile(dirEntry.path());
                outFile << j.dump(4);
                outFile.close();

                // Clear attributeShares for next file
                attributeShares.clear();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

TEST(SQLHandlerIntegrationTest, PythonParserRuns) {
    int ret = std::system("python3 ../cpp-sql-server/sql_parser.py");
    ASSERT_EQ(ret, 0); // Check that the script ran successfully

}

TEST(SQLHandlerIntegrationTest, MainLogicRunsWithoutError) {
    ASSERT_EQ(run_sql_handler_main_logic(), 0);
}

TEST(SQLHandlerIntegrationTest, ShareAppender) {
    ASSERT_EQ(run_sql_appender_logic(), 0);
}

int main(int argc, char **argv)
{
    //srand(TEST_SEED);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}