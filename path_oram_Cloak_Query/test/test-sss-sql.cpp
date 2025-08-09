#include "definitions.h"
#include "oram.hpp"
#include "utility.hpp"
#include <filesystem>
#include <mutex>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <math.h>
#include <thread>
#include "../../cpp-sql-server/src/sql_handler.h"
#include "../../cpp-sql-server/src/sql_utils.h"
using json = nlohmann::json;

namespace fs = std::filesystem;

// Load secret shares from the first file found in the ../shares directory
std::vector<std::vector<int64_t>> loadSecretShares(int serverNumber) {
	std::vector<std::vector<int64_t>> allShares;
	
	std::string baseDir = "../shares"; // Relative path to the shares directory

	std::filesystem::path path = std::filesystem::current_path();
		// Debug dialog to check the current path
		//std::cout << "Current path is: " << path << std::endl;

		std::string filePath = baseDir + "/server_" + std::to_string(serverNumber) + ".txt";
		// Pass file path to ifstream
		std::ifstream file(filePath, std::ios::in);

		// Debug dialog to check which server file is being accessed
		//std::cout << "Attempting to open file: " << filePath << std::endl;

		if (file.is_open()) {
			std::string line;
			while (std::getline(file, line)) {
				std::istringstream iss(line);
				std::string y_str;
				std::vector<int64_t> tupleShares;
				while (std::getline(iss, y_str, '|')) {
				    // Trim leading and trailing whitespacet
				    y_str.erase(0, y_str.find_first_not_of(" \t\n\r"));
				    y_str.erase(y_str.find_last_not_of(" \t\n\r") + 1);

				    if (!y_str.empty()) {
				        tupleShares.push_back(std::stoll(y_str));
				    }
				}
				allShares.push_back({tupleShares});
			}
			file.close();
		} else {
			std::cerr << "Error opening file: server_" << serverNumber << ".txt" << std::endl;
	}
	std::cout << "Size of all shares: " << allShares.size() << std::endl;
	return allShares;
}

std::unordered_set<CloakQueryPathORAM::number> loadUsedBlockIDs(const std::string& filename) {
    std::unordered_set<CloakQueryPathORAM::number> ids;
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) return ids;
    uint64_t count;
    ifs.read(reinterpret_cast<char*>(&count), sizeof(count));
    for (uint64_t i = 0; i < count; ++i) {
        CloakQueryPathORAM::number id;
        ifs.read(reinterpret_cast<char*>(&id), sizeof(id));
        ids.insert(id);
    }
    return ids;
}
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

std::mutex timing_metrics_mutex;

void writeTimingMetric(const std::string& testName, long long duration_ms, int server, bool truncate = false) {
    std::lock_guard<std::mutex> lock(timing_metrics_mutex);
    std::ofstream ofs;
    if (truncate)
        ofs.open("../Query_Result_SSS/timing_metrics.txt", std::ios::out | std::ios::trunc);
    else
        ofs.open("../Query_Result_SSS/timing_metrics.txt", std::ios::out | std::ios::app);
    if (ofs.is_open()) {
        ofs << testName << ": " << duration_ms << " ms" << std::endl;
        ofs.close();
    }
}

namespace CloakQueryPathORAM
{
    class ORAMTestSQL : public ::testing::Test
	{
		public:
        std::vector<std::vector<int64_t>> secretShares;
        std::vector<int64_t> filter_ids;
		std::string where_clause;
		std::string query_type;
    };

