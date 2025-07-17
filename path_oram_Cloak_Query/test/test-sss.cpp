#include "definitions.h"
#include "oram.hpp"
#include "utility.hpp"
#include <filesystem>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <math.h>

namespace fs = std::filesystem;

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

// Overloaded function to load secret shares from the first file found in the ../shares directory
std::vector<std::vector<int64_t>> loadSecretShares() {
    std::string baseDir = "../shares";
    for (const auto& entry : std::filesystem::directory_iterator(baseDir)) {
        if (entry.is_regular_file()) {
            std::ifstream file(entry.path(), std::ios::in);
            std::vector<std::vector<int64_t>> allShares;
            if (file.is_open()) {
                std::string line;
                while (std::getline(file, line)) {
                    std::istringstream iss(line);
                    std::string y_str;
                    std::vector<int64_t> tupleShares;
                    while (std::getline(iss, y_str, '|')) {
                        y_str.erase(0, y_str.find_first_not_of(" \t\n\r"));
                        y_str.erase(y_str.find_last_not_of(" \t\n\r") + 1);
                        if (!y_str.empty()) {
                            tupleShares.push_back(std::stoll(y_str));
                        }
                    }
                    allShares.push_back(tupleShares);
                }
                file.close();
            }
            return allShares; // Return after first file
        }
    }
    throw std::runtime_error("No share file found in ../shares");
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

namespace CloakQueryPathORAM
{
    class ORAMTestSSS : public ::testing::Test
	{
		public:
		number LOG_CAPACITY;
		number CAPACITY;
		number totalBlocks;
		number totalBuckets;

		protected:
		inline static const number Z			= 3; // Number of blocks per bucket
		// Block size calculation:
		// 16 attributes * 8 bytes per attribute = 128 bytes
		// Block size = N * 128 bytes
		// Considering 1000 tuple entries per block size (N = 1000), we get:
		// Block size = 1000 * 128 = 128000 bytes = 128 KB
		// Considering 10 % overhead for serialization, we get:
		// Block size = 128000 * 1.1 = 140.8 KB
		// Nearest aligned size for 8 bytes = 140.8 KB
		inline static const number BLOCK_SIZE	= 140800;
		inline static const number BATCH_SIZE	= 10; // Number of requests to process at a time
		//inline static string FILENAME = "storage.bin";
		inline static bytes KEY; // AES key for encryption operations
		inline static size_t commonSecretShareSize = 0; // Size of the secret shares, to be set in initialize()
		// create and std pair for saving the server and blocksize
		//inline static size_t stashBlockSize = 0;

		//inline static const number CAPACITY = (1 << LOG_CAPACITY); // Total number of blocks in ORAM

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
			std::string backupDir = "../backup_sss";
			if (!std::filesystem::exists(backupDir)) {
				std::filesystem::create_directory(backupDir);
			}
			std::string uniqueFilename = backupDir + "/storage_server_" + std::to_string(serverIndex) + ".bin";

			auto storage = shared_ptr<AbsStorageAdapter>(new FileSystemStorageAdapter(CAPACITY + Z, BLOCK_SIZE, KEY, uniqueFilename, true, Z));
			auto map = shared_ptr<AbsPositionMapAdapter>(new InMemoryPositionMapAdapter(CAPACITY * Z + Z));
			auto stash = make_shared<InMemoryStashAdapter>(3 * LOG_CAPACITY * Z);

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
			std::string backupDir = "../backup_sss";
			std::string keyFile = backupDir + "/key_server_" + std::to_string(serverIndex) + ".bin";
			std::string posmapFile = backupDir + "/position-map_server_" + std::to_string(serverIndex) + ".bin";
			std::string stashFile = backupDir + "/stash_server_" + std::to_string(serverIndex) + ".bin";
			std::string storageFile = backupDir + "/storage_server_" + std::to_string(serverIndex) + ".bin";
			totalBlocks = (secretSharesSize / 1000) + 1; 
			totalBuckets = (totalBlocks / Z) + 1;        
			LOG_CAPACITY = static_cast<number>(ceil(log2(totalBuckets))); 
			CAPACITY = (1 << LOG_CAPACITY); // Total number of buckets in the ORAM


			// Reconstruct the ORAM instance from the backup files
			if (!fs::exists(storageFile) || !fs::exists(keyFile) || !fs::exists(posmapFile) || !fs::exists(stashFile)) {
				std::cerr << "Backup files do not exist. Please run the PutContainerMultipleORAMs test first." << std::endl;
				return {};
			}

			KEY = loadKey(keyFile);
			//std::cout << "Called in initializeFromBackup with key size: " << KEY.size() << std::endl;
			//std::cout << "CAPACITY: " << CAPACITY << ", Z: " << Z << ", map size: " << (CAPACITY * Z + Z) << std::endl;
			// Loading storage
			auto storage = std::make_shared<FileSystemStorageAdapter>(CAPACITY + Z, BLOCK_SIZE, KEY, storageFile, false, Z, 0);
			
			// Loading map
			auto map = std::make_shared<InMemoryPositionMapAdapter>(CAPACITY * Z + Z);
			dynamic_pointer_cast<InMemoryPositionMapAdapter>(map)->loadFromFile(posmapFile);	
			auto inMemMap = std::dynamic_pointer_cast<InMemoryPositionMapAdapter>(map);
			for (int i = 0; i < 10; ++i) {
				std::cout << "map[" << i << "] = " << inMemMap->get(i) << std::endl;
			}		

			// Loading stash
			// Commented out code below can be used for calculating the block size of the stash
			// vector<block> stashDump;
			// stashVar->getAll(stashDump);
			// auto blockSize = stashDump.size() > 0 ? stashDump[0].second.size() : 0;
			//auto stash = make_shared<InMemoryStashAdapter>(2 * LOG_CAPACITY * Z);
			//TODO: This might be wrong, need to check the block size
			//dynamic_pointer_cast<InMemoryStashAdapter>(stash)->loadFromFile(stashFile, BLOCK_SIZE);
			auto stash = make_shared<InMemoryStashAdapter>(2 * LOG_CAPACITY * Z);
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
			std::string backupDir = "../backup_sss";
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

		void callSyncCache(std::unique_ptr<ORAM>& oram) {
			oram->syncCache();
		}

	};

	// TEST_F(ORAMTestSSS, PutGetContainer)
	// {
	// 	// Load and store secret shares for one server
	// 	std::vector<std::vector<int64_t>> secretShares = loadSecretShares(1);
	// 	auto [storage, map, stash, oram] = initialize(secretShares.size());
	// 	initialize(secretShares.size());
	// 	populateStorage(storage); // Called to set block ID's within each bucket across the entire ORAM

	// 	// Computer the MAC for the current buckets in storage
	// 	oram->computeAndStoreAllBucketMACs();

	// 	// Print the size of secret shares before storing them
	// 	std::cout << "Size of secret shares: " << secretShares.size() << std::endl;

	// 	number currentIndex = 0;
	// 	number blockID = 0;

	// 	while (currentIndex < secretShares.size())
	// 	{
	// 		// Get the next chunk of secret shares to store in the ORAM
	// 		std::vector<std::vector<int64_t>> secretSharesPerBlock(
	// 			secretShares.begin() + currentIndex,
	// 			secretShares.begin() + std::min(currentIndex + 1000, (number)secretShares.size()));

	// 		// Store the chunk in the ORAM
	// 		ASSERT_NO_THROW(oram->putContainer(blockID, secretSharesPerBlock));

	// 		// Update the current index and block ID
	// 		currentIndex += 1000;
	// 		blockID++;
	// 	}

	// 	// Get the container back from ORAM
	// 	std::vector<std::vector<int64_t>> retrievedShares;
	// 		for (number i = 0; i < blockID; i++)
	// 		{
	// 			std::cout << "retrievedShares for block ID: " << i << std::endl;
	// 				std::vector<std::vector<int64_t>> blockShares;
	// 				ASSERT_NO_THROW(blockShares = oram->getContainer(i));
					
	// 				// Appending the retrieved shares to the retrievedShares vector
	// 				retrievedShares.insert(retrievedShares.end(), blockShares.begin(), blockShares.end());
	// 		}

	// 	// Verify that the retrieved shares match the original shares
	// 	ASSERT_EQ(secretShares.size(), retrievedShares.size());
	// 	for (size_t i = 0; i < secretShares.size(); ++i)
	// 	{
	// 		ASSERT_EQ(secretShares[i].size(), retrievedShares[i].size());
	// 		for (size_t j = 0; j < secretShares[i].size(); ++j)
	// 		{
	// 			ASSERT_EQ(secretShares[i][j], retrievedShares[i][j]);
	// 		}
	// 	}
	// 	backupGenerator(storage, map, stash, oram, 0);
	// }

	// TEST_F(ORAMTestSSS, PutGetContainerMultipleORAMs)
	// {
	// 	// Load secret shares
	// 	std::vector<std::vector<std::vector<int64_t>>> secretShares;
	// 	for (int i = 1; i <= 6; i++) 
	// 	{ 
	// 		secretShares.push_back(loadSecretShares(i));
	// 	}
	// 	// Print out integer value in secretSHares[0]
	// 	// for (size_t i = 0; i < secretShares[0][0].size(); ++i)
	// 	// {
	// 	// 	//for (size_t j = 0; j < secretShares[0][i].size(); ++j)
	// 	// 	//{
	// 	// 		std::cout << secretShares[0][0][i] << " ";
	// 	// 	//}
	// 	// 	std::cout << std::endl;
	// 	// }
	// 	//std::cout << "Size of secret shares: " << secretShares.size() << std::endl;
	// 	std::vector<std::unique_ptr<ORAM>> oramInstances;
	// 	std::vector<std::shared_ptr<AbsStorageAdapter>> storages;
	// 	std::vector<std::shared_ptr<AbsPositionMapAdapter>> maps;
	// 	std::vector<std::shared_ptr<AbsStashAdapter>> stashes;

	// 	for (size_t serverIndex = 0; serverIndex < secretShares.size(); serverIndex++)
	// 	{
	// 		auto [storage, map, stash, oram] = initialize(secretShares[serverIndex].size(), serverIndex);
	// 		populateStorage(storage);
	// 		oram->computeAndStoreAllBucketMACs();

	// 		oramInstances.push_back(std::move(oram));
	// 		storages.push_back(storage);
	// 		maps.push_back(map);
	// 		stashes.push_back(stash);

	// 		//backupGenerator(storage, map, stash, oramInstances.back(), serverIndex); // Save state for this server
	// 	}

	// 	number currentIndex = 0;
	// 	number blockID = 0;

	// 	// Retrieve and verify the secret shares from each ORAM
	// 	for (size_t serverIndex = 0; serverIndex < secretShares.size(); ++serverIndex)
	// 	{
	// 		std::cout << "Entered the for loop for putting and retrieveing data: " << serverIndex << std::endl;
	// 		// Block ID to store the container
	// 		for (size_t i = 0; i < totalBlocks; i++)
	// 		{
	// 			while (currentIndex < secretShares[serverIndex].size())
	// 			{
	// 				// Get the next chunk of secret shares to store in the ORAM
	// 				std::vector<std::vector<int64_t>> secretSharesPerBlock(
	// 					secretShares[serverIndex].begin() + currentIndex,
	// 					secretShares[serverIndex].begin() + std::min(currentIndex + 1000, (number)secretShares[serverIndex].size()));

	// 				// Store the chunk in the ORAM
	// 				ASSERT_NO_THROW(oramInstances[serverIndex]->putContainer(blockID, secretSharesPerBlock));

	// 				// Update the current index and block ID
	// 				currentIndex += 1000;
	// 				blockID++;
	// 			}
	// 			//ASSERT_NO_THROW(oramInstances[serverIndex]->putContainer(blockID, secretShares[serverIndex]));
	// 			// Reset thec current index for the next server
	// 			currentIndex = 0;

	// 			// Get the container back from the respective ORAM
	// 			std::vector<std::vector<int64_t>> retrievedShares;
	// 			for (number i = 0; i < blockID; i++)
	// 			{
	// 				std::cout << "retrievedShares for block ID: " << i << std::endl;
	// 					std::vector<std::vector<int64_t>> blockShares;
	// 					ASSERT_NO_THROW(blockShares = oramInstances[serverIndex]->getContainer(i));
						
	// 					// Appending the retrieved shares to the retrievedShares vector
	// 					retrievedShares.insert(retrievedShares.end(), blockShares.begin(), blockShares.end());
	// 			}
	// 		//	ASSERT_NO_THROW(retrievedShares = oramInstances[serverIndex]->getContainer(blockID));

	// 			// Verify that the retrieved shares match the original shares
	// 			//std::cout << "Size of secret shares: " << secretShares[0][0].size() << std::endl;
	// 			ASSERT_EQ(retrievedShares.size(), secretShares[serverIndex].size());
	// 			ASSERT_EQ(secretShares[0][0].size(), retrievedShares[0].size());
	// 			for (size_t j = 0; j < secretShares[serverIndex].size(); ++j)
	// 			{
	// 				ASSERT_EQ(secretShares[serverIndex][j], retrievedShares[j]);
	// 				// Print out the data in retrievedShares
	// 				// for (const auto& vec: retrievedShares[j])
	// 				// {
	// 				// 	std::cout << vec << " ";
	// 				// }
	// 				// std::cout << std::endl;
	// 			}

	// 			// Reset the block ID for the next server
	// 			blockID = 0;
	// 		}
	// 		// Save the state for this server
	// 		backupGenerator(storages[serverIndex], maps[serverIndex], stashes[serverIndex], oramInstances[serverIndex], serverIndex);
	// 	}
	// }

	// TEST_F(ORAMTestSSS, PutContainerMultipleORAMs)
	// {
	// 	using namespace std::chrono;
	// 	auto start = std::chrono::high_resolution_clock::now();
	// 	// Load secret shares for each server
	// 	std::vector<std::vector<std::vector<int64_t>>> secretShares;
	// 	for (int i = 1; i <= 6; i++) 
	// 	{ 
	// 		secretShares.push_back(loadSecretShares(i));
	// 	}

	// 	std::vector<std::unique_ptr<ORAM>> oramInstances;
	// 	std::vector<std::shared_ptr<AbsStorageAdapter>> storages;
	// 	std::vector<std::shared_ptr<AbsPositionMapAdapter>> maps;
	// 	std::vector<std::shared_ptr<AbsStashAdapter>> stashes;
	// 	commonSecretShareSize = secretShares[1].size();

	// 	for (size_t serverIndex = 0; serverIndex < secretShares.size(); ++serverIndex)
	// 	{
	// 		auto [storage, map, stash, oram] = initialize(secretShares[serverIndex].size(), serverIndex);
	// 		populateStorage(storage);
	// 		oram->computeAndStoreAllBucketMACs();

	// 		oramInstances.push_back(std::move(oram));
	// 		storages.push_back(storage);
	// 		maps.push_back(map);
	// 		stashes.push_back(stash);
	// 	}

	// 	// Now, time the population of each ORAM
	// 	for (size_t serverIndex = 0; serverIndex < secretShares.size(); ++serverIndex)
	// 	{
	// 		std::cout << "Populating ORAM for server " << serverIndex << std::endl;
	// 		number currentIndex = 0;
	// 		number blockID = 0;

			

	// 		while (currentIndex < secretShares[serverIndex].size())
	// 		{
	// 			std::vector<std::vector<int64_t>> secretSharesPerBlock(
	// 				secretShares[serverIndex].begin() + currentIndex,
	// 				secretShares[serverIndex].begin() + std::min(currentIndex + 1000, (number)secretShares[serverIndex].size()));

	// 			ASSERT_NO_THROW(oramInstances[serverIndex]->putContainer(blockID, secretSharesPerBlock));

	// 			currentIndex += 1000;
	// 			blockID++;
	// 		}
	// 		// Save the state for this server after populating ORAM
	// 		backupGenerator(storages[serverIndex], maps[serverIndex], stashes[serverIndex], oramInstances[serverIndex], serverIndex);
	// 	}

	// 	std::ofstream sizeFile("../backup/common_secret_share_size.txt");
	// 	if (sizeFile.is_open()) {
	// 		sizeFile << secretShares[0].size(); // Considering the secret shares size for one server, since all of them 
	// 											// have the same size.
	// 		sizeFile.close();
	// 	}
	// 	auto end = std::chrono::high_resolution_clock::now();
	// 	auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	// 	std::cout << "Time to populate ORAM for server " << ": " << duration_ms << " ms" << std::endl;
	// }

	// TEST_F(ORAMTestSSS, GetContainerMultipleORAMs)
	// {
	// 	// Vector to store all the retrieved shares
	// 	std::vector<std::vector<std::vector<int64_t>>> retrievedShares;

	// 	struct ORAMSet {
	// 		std::unique_ptr<ORAM> oramInstance;
	// 		std::shared_ptr<AbsStorageAdapter> storage;
	// 		std::shared_ptr<AbsPositionMapAdapter> map;
	// 		std::shared_ptr<AbsStashAdapter> stash;
	// 		std::unordered_set<number> usedBlockIDs;
	// 		std::vector<number> sortedUsedBlockIDs;
	// 	};

	// 	// Vector to store used block IDs for each ORAM created for each server
	// 	std::vector<ORAMSet> oramSets;

	// 	// Reinitialize all the parts which make up the oram and other related components
	// 	for (size_t serverIndex = 0; serverIndex < 6; ++serverIndex) 
	// 	{
	// 		auto [storage, map, stash, oram] = initializeFromBackup(commonSecretShareSize, serverIndex);
	// 		size_t countOfBLock = oram->getUsedBlockIDs().size();
	// 		std::cout << "Total number of blocks used in oram after initialization : " << countOfBLock << std::endl;
	// 		ORAMSet oramSet;
	// 		oramSet.storage = storage;
	// 		oramSet.map = map;
	// 		oramSet.stash = stash;
	// 		oramSet.oramInstance = std::move(oram);
	// 		oramSet.oramInstance->loadMacMap("../backup/mac_map_server_" + std::to_string(serverIndex) + ".bin");
	// 		oramSet.usedBlockIDs = (loadUsedBlockIDs("../backup/used_block_ids_server_" + std::to_string(serverIndex) + ".bin"));
	// 		oramSet.sortedUsedBlockIDs = std::vector<number>(oramSet.usedBlockIDs.begin(), oramSet.usedBlockIDs.end());
	// 		std::sort(oramSet.sortedUsedBlockIDs.begin(), oramSet.sortedUsedBlockIDs.end());
	// 		std::cout << "Total number of blocks stored in the current ORAM: " << oramSet.usedBlockIDs.size() << std::endl;
	// 		std::cout << "Total number of blocks stored in the ORAM: while getting: " << oramSet.sortedUsedBlockIDs.size() << std::endl;
	// 		// Add the oramSet to the ORAM vector
	// 		oramSets.push_back(std::move(oramSet));
	// 	}

	// 	// Retrieved secret shares
	// 	std::vector<std::vector<std::vector<int64_t>> >retrievedSecretShares;

	// 	// Print the used block IDs
	// 	// for (const auto& id : sortedUsedBlockIDs)
	// 	// {
	// 	// 	std::cout << "Used block ID: " << id << std::endl;
	// 	// }

	// 	// Retrieve the secret shares for every corresponding server
	// 	for (size_t serverIndex = 0; serverIndex < oramSets.size(); ++serverIndex){
	// 		retrievedShares.resize(oramSets.size());
	// 		for (number id : oramSets[serverIndex].sortedUsedBlockIDs)
	// 		{
	// 			std::cout << "Reading block ID: " << id << std::endl;
	// 			std::vector<std::vector<int64_t>> blockShares;
	// 			uint64_t countOfUsedBlocks = oramSets[serverIndex].oramInstance->getUsedBlockIDs().size();
	// 			std::cout << "Count of used blocks in current ORAM: " << countOfUsedBlocks << std::endl;
	// 			ASSERT_NO_THROW(blockShares = oramSets[serverIndex].oramInstance->getContainer(id));
	// 			retrievedShares[serverIndex].insert(retrievedShares[serverIndex].end(), blockShares.begin(), blockShares.end());
	// 		}
	// 		std::cout << "Total number of blocks stored in the ORAM: while getting: " << oramSets[serverIndex].usedBlockIDs.size() << std::endl;

	// 		// Load the original secret shares for verification
	// 		std::vector<std::vector<int64_t>> secretShares = loadSecretShares(serverIndex + 1);
	// 		ASSERT_EQ(retrievedShares[serverIndex].size(), secretShares.size());
	// 	}
		
	// 	// ASSERT_EQ(secretShares[0].size(), retrievedShares[0].size());
	// 	// for (size_t j = 0; j < secretShares.size(); ++j)
	// 	// {
	// 	// 	ASSERT_EQ(secretShares[j], retrievedShares[j]);
	// 	// }
	// 	// std::cout<< "Retrieved all secret shares successfully." << std::endl;
	// }

	// ----------------------------------Server 1 Tests----------------------------------
	// TEST_F(ORAMTestSSS, PutContainerServerORAM1)
	// {
	// 	using namespace std::chrono;
	// 	auto start = std::chrono::high_resolution_clock::now();
		
	// 	// Load and store secret shares for one server
	// 	std::vector<std::vector<int64_t>> secretShares = loadSecretShares(1);
	// 	auto [storage, map, stash, oram] = initialize(secretShares.size(), 0);
	// 	commonSecretShareSize = secretShares.size(); // Set the common secret share size for all the ORAM instance

	// 	populateStorage(storage); // Called to set block ID's within each bucket across the entire ORAM

	// 	// Computer the MAC for the current buckets in storage
	// 	oram->computeAndStoreAllBucketMACs();

	// 	// Print the size of secret shares before storing them
	// 	std::cout << "Size of secret shares: " << secretShares.size() << std::endl;

	// 	number currentIndex = 0;
	// 	number blockID = 0;

	// 	while (currentIndex < secretShares.size())
	// 	{
	// 		// Get the next chunk of secret shares to store in the ORAM
	// 		std::cout << "Writing to block ID: " << blockID << std::endl;
	// 		std::vector<std::vector<int64_t>> secretSharesPerBlock(
	// 			secretShares.begin() + currentIndex,
	// 			secretShares.begin() + std::min(currentIndex + 1000, (number)secretShares.size()));

	// 		// Store the chunk in the ORAM
	// 		ASSERT_NO_THROW(oram->putContainer(blockID, secretSharesPerBlock));

	// 		// Update the current index and block ID
	// 		currentIndex += 1000;
	// 		blockID++;
	// 	}

	// 	auto end = std::chrono::high_resolution_clock::now();
	//  	auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	//  	std::cout << "Time to populate ORAM for server 1" << ": " << duration_ms << " ms" << std::endl;
	// 	// Total number of blocks stored in the ORAM
	// 	std::cout << "Total number of blocks stored in the ORAM: " << oram->getUsedBlockIDs().size() << std::endl;

	// 	callSyncCache(oram);
	// 	backupGenerator(storage, map, stash, oram, 0);
	// 			// Storing the secret share size since it is also used in the backupGenerator
	// 	std::ofstream sizeFile("../backup/common_secret_share_size.txt");
	// 	if (sizeFile.is_open()) {
	// 		sizeFile << secretShares.size();
	// 		sizeFile.close();
	// 	}
	// }

	// ----------------------------------Server 2 Tests----------------------------------

	TEST_F(ORAMTestSSS, PutContainerServerORAM2)
	{
		using namespace std::chrono;
		auto start = std::chrono::high_resolution_clock::now();
		
		// Load and store secret shares for one server
		std::vector<std::vector<int64_t>> secretShares = loadSecretShares(2);
		auto [storage, map, stash, oram] = initialize(secretShares.size(), 1);
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
			std::cout << "Writing to block ID: " << blockID << " with " << secretSharesPerBlock.size() << " shares" << std::endl;

			// Store the chunk in the ORAM
			ASSERT_NO_THROW(oram->putContainer(blockID, secretSharesPerBlock));

			// Update the current index and block ID
			currentIndex += 1000;
			blockID++;
		}
		std::cout << "Total blocks written: " << blockID << std::endl;
		auto end = std::chrono::high_resolution_clock::now();
	 	auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	 	std::cout << "Time to populate ORAM for server 2" << ": " << duration_ms << " ms" << std::endl;
		// Total number of blocks stored in the ORAM
		std::cout << "Total number of blocks stored in the ORAM: " << oram->getUsedBlockIDs().size() << std::endl;

		callSyncCache(oram);
		backupGenerator(storage, map, stash, oram, 1);
				// Storing the secret share size since it is also used in the backupGenerator
		std::ofstream sizeFile("../backup_sss/common_secret_share_size.txt");
		if (sizeFile.is_open()) {
			sizeFile << secretShares.size();
			sizeFile.close();
		}
	}
	
	// TEST_F(ORAMTestSSS, GetContainerServerORAM1)
	// {
	// 	// Retrieved secret shares
	// 	std::vector<std::vector<int64_t>> retrievedSecretShares;

	// 	// Retrieve all the data in the ORAM and store it in a text file
	// 	auto [storage, map, stash, oram] = initializeFromBackup(commonSecretShareSize, 0); // Initialize from backup for server
	// 	std::string usedBlockIDsFile = "../backup/used_block_ids_server_0.bin";

	// 	// Use getUsedBlockIDs to only iterate over blocks that actually contain data
	// 	std::unordered_set<number> usedBlockIDs = loadUsedBlockIDs(usedBlockIDsFile);
	// 	// sort the usedBlockIDs to ensure they are in order
	// 	std::vector<number> sortedUsedBlockIDs(usedBlockIDs.begin(), usedBlockIDs.end());
	// 	std::sort(sortedUsedBlockIDs.begin(), sortedUsedBlockIDs.end());
	// 	std::cout << "Total number of blocks stored in the ORAM: " << usedBlockIDs.size() << std::endl;
	// 	// Load mac map from file
	// 	oram->loadMacMap("../backup/mac_map_server_0.bin");
		
	// 	std::vector<std::vector<int64_t>> retrievedShares;
	// 	for (number id : sortedUsedBlockIDs)
	// 	{
	// 		std::cout << "Reading block ID: " << id << std::endl;
	// 		std::vector<std::vector<int64_t>> blockShares;
	// 		ASSERT_NO_THROW(blockShares = oram->getContainer(id));
	// 		retrievedShares.insert(retrievedShares.end(), blockShares.begin(), blockShares.end());
	// 	}
	// 	std::cout << "Total number of blocks stored in the ORAM: while getting: " << usedBlockIDs.size() << std::endl;
	// 	// Load the original secret shares for verification
	// 	std::vector<std::vector<int64_t>> secretShares = loadSecretShares(1);
	// 	ASSERT_EQ(retrievedShares.size(), secretShares.size());
	// 	ASSERT_EQ(secretShares[0].size(), retrievedShares[0].size());
	// 	for (size_t j = 0; j < secretShares.size(); ++j)
	// 	{
	// 		ASSERT_EQ(secretShares[j], retrievedShares[j]);
	// 	}
	// 	std::cout<< "Retrieved all secret shares successfully." << std::endl;
	// }
	
	TEST_F(ORAMTestSSS, GetContainerServerORAM2)
	{
		// Retrieved secret shares
		std::vector<std::vector<int64_t>> retrievedSecretShares;

		// Retrieve all the data in the ORAM and store it in a text file
		auto [storage, map, stash, oram] = initializeFromBackup(commonSecretShareSize, 1); // Initialize from backup for server
		std::string usedBlockIDsFile = "../backup_sss/used_block_ids_server_1.bin";

		// Use getUsedBlockIDs to only iterate over blocks that actually contain data
		std::unordered_set<number> usedBlockIDs = loadUsedBlockIDs(usedBlockIDsFile);
		// sort the usedBlockIDs to ensure they are in order
		std::vector<number> sortedUsedBlockIDs(usedBlockIDs.begin(), usedBlockIDs.end());
		std::sort(sortedUsedBlockIDs.begin(), sortedUsedBlockIDs.end());
		std::cout << "Total number of blocks stored in the ORAM: " << usedBlockIDs.size() << std::endl;
		// Load mac map from file
		oram->loadMacMap("../backup_sss/mac_map_server_1.bin");

		std::vector<std::vector<int64_t>> retrievedShares;
		for (number id : sortedUsedBlockIDs)
		{
			std::cout << "Reading block ID: " << id << std::endl;
			std::vector<std::vector<int64_t>> blockShares;
			ASSERT_NO_THROW(blockShares = oram->getContainer(id));
			std::cout << "Block " << id << " has " << blockShares.size() << " tuples" << std::endl;
			retrievedShares.insert(retrievedShares.end(), blockShares.begin(), blockShares.end());
		}
		std::cout << "Total number of blocks stored in the ORAM: while getting: " << usedBlockIDs.size() << std::endl;
		// Load the original secret shares for verification
		std::vector<std::vector<int64_t>> secretShares = loadSecretShares(2);
		ASSERT_EQ(retrievedShares.size(), secretShares.size());
		ASSERT_EQ(secretShares[0].size(), retrievedShares[0].size());
		for (size_t j = 0; j < secretShares.size(); ++j)
		{
			ASSERT_EQ(secretShares[j], retrievedShares[j]);
		}
		std::cout<< "Retrieved all secret shares successfully." << std::endl;
	}
}


int main(int argc, char **argv)
{
	srand(TEST_SEED);

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}