#include "definitions.h"
#include "oram.hpp"
#include "utility.hpp"
#include <filesystem>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <math.h>
#include <thread>
#include <mutex>
#include "../../cpp-sql-server/src/sql_handler.h"
#include "../../cpp-sql-server/src/sql_utils.h"
using json = nlohmann::json;

namespace fs = std::filesystem;
// Global variable to hold the retrieved shares, ALWAYS reset before populating
std::vector<std::vector<int64_t>> retrievedShares_global;
// Load secret shares from the first file found in the ../shares directory
std::vector<std::vector<int64_t>> loadSecretShares(int serverNumber) {
	std::vector<std::vector<int64_t>> allShares;
	
	std::string baseDir = "../shares"; // Relative path to the shares directory

	std::filesystem::path path = std::filesystem::current_path();

		std::string filePath = baseDir + "/server_" + std::to_string(serverNumber) + ".txt";
		// Pass file path to ifstream
		std::ifstream file(filePath, std::ios::in);

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

/*
 * This struct is used to store the timing details of various operations in the ORAM test.
 * It includes the following fields:
 * puttingShares for the time taken to put shares
 * gettingShares for the time taken to get shares
 * queryTranslation for the time taken to translate queries
 * shufflePaths for the time taken to shuffle paths
 * integrityCheck for the time taken to perform integrity checks.
 */
struct timingDetails {
	long long puttingShares; // Time taken to put shares in ms.
	long long gettingShares; // Time taken to get shares in ms.
	long long queryTranslation; // Time taken for translating the query into read write queries.
	long long shufflePaths; // Time taken to shuffle the ORAM paths.
	long long integrityCheck; // Time taken for integrity check.
	std::string testName; // Test name.
};

std::mutex timing_metrics_mutex;

void writeTimingMetric(const timingDetails& details, bool truncate = false) {
    std::lock_guard<std::mutex> lock(timing_metrics_mutex);
    std::ofstream ofs;
    if (truncate) {
		ofs.open("../backup_sql/timing_metrics.txt", std::ios::out | std::ios::trunc);
	} else {
        ofs.open("../backup_sql/timing_metrics.txt", std::ios::out | std::ios::app);
	}
	if (ofs.is_open() && details.testName != "") {
		if (details.shufflePaths != 0)
			ofs << details.testName << " Shuffle Paths: " << details.shufflePaths << " ms" << std::endl;
		if (details.integrityCheck != 0)
			ofs << details.testName << " Integrity Check: " << details.integrityCheck << " ms" << std::endl;
		if (details.puttingShares != 0)
			ofs << details.testName << " Putting Shares: " << details.puttingShares << " ms" << std::endl;
		if (details.gettingShares != 0)
			ofs << details.testName << " Getting Shares: " << details.gettingShares << " ms" << std::endl;
		if (details.queryTranslation != 0)
			ofs << details.testName << " Query Translation: " << details.queryTranslation << " ms" << std::endl;
		ofs.close();
	}
}
namespace CloakQueryPathORAM
{
    class ORAMTestSQL : public ::testing::Test
	{
		public:
		number LOG_CAPACITY;
		number CAPACITY;
		number totalBlocks;
		number totalBuckets;
		std::vector<int64_t> filter_ids;
		std::string where_clause;
		std::string query_type;

		protected:
		inline static const number Z			= 3; // Number of blocks per bucket
		inline static const number BLOCK_SIZE	= 140800;
		inline static const number BATCH_SIZE	= 10; // Number of requests to process at a time
		inline static bytes KEY; // AES key for encryption operations
        inline static size_t commonSecretShareSize = 0; // Size of the secret shares, to be set in initialize()

		protected:

        // Change initialize to return all components for each ORAM instance
		std::tuple<
			std::shared_ptr<AbsStorageAdapter>,
			std::shared_ptr<AbsPositionMapAdapter>,
			std::shared_ptr<AbsStashAdapter>,
			std::unique_ptr<ORAM>
		>
		initialize(size_t secretSharesSize, int serverIndex = 0)
		{
			KEY = getRandomBlock(KEYSIZE);
			// Calculate the total number of blocks and buckets
			totalBlocks = (secretSharesSize / 1000) + 1; 
			totalBuckets = (totalBlocks / Z) + 1;		
			//CAPACITY = totalBuckets * Z; 
			// This is used to calculate the height of the ORAM tree. 
			// For example, if CAPACITY = 32, then LOG_CAPACITY = 5
			// Logarithm base 2 of the capacity
			LOG_CAPACITY = static_cast<number>(ceil(log2(totalBuckets))); 
			CAPACITY = (1 << LOG_CAPACITY);	 // Total number of buckets in the ORAM
			std::cout << "Called in initialize with key size: " << KEY.size() << std::endl;
			std::cout << "CAPACITY: " << CAPACITY << ", Z: " << Z << ", map size: " << (CAPACITY * Z + Z) << std::endl;
			// Use the same backupDir as backupGenerator()
			std::string backupDir = "../backup_sql";
			if (!std::filesystem::exists(backupDir)) {
				std::filesystem::create_directory(backupDir);
			}
			std::string uniqueFilename = backupDir + "/storage_server_" + std::to_string(serverIndex) + ".bin";

			auto storage = shared_ptr<AbsStorageAdapter>(new FileSystemStorageAdapter(totalBlocks + 10, BLOCK_SIZE, KEY, uniqueFilename, true, Z));
			auto map = shared_ptr<AbsPositionMapAdapter>(new InMemoryPositionMapAdapter(totalBlocks * Z + Z));
			auto stash = make_shared<InMemoryStashAdapter>(totalBlocks + 10); // Add safety margin

			auto oram = std::make_unique<ORAM>(
				LOG_CAPACITY,
				BLOCK_SIZE,
				Z,
				storage,
				map,
				stash,
				true,
				BATCH_SIZE);

			return {storage, map, stash, std::move(oram)};
		}

		std::tuple<
			std::shared_ptr<AbsStorageAdapter>,
			std::shared_ptr<AbsPositionMapAdapter>,
			std::shared_ptr<AbsStashAdapter>,
			std::unique_ptr<ORAM>
		>
		initializeFromBackup (size_t secretSharesSize, int serverIndex) {
			// Store path to the backup directory
			std::string backupDir = "../backup_sql";
			std::string keyFile = backupDir + "/key_server_0.bin";
			std::string posmapFile = backupDir + "/position-map_server_0.bin";
			std::string stashFile = backupDir + "/stash_server_0.bin";
			std::string storageFile = backupDir + "/storage_server_0.bin";
			totalBlocks = (secretSharesSize / 1000) + 1; 
			totalBuckets = (totalBlocks / Z) + 1;        
			LOG_CAPACITY = static_cast<number>(ceil(log2(totalBuckets))); 
			CAPACITY = (1 << LOG_CAPACITY); // Total number of buckets in the ORAM


			// Reconstruct the ORAM instance from the backup files
			if (!fs::exists(storageFile) || !fs::exists(keyFile) || !fs::exists(posmapFile) || !fs::exists(stashFile)) {
				std::cerr << "Backup files do not exist. Please run the PutContainerSingleORAM test first." << std::endl;
				return {};
			}

			KEY = loadKey(keyFile);
			// Loading storage
			auto storage = std::make_shared<FileSystemStorageAdapter>(totalBlocks + 10, BLOCK_SIZE, KEY, storageFile, false, Z);
			
			// Loading map
			auto map = std::make_shared<InMemoryPositionMapAdapter>(totalBlocks * Z + Z); // Use totalBlocks * Z + Z for the map size
			dynamic_pointer_cast<InMemoryPositionMapAdapter>(map)->loadFromFile(posmapFile);

			// Loading stash
			// Commented out code below can be used for calculating the block size of the stash
			// vector<block> stashDump;
			// stashVar->getAll(stashDump);
			// auto blockSize = stashDump.size() > 0 ? stashDump[0].second.size() : 0;
			auto stash = make_shared<InMemoryStashAdapter>(totalBlocks + 10); // Add safety margin
			if (fs::file_size(stashFile) > 0) {
				// File is not empty, infer block size as before
				dynamic_pointer_cast<InMemoryStashAdapter>(stash)->loadFromFile(stashFile, BLOCK_SIZE); // or infer from file
				std::cout << "Loaded stash with block size: " << BLOCK_SIZE << std::endl;
			} else {
				// File is empty, stash remains empty with correct block size
			}

			// Reconstruct the ORAM instance
			auto oram = make_unique<ORAM>(
				LOG_CAPACITY,
				BLOCK_SIZE,
				Z,
				storage,
				map,
				stash,
				false,
				BATCH_SIZE);

			return {storage, map, stash, std::move(oram)};
		}
        
        // Updated populateStorage to accept storage, stash, map
		void populateStorage(const shared_ptr<AbsStorageAdapter>& storageAdapter)
		{
			std::cout << "Populating storage with " << CAPACITY << " buckets..." << std::endl;
			for (number i = 0; i < (CAPACITY + Z); i++)
			{
				bucket bucket;
				for (auto j = 0uLL; j < Z; j++)
				{
					bucket.push_back({i * Z + j, bytes(BLOCK_SIZE, 0)}); // Always use correct block size
				}
				storageAdapter->set(i, bucket);
			}
		}

        // Change backupGenerator to take explicit arguments
		void backupGenerator(
		 std::shared_ptr<AbsStorageAdapter>& storage,
		 std::shared_ptr<AbsPositionMapAdapter>& map,
		 std::shared_ptr<AbsStashAdapter>& stash,
		 std::unique_ptr<ORAM>& oram,
		int serverIndex)
		{
			std::string backupDir = "../backup_sql";
			if (!std::filesystem::exists(backupDir)) {
				std::filesystem::create_directory(backupDir);
			}
			std::string keyFile = backupDir + "/key_server_" + std::to_string(serverIndex) + ".bin";
			std::string posmapFile = backupDir + "/position-map_server_" + std::to_string(serverIndex) + ".bin";
			std::string stashFile = backupDir + "/stash_server_" + std::to_string(serverIndex) + ".bin";
			std::string usedBlockIDsFile = backupDir + "/used_block_ids_server_" + std::to_string(serverIndex) + ".bin";
			std::string macMapFile = backupDir + "/mac_map_server_" + std::to_string(serverIndex) + ".bin";

			std::ofstream ofs(usedBlockIDsFile, std::ios::binary);
			uint64_t count = oram->getUsedBlockIDs().size();
			ofs.write(reinterpret_cast<const char*>(&count), sizeof(count));
			for (auto id : oram->getUsedBlockIDs()) {
				ofs.write(reinterpret_cast<const char*>(&id), sizeof(id));
			}
			storage.reset(); // Reset storage to release resources
			// Save key
			storeKey(oram->getKey(), keyFile);

			// Save position map
			dynamic_pointer_cast<InMemoryPositionMapAdapter>(map)->storeToFile(posmapFile);
			//map.reset();
			
			// Save stash
			dynamic_pointer_cast<InMemoryStashAdapter>(stash)->storeToFile(stashFile);
			stash.reset();
			

			// Save MAC map
			oram->saveMacMap(macMapFile);
			//oram.reset();
		}
		
		std::unique_ptr<ORAM> loadORAMAndShares() {
			std::vector<std::vector<int64_t>> retrievedShares;
			auto [storage, map, stash, oram] = initializeFromBackup(commonSecretShareSize, 0);
			std::string usedBlockIDsFile = "../backup_sql/used_block_ids_server_0.bin";
			oram->resetTimingMetrics(); // Reset the timing metrics before starting the test
			std::unordered_set<number> usedBlockIDs = loadUsedBlockIDs(usedBlockIDsFile);
			std::vector<number> sortedUsedBlockIDs(usedBlockIDs.begin(), usedBlockIDs.end());
			std::sort(sortedUsedBlockIDs.begin(), sortedUsedBlockIDs.end());
			oram->loadMacMap("../backup_sql/mac_map_server_0.bin");
			for (number id : sortedUsedBlockIDs) {
				std::vector<std::vector<int64_t>> blockShares;
				blockShares = oram->getContainer(id);
				retrievedShares.insert(retrievedShares.end(), blockShares.begin(), blockShares.end());
			}
			return std::move(oram);
		}

        void callSyncCache(std::unique_ptr<ORAM>& oram) {
			oram->syncCache();
		}

    };


    TEST_F(ORAMTestSQL, PutContainerServerORAM1)
	{
		using namespace std::chrono;
		auto start = std::chrono::high_resolution_clock::now();
		
		// Load and store secret shares for one server
		std::vector<std::vector<int64_t>> secretShares = loadSecretShares(1);
		auto [storage, map, stash, oram] = initialize(secretShares.size(), 0);
		commonSecretShareSize = secretShares.size(); // Set the common secret share size for all the ORAM instance

		populateStorage(storage); // Called to set block ID's within each bucket across the entire ORAM

		// Computer the MAC for the current buckets in storage
		oram->computeAndStoreAllBucketMACs();

		// Print the size of secret shares before storing them
		std::cout << "Size of secret shares: " << secretShares.size() << std::endl;

		number currentIndex = 0;
		number blockID = 0;

		while (currentIndex < secretShares.size())
		{
			// Get the next chunk of secret shares to store in the ORAM
			std::vector<std::vector<int64_t>> secretSharesPerBlock(
				secretShares.begin() + currentIndex,
				secretShares.begin() + std::min(currentIndex + 1000, (number)secretShares.size()));
			std::cout << "Writing to block ID: " << blockID << ", tuples: " << secretSharesPerBlock.size() << std::endl;
			// Store the chunk in the ORAM
			ASSERT_NO_THROW(oram->putContainer(blockID, secretSharesPerBlock));

			// Update the current index and block ID
			currentIndex += 1000;
			blockID++;
		}

		auto end = std::chrono::high_resolution_clock::now();
	 	auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	 	std::cout << "Time to populate ORAM for server 1" << ": " << duration_ms << " ms" << std::endl;
		// Total number of blocks stored in the ORAM
		std::cout << "Total number of blocks stored in the ORAM: " << oram->getUsedBlockIDs().size() << std::endl;

		callSyncCache(oram);
		backupGenerator(storage, map, stash, oram, 0);
				// Storing the secret share size since it is also used in the backupGenerator
		std::ofstream sizeFile("../backup_sql/common_secret_share_size.txt");
		if (sizeFile.is_open()) {
			sizeFile << secretShares.size();
			sizeFile.close();
		}
	}

    TEST_F(ORAMTestSQL, GetContainerSingleORAM)
    {
		using namespace std::chrono;

		timingDetails details = {};
		details.testName = "GetContainerServerORAM";

        // Retrieved secret shares
		std::vector<std::vector<int64_t>> retrievedSecretShares;

        // Retrieve all the data in the ORAM and store it in a text file
        auto [storage, map, stash, oram] = initializeFromBackup(commonSecretShareSize, 0); // Initialize from backup for server 0
        std::string usedBlockIDsFile = "../backup_sql/used_block_ids_server_0.bin";
		oram->resetTimingMetrics(); // Reset the timing metrics before starting the test
        
        // Use getUsedBlockIDs to only iterate over blocks that actually contain data
        std::unordered_set<number> usedBlockIDs = loadUsedBlockIDs(usedBlockIDsFile);
        // sort the usedBlockIDs to ensure they are in order
        std::vector<number> sortedUsedBlockIDs(usedBlockIDs.begin(), usedBlockIDs.end());
        std::sort(sortedUsedBlockIDs.begin(), sortedUsedBlockIDs.end());
        //std::cout << "Total number of blocks stored in the ORAM: " << usedBlockIDs.size() << std::endl;
        //std::cout << "Total number of blocks stored in the ORAM: while getting: " << sortedUsedBlockIDs.size() << std::endl;
        // Print the used block IDs
        // for (const auto& id : sortedUsedBlockIDs)
        // {
        //     std::cout << "Used block ID: " << id << std::endl;
        // }
        // Load mac map from file
        oram->loadMacMap("../backup_sql/mac_map_server_0.bin");	

        for (number id : sortedUsedBlockIDs)
        {
            //std::cout << "Reading block ID: " << id << std::endl;
            std::vector<std::vector<int64_t>> blockShares;
            ASSERT_NO_THROW(blockShares = oram->getContainer(id));
            retrievedShares_global.insert(retrievedShares_global.end(), blockShares.begin(), blockShares.end());
        }

		details.gettingShares = oram->getPathRetrievalTime();
		
		std::cout << "Total number of blocks stored in the ORAM: while getting: " << usedBlockIDs.size() << std::endl;
        // Load the original secret shares for verification
        std::vector<std::vector<int64_t>> secretShares = loadSecretShares(1);
        ASSERT_EQ(retrievedShares_global.size(), secretShares.size());
        ASSERT_EQ(secretShares[0].size(), retrievedShares_global[0].size());
        for (size_t j = 0; j < secretShares.size(); ++j)
        {
            ASSERT_EQ(secretShares[j], retrievedShares_global[j]);
        }

		std::cout<< "Retrieved all secret shares successfully." << std::endl;
		details.integrityCheck = oram->getTotalIntegrityCheckTime();
		details.shufflePaths = oram->getTotalReshuffleTime();
        writeTimingMetric(details, true);
    }

	TEST_F(ORAMTestSQL, SQLCountORQuery) {
		std::ifstream ifs("../SQL_Queries/COUNT/Status_Flag.json");
		json j;
		ifs >> j;
		std::vector<std::string> attributeIDs;
		std::vector<size_t> attr_idx;

		using namespace std::chrono;
		timingDetails details = {};
		details.testName = "SQLCountORQuery";

		auto oram = loadORAMAndShares();
		details.gettingShares = oram->getPathRetrievalTime();

		// Start tracking the time for query translation
		auto start = std::chrono::high_resolution_clock::now();

		// Extract id_0 from both filters
		for (const auto& filter : j["filters"]) {
			if (filter.contains("shareID") && filter["shareID"].contains("id_0")) {
				filter_ids.push_back(filter["shareID"]["id_0"].get<int64_t>());
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
		for (const auto& tuple : retrievedShares_global) {
		bool found0 = (tuple.size() > attr_idx[0]) && (tuple[attr_idx[0]] == filter_ids[0]);
		bool found1 = (tuple.size() > attr_idx[1]) && (tuple[attr_idx[1]] == filter_ids[1]);
		if (found0 || found1) {
			count++;
			}
		}

		auto end = std::chrono::high_resolution_clock::now();
		details.queryTranslation = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		details.integrityCheck = oram->getTotalIntegrityCheckTime();
		details.shufflePaths = oram->getTotalReshuffleTime();
		writeTimingMetric(details);

		std::cout << "Count of tuples containing either filter_id: " << count << std::endl;
	}

	TEST_F(ORAMTestSQL, SQLCountANDQuery) {
		std::ifstream ifs("../SQL_Queries/COUNT/Return_Flag.json");
		json j;
		ifs >> j;
		std::vector<std::string> attributeIDs;
		std::vector<size_t> attr_idx;

		timingDetails details = {};
		details.testName = "SQLCountANDQuery";

		
		auto oram = loadORAMAndShares();
		details.gettingShares = oram->getPathRetrievalTime();

		// Start tracking the time for query translation
		auto start = std::chrono::high_resolution_clock::now();

		// Extract id_0 from both filters
		for (const auto& filter : j["filters"]) {
			if (filter.contains("shareID") && filter["shareID"].contains("id_0")) {
				filter_ids.push_back(filter["shareID"]["id_0"].get<int64_t>());
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
		for (const auto& tuple : retrievedShares_global) {
		bool found0 = (tuple.size() > attr_idx[0]) && (tuple[attr_idx[0]] == filter_ids[0]);
		bool found1 = (tuple.size() > attr_idx[1]) && (tuple[attr_idx[1]] == filter_ids[1]);
			if (found0 && found1) {
			count++;
			}
		}
		
		auto end = std::chrono::high_resolution_clock::now();
		details.queryTranslation = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		details.integrityCheck = oram->getTotalIntegrityCheckTime();
		details.shufflePaths = oram->getTotalReshuffleTime();
		writeTimingMetric(details);

		std::cout << "Count of tuples containing both filter_ids: " << count << std::endl;
	}

	TEST_F(ORAMTestSQL, SQLSUMORQuery) {
		std::ifstream ifs("../SQL_Queries/SUM/ExtendedPrice.json");
		json j;
		ifs >> j;
		std::vector<std::string> attributeIDs;
		std::vector<size_t> attr_idx;
		using namespace std::chrono;
		timingDetails details = {};

		details.testName = "SQLSUMORQuery";

		
		auto oram = loadORAMAndShares();
		details.gettingShares = oram->getPathRetrievalTime();

		// Start tracking the time for query translation
		auto start = std::chrono::high_resolution_clock::now();

		std::string resultDir = "../Query_Result/SUMOR";
		std::filesystem::create_directories(resultDir); // Ensure the folder exists
		std::ofstream outFile(resultDir + "/server_1.txt");

		// Extract id_0 from both filters
		for (const auto& filter : j["filters"]) {
			if (filter.contains("shareID") && filter["shareID"].contains("id_0")) {
				filter_ids.push_back(filter["shareID"]["id_0"].get<int64_t>());
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
		for (const auto& tuple : retrievedShares_global) {
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

		auto end = std::chrono::high_resolution_clock::now();
		details.queryTranslation = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		details.integrityCheck = oram->getTotalIntegrityCheckTime();
		details.shufflePaths = oram->getTotalReshuffleTime();
		writeTimingMetric(details);

		std::cout << "Count of tuples containing either filter_id in SUM Query: " << count << std::endl;
	}

	TEST_F(ORAMTestSQL, SQLSUMANDQuery) {
		std::ifstream ifs("../SQL_Queries/SUM/Quantity.json");
		json j;
		ifs >> j;
		std::vector<std::string> attributeIDs;
		std::vector<size_t> attr_idx;
		using namespace std::chrono;
		
		timingDetails details = {};
		details.testName = "SQLSUMANDQuery";

		
		auto oram = loadORAMAndShares();
		details.gettingShares = oram->getPathRetrievalTime();

		// Start tracking the time for query translation
		auto start = std::chrono::high_resolution_clock::now();

		std::string resultDir = "../Query_Result/SUMAND";
		std::filesystem::create_directories(resultDir); // Ensure the folder exists
		std::ofstream outFile(resultDir + "/server_1.txt");

		// Extract id_0 from both filters
		for (const auto& filter : j["filters"]) {
			if (filter.contains("shareID") && filter["shareID"].contains("id_0")) {
				filter_ids.push_back(filter["shareID"]["id_0"].get<int64_t>());
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
		for (const auto& tuple : retrievedShares_global) {
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

		auto end = std::chrono::high_resolution_clock::now();
		details.queryTranslation = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		details.integrityCheck = oram->getTotalIntegrityCheckTime();
		details.shufflePaths = oram->getTotalReshuffleTime();
		writeTimingMetric(details);

		std::cout << "Count of tuples containing both filter_id in SUM Query: " << count << std::endl;
	}

	TEST_F(ORAMTestSQL, SQLAVGORQuery) {
		std::ifstream ifs("../SQL_Queries/AVG/Quantity.json");
		json j;
		ifs >> j;
		std::vector<std::string> attributeIDs;
		std::vector<size_t> attr_idx;

		using namespace std::chrono;

		timingDetails details = {};
		details.testName = "SQLAVGORQuery";

		
		auto oram = loadORAMAndShares();
		details.gettingShares = oram->getPathRetrievalTime();

		// Start tracking the time for query translation
		auto start = std::chrono::high_resolution_clock::now();
		std::string resultDir = "../Query_Result/AVGOR";
		std::filesystem::create_directories(resultDir); // Ensure the folder exists
		std::ofstream outFile(resultDir + "/server_1.txt");

		// Extract id_0 from both filters
		for (const auto& filter : j["filters"]) {
			if (filter.contains("shareID") && filter["shareID"].contains("id_0")) {
				filter_ids.push_back(filter["shareID"]["id_0"].get<int64_t>());
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
		for (const auto& tuple : retrievedShares_global) {
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
		details.queryTranslation = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		details.integrityCheck = oram->getTotalIntegrityCheckTime();
		details.shufflePaths = oram->getTotalReshuffleTime();
		writeTimingMetric(details);
	}

	TEST_F(ORAMTestSQL, SQLAVGANDQuery) {
		std::ifstream ifs("../SQL_Queries/AVG/Discount.json");
		json j;
		ifs >> j;
		std::vector<std::string> attributeIDs;
		std::vector<size_t> attr_idx;

		using namespace std::chrono;
		timingDetails details = {};
		details.testName = "SQLAVGANDQuery";

		
		auto oram = loadORAMAndShares();
		details.gettingShares = oram->getPathRetrievalTime();

		// Start tracking the time for query translation
		auto start = std::chrono::high_resolution_clock::now();

		std::string resultDir = "../Query_Result/AVGAND";
		std::filesystem::create_directories(resultDir); // Ensure the folder exists
		std::ofstream outFile(resultDir + "/server_1.txt");

		// Extract id_0 from both filters
		for (const auto& filter : j["filters"]) {
			if (filter.contains("shareID") && filter["shareID"].contains("id_0")) {
				filter_ids.push_back(filter["shareID"]["id_0"].get<int64_t>());
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
		for (const auto& tuple : retrievedShares_global) {
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
		details.queryTranslation = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		details.integrityCheck = oram->getTotalIntegrityCheckTime();
		details.shufflePaths = oram->getTotalReshuffleTime();
		writeTimingMetric(details);
	}
	
	TEST_F(ORAMTestSQL, SQLMINORQuery) {
		std::ifstream ifs("../SQL_Queries/MIN/ExtendedPrice_Min.json");
		json j;
		ifs >> j;
		std::vector<std::string> attributeIDs;
		std::vector<size_t> attr_idx;

		using namespace std::chrono;
		timingDetails details = {};
		details.testName = "SQLMINORQuery";

		
		auto oram = loadORAMAndShares();
		details.gettingShares = oram->getPathRetrievalTime();

		// Start tracking the time for query translation
		auto start = std::chrono::high_resolution_clock::now();

		std::string resultDir = "../Query_Result/MINOR";
		std::filesystem::create_directories(resultDir); // Ensure the folder exists
		std::ofstream outFile(resultDir + "/server_1.txt");

		// Extract id_0 from both filters
		for (const auto& filter : j["filters"]) {
			if (filter.contains("shareID") && filter["shareID"].contains("id_0")) {
				filter_ids.push_back(filter["shareID"]["id_0"].get<int64_t>());
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
		for (const auto& tuple : retrievedShares_global) {
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
		details.queryTranslation = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		details.integrityCheck = oram->getTotalIntegrityCheckTime();
		details.shufflePaths = oram->getTotalReshuffleTime();
		writeTimingMetric(details);
	}

	TEST_F(ORAMTestSQL, SQLMINANDQuery) {
		std::ifstream ifs("../SQL_Queries/MIN/Tax_Min.json");
		json j;
		ifs >> j;
		std::vector<std::string> attributeIDs;
		std::vector<size_t> attr_idx;

		using namespace std::chrono;
		timingDetails details = {};
		details.testName = "SQLMINANDQuery";

		
		auto oram = loadORAMAndShares();
		details.gettingShares = oram->getPathRetrievalTime();

		// Start tracking the time for query translation
		auto start = std::chrono::high_resolution_clock::now();
		std::string resultDir = "../Query_Result/MINAND";
		std::filesystem::create_directories(resultDir); // Ensure the folder exists
		std::ofstream outFile(resultDir + "/server_1.txt");

		// Extract id_0 from both filters
		for (const auto& filter : j["filters"]) {
			if (filter.contains("shareID") && filter["shareID"].contains("id_0")) {
				filter_ids.push_back(filter["shareID"]["id_0"].get<int64_t>());
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
		for (const auto& tuple : retrievedShares_global) {
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
		details.queryTranslation = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		details.integrityCheck = oram->getTotalIntegrityCheckTime();
		details.shufflePaths = oram->getTotalReshuffleTime();
		writeTimingMetric(details);
	}

	TEST_F(ORAMTestSQL, SQLMAXORQuery) {
		std::ifstream ifs("../SQL_Queries/MAX/ExtendedPrice_Max.json");
		json j;
		ifs >> j;
		std::vector<std::string> attributeIDs;
		std::vector<size_t> attr_idx;

		using namespace std::chrono;
		timingDetails details = {};
		details.testName = "SQLMAXORQuery";

		
		auto oram = loadORAMAndShares();
		details.gettingShares = oram->getPathRetrievalTime();

		// Start tracking the time for query translation
		auto start = std::chrono::high_resolution_clock::now();

		std::string resultDir = "../Query_Result/MAXOR";
		std::filesystem::create_directories(resultDir); // Ensure the folder exists
		std::ofstream outFile(resultDir + "/server_1.txt");

		// Extract id_0 from both filters
		for (const auto& filter : j["filters"]) {
			if (filter.contains("shareID") && filter["shareID"].contains("id_0")) {
				filter_ids.push_back(filter["shareID"]["id_0"].get<int64_t>());
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
		for (const auto& tuple : retrievedShares_global) {
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
		details.queryTranslation = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		details.integrityCheck = oram->getTotalIntegrityCheckTime();
		details.shufflePaths = oram->getTotalReshuffleTime();
		writeTimingMetric(details);
	}

	TEST_F(ORAMTestSQL, SQLMAXANDQuery) {
		std::ifstream ifs("../SQL_Queries/MAX/Tax_Max.json");
		json j;
		ifs >> j;
		std::vector<std::string> attributeIDs;
		std::vector<size_t> attr_idx;

		using namespace std::chrono;
		timingDetails details = {};
		details.testName = "SQLMAXANDQuery";

		
		auto oram = loadORAMAndShares();
		details.gettingShares = oram->getPathRetrievalTime();

		// Start tracking the time for query translation
		auto start = std::chrono::high_resolution_clock::now();

		std::string resultDir = "../Query_Result/MAXAND";
		std::filesystem::create_directories(resultDir); // Ensure the folder exists
		std::ofstream outFile(resultDir + "/server_1.txt");

		// Extract id_0 from both filters
		for (const auto& filter : j["filters"]) {
			if (filter.contains("shareID") && filter["shareID"].contains("id_0")) {
				filter_ids.push_back(filter["shareID"]["id_0"].get<int64_t>());
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
		for (const auto& tuple : retrievedShares_global) {
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
		details.queryTranslation = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		details.integrityCheck = oram->getTotalIntegrityCheckTime();
		details.shufflePaths = oram->getTotalReshuffleTime();
		writeTimingMetric(details);
	}
}


int main(int argc, char **argv)
{
	srand(TEST_SEED);

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}