	TEST_F(ORAMTestSQL, SQLCountORQuery) {
        for (int ser = 1; ser <= 6; ser++) {
            using namespace std::chrono;
            secretShares.clear();
            filter_ids.clear();
            where_clause.clear();
            query_type.clear();
            auto startLoadingSharesTime = std::chrono::high_resolution_clock::now();
            secretShares = loadSecretShares(ser);
            auto loadingSharesTiming = std::chrono::high_resolution_clock::now();
            std::cout << "Time to load secret shares for server " << ser << ": "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(loadingSharesTiming - startLoadingSharesTime).count() << " ms" << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            std::ifstream ifs("../SQL_Queries/COUNT/Status_Flag.json");
            json j;
            ifs >> j;
            std::vector<std::string> attributeIDs;
            std::vector<size_t> attr_idx;

            // Extract id_0 from both filters
            std::string id_key = "id_" + std::to_string(ser - 1);
            for (const auto& filter : j["filters"]) {
                if (filter.contains("shareID") && filter["shareID"].contains(id_key)) {
                    filter_ids.push_back(filter["shareID"][id_key].get<int64_t>());
                }
                if (filter.contains("whereClause")) {
                    where_clause = filter["whereClause"].get<std::string>();
                }
                if (filter.contains("attribute")) {
                    // Store the attribute ID for later use
                    attributeIDs.push_back(filter["attribute"].get<std::string>());
                }
            }
            for (const auto& attr : attributeIDs) {
                attr_idx.emplace_back(attributeIndex(attr)); // Call attributeIndex to ensure it is used
            }

            // Extract query_type from select
            if (!j["select"].empty() && j["select"][0].contains("query_type")) {
                query_type = j["select"][0]["query_type"].get<std::string>();
            }
            ASSERT_EQ(filter_ids.size(), 2) << "Expected two filter IDs, but found: " << filter_ids.size();
            ASSERT_EQ(where_clause, "OR") << "Expected where clause to be 'AND', but found: " << where_clause;
            // Count tuples containing either filter_id
            int count = 0;
            for (const auto& tuple : secretShares) {
            bool found0 = (tuple.size() > attr_idx[0]) && (tuple[attr_idx[0]] == filter_ids[0]);
            bool found1 = (tuple.size() > attr_idx[1]) && (tuple[attr_idx[1]] == filter_ids[1]);
            if (found0 || found1) {
                count++;
                }
            }
            std::cout << "Count of tuples containing either filter_id: " << count << std::endl;
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            writeTimingMetric("SQLCountORQueryServer" + std::to_string(ser), duration_ms, ser);
        }
	}

	TEST_F(ORAMTestSQL, SQLCountANDQuery) {
        for (int ser = 1; ser <= 6; ser++) {
            using namespace std::chrono;
            secretShares.clear();
            filter_ids.clear();
            where_clause.clear();
            query_type.clear();
            auto startLoadingSharesTime = std::chrono::high_resolution_clock::now();
            secretShares = loadSecretShares(ser);
            auto loadingSharesTiming = std::chrono::high_resolution_clock::now();
            std::cout << "Time to load secret shares for server " << ser << ": "
                        << std::chrono::duration_cast<std::chrono::milliseconds>(loadingSharesTiming - startLoadingSharesTime).count() << " ms" << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            std::ifstream ifs("../SQL_Queries/COUNT/Return_Flag.json");
            json j;
            ifs >> j;
            std::vector<std::string> attributeIDs;
            std::vector<size_t> attr_idx;

            // Extract id_0 from both filters
            std::string id_key = "id_" + std::to_string(ser - 1);
            for (const auto& filter : j["filters"]) {
                if (filter.contains("shareID") && filter["shareID"].contains(id_key)) {
                    filter_ids.push_back(filter["shareID"][id_key].get<int64_t>());
                }
                if (filter.contains("whereClause")) {
                    where_clause = filter["whereClause"].get<std::string>();
                }
                if (filter.contains("attribute")) {
                    // Store the attribute ID for later use
                    attributeIDs.push_back(filter["attribute"].get<std::string>());
                }
            }
            for (const auto& attr : attributeIDs) {
                attr_idx.emplace_back(attributeIndex(attr)); // Call attributeIndex to ensure it is used
            }

            // Extract query_type from select
            if (!j["select"].empty() && j["select"][0].contains("query_type")) {
                query_type = j["select"][0]["query_type"].get<std::string>();
            }
            ASSERT_EQ(filter_ids.size(), 2) << "Expected two filter IDs, but found: " << filter_ids.size();
            ASSERT_EQ(where_clause, "AND") << "Expected where clause to be 'AND', but found: " << where_clause;
            // Count tuples containing either filter_id
            int count = 0;
            for (const auto& tuple : secretShares) {
            bool found0 = (tuple.size() > attr_idx[0]) && (tuple[attr_idx[0]] == filter_ids[0]);
            bool found1 = (tuple.size() > attr_idx[1]) && (tuple[attr_idx[1]] == filter_ids[1]);
                if (found0 && found1) {
                count++;
                }
            }
            std::cout << "Count of tuples containing both filter_ids: " << count << std::endl;
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            writeTimingMetric("SQLCountANDQueryServer" + std::to_string(ser), duration_ms, ser);
        }
	}

