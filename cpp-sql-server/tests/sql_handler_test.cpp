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

// Now add a test that calls this function
TEST(SQLHandlerIntegrationTest, MainLogicRunsWithoutError) {
    ASSERT_EQ(run_sql_handler_main_logic(), 0);
}

int main(int argc, char **argv)
{
    //srand(TEST_SEED);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}