	TEST_F(ORAMTestSQL, SQLSUMORQuery) {
        for (int ser = 1; ser <= 6; ser++) {
            using namespace std::chrono;
            secretShares.clear();
            filter_ids.clear();
            where_clause.clear();
            query_type.clear();
            auto startLoadingSharesTime = std::chrono::high_resolution_clock::now();
            secretShares = loadSecretShares(ser);
            auto loadingSharesTiming = std::chrono::high_resolution_clock::now();
            std::cout << "Time to load secret shares for server " << ser << ": "
                        << std::chrono::duration_cast<std::chrono::milliseconds>(loadingSharesTiming - startLoadingSharesTime).count() << " ms" << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            std::ifstream ifs("../SQL_Queries/SUM/ExtendedPrice.json");
            json j;
            ifs >> j;
            std::vector<std::string> attributeIDs;
            std::vector<size_t> attr_idx;
            std::string resultDir = "../Query_Result_SSS/SUMOR";
            std::filesystem::create_directories(resultDir); // Ensure the folder exists
            std::ofstream outFile(resultDir + "/server_" + std::to_string(ser) + ".txt");

            // Extract id_0 from both filters
            std::string id_key = "id_" + std::to_string(ser - 1);
            for (const auto& filter : j["filters"]) {
                if (filter.contains("shareID") && filter["shareID"].contains(id_key)) {
                    filter_ids.push_back(filter["shareID"][id_key].get<int64_t>());
                }
                if (filter.contains("whereClause")) {
                    where_clause = filter["whereClause"].get<std::string>();
                }
                if (filter.contains("attribute")) {
                    // Store the attribute ID for later use
                    attributeIDs.push_back(filter["attribute"].get<std::string>());
                }
            }
            for (const auto& attr : attributeIDs) {
                attr_idx.emplace_back(attributeIndex(attr)); // Call attributeIndex to ensure it is used
            }

            // Extract query_type from select
            if (!j["select"].empty() && j["select"][0].contains("query_type")) {
                query_type = j["select"][0]["query_type"].get<std::string>();
            }
            ASSERT_EQ(filter_ids.size(), 2) << "Expected two filter IDs, but found: " << filter_ids.size();
            ASSERT_EQ(where_clause, "OR") << "Expected where clause to be 'AND', but found: " << where_clause;
            // Count tuples containing either filter_id
            int count = 0;
            for (const auto& tuple : secretShares) {
            bool found0 = (tuple.size() > attr_idx[0]) && (tuple[attr_idx[0]] == filter_ids[0]);
            bool found1 = (tuple.size() > attr_idx[1]) && (tuple[attr_idx[1]] == filter_ids[1]);
                if (found0 || found1) {
                count++;
                // Write tuple to file
                    for (size_t i = 0; i < tuple.size(); ++i) {
                        outFile << tuple[i];
                        if (i < tuple.size() - 1) outFile << " | ";
                    }
                outFile << "\n";
                }
            }	
            outFile.close();
            std::cout << "Count of tuples containing either filter_id in SUM Query: " << count << std::endl;
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            writeTimingMetric("SQLSUMORQueryServer" + std::to_string(ser), duration_ms, ser);
        }
	}

	TEST_F(ORAMTestSQL, SQLSUMANDQuery) {
        for (int ser = 1; ser <= 6; ser++) {
            using namespace std::chrono;
            secretShares.clear();
            filter_ids.clear();
            where_clause.clear();
            query_type.clear();
            auto startLoadingSharesTime = std::chrono::high_resolution_clock::now();
            secretShares = loadSecretShares(ser);
            auto loadingSharesTiming = std::chrono::high_resolution_clock::now();
            std::cout << "Time to load secret shares for server " << ser << ": "
                        << std::chrono::duration_cast<std::chrono::milliseconds>(loadingSharesTiming - startLoadingSharesTime).count() << " ms" << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            std::ifstream ifs("../SQL_Queries/SUM/Quantity.json");
            json j;
            ifs >> j;
            std::vector<std::string> attributeIDs;
            std::vector<size_t> attr_idx;
            std::string resultDir = "../Query_Result_SSS/SUMAND";
            std::filesystem::create_directories(resultDir); // Ensure the folder exists
            std::ofstream outFile(resultDir + "/server_" + std::to_string(ser) + ".txt");

            // Extract id_0 from both filters
            std::string id_key = "id_" + std::to_string(ser - 1);
            for (const auto& filter : j["filters"]) {
                if (filter.contains("shareID") && filter["shareID"].contains(id_key)) {
                    filter_ids.push_back(filter["shareID"][id_key].get<int64_t>());
                }
                if (filter.contains("whereClause")) {
                    where_clause = filter["whereClause"].get<std::string>();
                }
                if (filter.contains("attribute")) {
                    // Store the attribute ID for later use
                    attributeIDs.push_back(filter["attribute"].get<std::string>());
                }
            }
            for (const auto& attr : attributeIDs) {
                attr_idx.emplace_back(attributeIndex(attr)); // Call attributeIndex to ensure it is used
            }

            // Extract query_type from select
            if (!j["select"].empty() && j["select"][0].contains("query_type")) {
                query_type = j["select"][0]["query_type"].get<std::string>();
            }
            ASSERT_EQ(filter_ids.size(), 2) << "Expected two filter IDs, but found: " << filter_ids.size();
            ASSERT_EQ(where_clause, "AND") << "Expected where clause to be 'AND', but found: " << where_clause;
            // Count tuples containing either filter_id
            int count = 0;
            for (const auto& tuple : secretShares) {
            bool found0 = (tuple.size() > attr_idx[0]) && (tuple[attr_idx[0]] == filter_ids[0]);
            bool found1 = (tuple.size() > attr_idx[1]) && (tuple[attr_idx[1]] == filter_ids[1]);
                if (found0 && found1) {
                count++;
                // Write tuple to file
                    for (size_t i = 0; i < tuple.size(); ++i) {
                        outFile << tuple[i];
                        if (i < tuple.size() - 1) outFile << " | ";
                    }
                outFile << "\n";
                }
            }	
            outFile.close();
            std::cout << "Count of tuples containing both filter_id in SUM Query: " << count << std::endl;
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            writeTimingMetric("SQLSUMANDQueryServer" + std::to_string(ser), duration_ms, ser);
        }
	}

	TEST_F(ORAMTestSQL, SQLAVGORQuery) {
        for (int ser = 1; ser <= 6; ser++) {
            using namespace std::chrono;
            secretShares.clear();
            filter_ids.clear();
            where_clause.clear();
            query_type.clear();
            auto startLoadingSharesTime = std::chrono::high_resolution_clock::now();
            secretShares = loadSecretShares(ser);
            auto loadingSharesTiming = std::chrono::high_resolution_clock::now();
            std::cout << "Time to load secret shares for server " << ser << ": "
                        << std::chrono::duration_cast<std::chrono::milliseconds>(loadingSharesTiming - startLoadingSharesTime).count() << " ms" << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            std::ifstream ifs("../SQL_Queries/AVG/Quantity.json");
            json j;
            ifs >> j;
            std::vector<std::string> attributeIDs;
            std::vector<size_t> attr_idx;
            std::string resultDir = "../Query_Result_SSS/AVGOR";
            std::filesystem::create_directories(resultDir); // Ensure the folder exists
            std::ofstream outFile(resultDir + "/server_" + std::to_string(ser) + ".txt");

            // Extract id_0 from both filters
            std::string id_key = "id_" + std::to_string(ser - 1);
            for (const auto& filter : j["filters"]) {
                if (filter.contains("shareID") && filter["shareID"].contains(id_key)) {
                    filter_ids.push_back(filter["shareID"][id_key].get<int64_t>());
                }
                if (filter.contains("whereClause")) {
                    where_clause = filter["whereClause"].get<std::string>();
                }
                if (filter.contains("attribute")) {
                    // Store the attribute ID for later use
                    attributeIDs.push_back(filter["attribute"].get<std::string>());
                }
            }
            for (const auto& attr : attributeIDs) {
                attr_idx.emplace_back(attributeIndex(attr)); // Call attributeIndex to ensure it is used
            }

            // Extract query_type from select
            if (!j["select"].empty() && j["select"][0].contains("query_type")) {
                query_type = j["select"][0]["query_type"].get<std::string>();
            }
            ASSERT_EQ(filter_ids.size(), 2) << "Expected two filter IDs, but found: " << filter_ids.size();
            ASSERT_EQ(where_clause, "OR") << "Expected where clause to be 'OR', but found: " << where_clause;
            // Count tuples containing either filter_id
            int count = 0;
            for (const auto& tuple : secretShares) {
            bool found0 = (tuple.size() > attr_idx[0]) && (tuple[attr_idx[0]] == filter_ids[0]);
            bool found1 = (tuple.size() > attr_idx[1]) && (tuple[attr_idx[1]] == filter_ids[1]);
                if (found0 || found1) {
                count++;
                // Write tuple to file
                    for (size_t i = 0; i < tuple.size(); ++i) {
                        outFile << tuple[i];
                        if (i < tuple.size() - 1) outFile << " | ";
                    }
                outFile << "\n";
                }
            }	
            outFile.close();
            std::cout << "Count of tuples containing either filter_id in AVG Query: " << count << std::endl;
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            writeTimingMetric("SQLAVGORQueryServer" + std::to_string(ser), duration_ms, ser);
        }
	}

	TEST_F(ORAMTestSQL, SQLAVGANDQuery) {
        for (int ser = 1; ser <= 6; ser++) {
            using namespace std::chrono;
            secretShares.clear();
            filter_ids.clear();
            where_clause.clear();
            query_type.clear();
            auto startLoadingSharesTime = std::chrono::high_resolution_clock::now();
            secretShares = loadSecretShares(ser);
            auto loadingSharesTiming = std::chrono::high_resolution_clock::now();
            std::cout << "Time to load secret shares for server " << ser << ": "
                        << std::chrono::duration_cast<std::chrono::milliseconds>(loadingSharesTiming - startLoadingSharesTime).count() << " ms" << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            std::ifstream ifs("../SQL_Queries/AVG/Discount.json");
            json j;
            ifs >> j;
            std::vector<std::string> attributeIDs;
            std::vector<size_t> attr_idx;
            std::string resultDir = "../Query_Result_SSS/AVGAND";
            std::filesystem::create_directories(resultDir); // Ensure the folder exists
            std::ofstream outFile(resultDir + "/server_" + std::to_string(ser) + ".txt");

            // Extract id_0 from both filters
            std::string id_key = "id_" + std::to_string(ser - 1);
            for (const auto& filter : j["filters"]) {
                if (filter.contains("shareID") && filter["shareID"].contains(id_key)) {
                    filter_ids.push_back(filter["shareID"][id_key].get<int64_t>());
                }
                if (filter.contains("whereClause")) {
                    where_clause = filter["whereClause"].get<std::string>();
                }
                if (filter.contains("attribute")) {
                    // Store the attribute ID for later use
                    attributeIDs.push_back(filter["attribute"].get<std::string>());
                }
            }
            for (const auto& attr : attributeIDs) {
                attr_idx.emplace_back(attributeIndex(attr)); // Call attributeIndex to ensure it is used
            }

            // Extract query_type from select
            if (!j["select"].empty() && j["select"][0].contains("query_type")) {
                query_type = j["select"][0]["query_type"].get<std::string>();
            }
            ASSERT_EQ(filter_ids.size(), 2) << "Expected two filter IDs, but found: " << filter_ids.size();
            ASSERT_EQ(where_clause, "AND") << "Expected where clause to be 'AND', but found: " << where_clause;
            // Count tuples containing either filter_id
            int count = 0;
            for (const auto& tuple : secretShares) {
            bool found0 = (tuple.size() > attr_idx[0]) && (tuple[attr_idx[0]] == filter_ids[0]);
            bool found1 = (tuple.size() > attr_idx[1]) && (tuple[attr_idx[1]] == filter_ids[1]);
                if (found0 && found1) {
                count++;
                // Write tuple to file
                    for (size_t i = 0; i < tuple.size(); ++i) {
                        outFile << tuple[i];
                        if (i < tuple.size() - 1) outFile << " | ";
                    }
                outFile << "\n";
                }
            }	
            outFile.close();
            std::cout << "Count of tuples containing both filter_id in AVG Query: " << count << std::endl;
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            writeTimingMetric("SQLAVGANDQueryServer" + std::to_string(ser), duration_ms, ser);
        }
	}
	
	TEST_F(ORAMTestSQL, SQLMINORQuery) {
        for (int ser = 1; ser <= 6; ser++) {
            using namespace std::chrono;
            secretShares.clear();
            filter_ids.clear();
            where_clause.clear();
            query_type.clear();
            auto startLoadingSharesTime = std::chrono::high_resolution_clock::now();
            secretShares = loadSecretShares(ser);
            auto loadingSharesTiming = std::chrono::high_resolution_clock::now();
            std::cout << "Time to load secret shares for server " << ser << ": "
                        << std::chrono::duration_cast<std::chrono::milliseconds>(loadingSharesTiming - startLoadingSharesTime).count() << " ms" << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            std::ifstream ifs("../SQL_Queries/MIN/ExtendedPrice_Min.json");
            json j;
            ifs >> j;
            std::vector<std::string> attributeIDs;
            std::vector<size_t> attr_idx;
            std::string resultDir = "../Query_Result_SSS/MINOR";
            std::filesystem::create_directories(resultDir); // Ensure the folder exists
            std::ofstream outFile(resultDir + "/server_" + std::to_string(ser) + ".txt");

            // Extract id_0 from both filters
            std::string id_key = "id_" + std::to_string(ser - 1);
            for (const auto& filter : j["filters"]) {
                if (filter.contains("shareID") && filter["shareID"].contains(id_key)) {
                    filter_ids.push_back(filter["shareID"][id_key].get<int64_t>());
                }
                if (filter.contains("whereClause")) {
                    where_clause = filter["whereClause"].get<std::string>();
                }
                if (filter.contains("attribute")) {
                    // Store the attribute ID for later use
                    attributeIDs.push_back(filter["attribute"].get<std::string>());
                }
            }
            for (const auto& attr : attributeIDs) {
                attr_idx.emplace_back(attributeIndex(attr)); // Call attributeIndex to ensure it is used
            }

            // Extract query_type from select
            if (!j["select"].empty() && j["select"][0].contains("query_type")) {
                query_type = j["select"][0]["query_type"].get<std::string>();
            }
            ASSERT_EQ(filter_ids.size(), 2) << "Expected two filter IDs, but found: " << filter_ids.size();
            ASSERT_EQ(where_clause, "OR") << "Expected where clause to be 'OR', but found: " << where_clause;
            // Count tuples containing either filter_id
            int count = 0;
            for (const auto& tuple : secretShares) {
            bool found0 = (tuple.size() > attr_idx[0]) && (tuple[attr_idx[0]] == filter_ids[0]);
            bool found1 = (tuple.size() > attr_idx[1]) && (tuple[attr_idx[1]] == filter_ids[1]);
                if (found0 || found1) {
                count++;
                // Write tuple to file
                    for (size_t i = 0; i < tuple.size(); ++i) {
                        outFile << tuple[i];
                        if (i < tuple.size() - 1) outFile << " | ";
                    }
                outFile << "\n";
                }
            }	
            outFile.close();
            std::cout << "Count of tuples containing either filter_id in MIN Query: " << count << std::endl;
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            writeTimingMetric("SQLMINORQueryServer" + std::to_string(ser), duration_ms, ser);
        }
	}

	TEST_F(ORAMTestSQL, SQLMINANDQuery) {
        for (int ser = 1; ser <= 6; ser++) {
            using namespace std::chrono;
            secretShares.clear();
            filter_ids.clear();
            where_clause.clear();
            query_type.clear();
            auto startLoadingSharesTime = std::chrono::high_resolution_clock::now();
            secretShares = loadSecretShares(ser);
            auto loadingSharesTiming = std::chrono::high_resolution_clock::now();
            std::cout << "Time to load secret shares for server " << ser << ": "
                        << std::chrono::duration_cast<std::chrono::milliseconds>(loadingSharesTiming - startLoadingSharesTime).count() << " ms" << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            std::ifstream ifs("../SQL_Queries/MIN/Tax_Min.json");
            json j;
            ifs >> j;
            std::vector<std::string> attributeIDs;
            std::vector<size_t> attr_idx;
            std::string resultDir = "../Query_Result_SSS/MINAND";
            std::filesystem::create_directories(resultDir); // Ensure the folder exists
            std::ofstream outFile(resultDir + "/server_" + std::to_string(ser) + ".txt");

            // Extract id_0 from both filters
            std::string id_key = "id_" + std::to_string(ser - 1);
            for (const auto& filter : j["filters"]) {
                if (filter.contains("shareID") && filter["shareID"].contains(id_key)) {
                    filter_ids.push_back(filter["shareID"][id_key].get<int64_t>());
                }
                if (filter.contains("whereClause")) {
                    where_clause = filter["whereClause"].get<std::string>();
                }
                if (filter.contains("attribute")) {
                    // Store the attribute ID for later use
                    attributeIDs.push_back(filter["attribute"].get<std::string>());
                }
            }
            for (const auto& attr : attributeIDs) {
                attr_idx.emplace_back(attributeIndex(attr)); // Call attributeIndex to ensure it is used
            }

            // Extract query_type from select
            if (!j["select"].empty() && j["select"][0].contains("query_type")) {
                query_type = j["select"][0]["query_type"].get<std::string>();
            }
            ASSERT_EQ(filter_ids.size(), 2) << "Expected two filter IDs, but found: " << filter_ids.size();
            ASSERT_EQ(where_clause, "AND") << "Expected where clause to be 'AND', but found: " << where_clause;
            // Count tuples containing either filter_id
            int count = 0;
            for (const auto& tuple : secretShares) {
            bool found0 = (tuple.size() > attr_idx[0]) && (tuple[attr_idx[0]] == filter_ids[0]);
            bool found1 = (tuple.size() > attr_idx[1]) && (tuple[attr_idx[1]] == filter_ids[1]);
                if (found0 && found1) {
                count++;
                // Write tuple to file
                    for (size_t i = 0; i < tuple.size(); ++i) {
                        outFile << tuple[i];
                        if (i < tuple.size() - 1) outFile << " | ";
                    }
                outFile << "\n";
                }
            }	
            outFile.close();
            std::cout << "Count of tuples containing both filter_id in MIN Query: " << count << std::endl;
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            writeTimingMetric("SQLMINANDQueryServer" + std::to_string(ser), duration_ms, ser);
        }
	}

	TEST_F(ORAMTestSQL, SQLMAXORQuery) {
        for (int ser = 1; ser <= 6; ser++) {
            using namespace std::chrono;
            secretShares.clear();
            filter_ids.clear();
            where_clause.clear();
            query_type.clear();
            auto startLoadingSharesTime = std::chrono::high_resolution_clock::now();
            secretShares = loadSecretShares(ser);
            std::cout << "Size for secret shares for server " << ser << ": " << secretShares.size() << std::endl;
            auto loadingSharesTiming = std::chrono::high_resolution_clock::now();
            std::cout << "Time to load secret shares for server " << ser << ": "
                        << std::chrono::duration_cast<std::chrono::milliseconds>(loadingSharesTiming - startLoadingSharesTime).count() << " ms" << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            std::ifstream ifs("../SQL_Queries/MAX/ExtendedPrice_Max.json");
            json j;
            ifs >> j;
            std::vector<std::string> attributeIDs;
            std::vector<size_t> attr_idx;
            std::string resultDir = "../Query_Result_SSS/MAXOR";
            std::filesystem::create_directories(resultDir); // Ensure the folder exists
            std::ofstream outFile(resultDir + "/server_" + std::to_string(ser) + ".txt");

            // Extract id_0 from both filters
            std::string id_key = "id_" + std::to_string(ser - 1);
            for (const auto& filter : j["filters"]) {
                if (filter.contains("shareID") && filter["shareID"].contains(id_key)) {
                    filter_ids.push_back(filter["shareID"][id_key].get<int64_t>());
                }
                if (filter.contains("whereClause")) {
                    where_clause = filter["whereClause"].get<std::string>();
                }
                if (filter.contains("attribute")) {
                    // Store the attribute ID for later use
                    attributeIDs.push_back(filter["attribute"].get<std::string>());
                }
            }
            for (const auto& attr : attributeIDs) {
                attr_idx.emplace_back(attributeIndex(attr)); // Call attributeIndex to ensure it is used
            }

            // Extract query_type from select
            if (!j["select"].empty() && j["select"][0].contains("query_type")) {
                query_type = j["select"][0]["query_type"].get<std::string>();
            }
            ASSERT_EQ(filter_ids.size(), 2) << "Expected two filter IDs, but found: " << filter_ids.size();
            ASSERT_EQ(where_clause, "OR") << "Expected where clause to be 'OR', but found: " << where_clause;
            // Count tuples containing either filter_id
            int count = 0;
            for (const auto& tuple : secretShares) {
            bool found0 = (tuple.size() > attr_idx[0]) && (tuple[attr_idx[0]] == filter_ids[0]);
            bool found1 = (tuple.size() > attr_idx[1]) && (tuple[attr_idx[1]] == filter_ids[1]);
                if (found0 || found1) {
                count++;
                // Write tuple to file
                    for (size_t i = 0; i < tuple.size(); ++i) {
                        outFile << tuple[i];
                        if (i < tuple.size() - 1) outFile << " | ";
                    }
                outFile << "\n";
                }
            }	
            outFile.close();
            std::cout << "Count of tuples containing either filter_id in MAX Query: " << count << std::endl;
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            writeTimingMetric("SQLMAXORQueryServer" + std::to_string(ser), duration_ms, ser);
        }
	}

	TEST_F(ORAMTestSQL, SQLMAXANDQuery) {
        for (int ser = 1; ser <= 6; ser++) {
            using namespace std::chrono;
            secretShares.clear();
            filter_ids.clear();
            where_clause.clear();
            query_type.clear();
            auto startLoadingSharesTime = std::chrono::high_resolution_clock::now();
            secretShares = loadSecretShares(ser);
            std::cout << "Size for secret shares for server " << ser << ": " << secretShares.size() << std::endl;
            auto loadingSharesTiming = std::chrono::high_resolution_clock::now();
            std::cout << "Time to load secret shares for server " << ser << ": "
                        << std::chrono::duration_cast<std::chrono::milliseconds>(loadingSharesTiming - startLoadingSharesTime).count() << " ms" << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            std::ifstream ifs("../SQL_Queries/MAX/Tax_Max.json");
            json j;
            ifs >> j;
            std::vector<std::string> attributeIDs;
            std::vector<size_t> attr_idx;
            std::string resultDir = "../Query_Result_SSS/MAXAND";
            std::filesystem::create_directories(resultDir); // Ensure the folder exists
            std::ofstream outFile(resultDir + "/server_" + std::to_string(ser) + ".txt");

            // Extract id_0 from both filters
            std::string id_key = "id_" + std::to_string(ser - 1);
            for (const auto& filter : j["filters"]) {
                if (filter.contains("shareID") && filter["shareID"].contains(id_key)) {
                    filter_ids.push_back(filter["shareID"][id_key].get<int64_t>());
                }
                if (filter.contains("whereClause")) {
                    where_clause = filter["whereClause"].get<std::string>();
                }
                if (filter.contains("attribute")) {
                    // Store the attribute ID for later use
                    attributeIDs.push_back(filter["attribute"].get<std::string>());
                }
            }
            for (const auto& attr : attributeIDs) {
                attr_idx.emplace_back(attributeIndex(attr)); // Call attributeIndex to ensure it is used
            }

            // Extract query_type from select
            if (!j["select"].empty() && j["select"][0].contains("query_type")) {
                query_type = j["select"][0]["query_type"].get<std::string>();
            }
            ASSERT_EQ(filter_ids.size(), 2) << "Expected two filter IDs, but found: " << filter_ids.size();
            ASSERT_EQ(where_clause, "AND") << "Expected where clause to be 'AND', but found: " << where_clause;
            // Count tuples containing either filter_id
            int count = 0;
            for (const auto& tuple : secretShares) {
            bool found0 = (tuple.size() > attr_idx[0]) && (tuple[attr_idx[0]] == filter_ids[0]);
            bool found1 = (tuple.size() > attr_idx[1]) && (tuple[attr_idx[1]] == filter_ids[1]);
                if (found0 && found1) {
                count++;
                // Write tuple to file
                    for (size_t i = 0; i < tuple.size(); ++i) {
                        outFile << tuple[i];
                        if (i < tuple.size() - 1) outFile << " | ";
                    }
                outFile << "\n";
                }
            }	
            outFile.close();
            std::cout << "Count of tuples containing both filter_id in MAX Query: " << count << std::endl;
            auto end = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            writeTimingMetric("SQLMAXANDQueryServer" + std::to_string(ser), duration_ms, ser);
        }
	}
}


int main(int argc, char **argv)
{
	srand(TEST_SEED);

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}