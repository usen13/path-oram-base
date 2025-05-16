#include "definitions.h"
#include "oram.hpp"
#include "utility.hpp"
#include <filesystem>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace fs = std::filesystem;

std::vector<std::vector<int64_t>> loadSecretShares(int serverNumber) {
	std::vector<std::vector<int64_t>> allShares;
	
	std::string baseDir = "../shares"; // Relative path to the shares directory

	std::filesystem::path path = std::filesystem::current_path();
		//fs::path path = fs::current_path();
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

namespace CloakQueryPathORAM
{
    class MockStorage : public AbsStorageAdapter
	{
		public:
		MockStorage(number capacity, number userBlockSize, bytes key, number Z, number batchLimit) :
			AbsStorageAdapter(capacity, userBlockSize, key, Z, batchLimit)
		{
			_real = make_unique<InMemoryStorageAdapter>(capacity, userBlockSize, key, Z, batchLimit);

			// by default, all calls are delegated to the real object
			ON_CALL(*this, getInternal).WillByDefault([this](const vector<number> &locations, vector<bytes> &response) {
				return ((AbsStorageAdapter *)_real.get())->getInternal(locations, response);
			});
			ON_CALL(*this, setInternal).WillByDefault([this](const vector<block> &requests) {
				return ((AbsStorageAdapter *)_real.get())->setInternal(requests);
			});
		}

		// these four do not need to be mocked, they just have to exist to make class concrete
		virtual void getInternal(const number location, bytes &response) const override
		{
			return _real->getInternal(location, response);
		}

		virtual void setInternal(const number location, const bytes &raw) override
		{
			_real->setInternal(location, raw);
		}

		virtual bool supportsBatchGet() const override
		{
			return false;
		}

		virtual bool supportsBatchSet() const override
		{
			return false;
		}

		// these two need to be mocked since we want to track how and when they are called (hence ON_CALL above)
		MOCK_METHOD(void, getInternal, (const vector<number> &locations, vector<bytes> &response), (const, override));
		MOCK_METHOD(void, setInternal, ((const vector<block>)&requests), (override));

		private:
		unique_ptr<InMemoryStorageAdapter> _real;
	};

    class ORAMTest : public ::testing::Test
	{
		public:
		number LOG_CAPACITY;
		number CAPACITY;
		number totalBlocks;
		number totalBuckets;

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

		//inline static const number CAPACITY = (1 << LOG_CAPACITY); // Total number of blocks in ORAM

		protected:
		unique_ptr<ORAM> oram;
		shared_ptr<AbsStorageAdapter> storage;
		shared_ptr<AbsStashAdapter> stash;

		void initialize(size_t secretSharesSize)
		{
			// Calculate the total number of blocks and buckets
			totalBlocks = (secretSharesSize / 1000) + 1; 
			totalBuckets = (totalBlocks / Z) + 1;
			

			//CAPACITY = totalBuckets * Z; 
			// This is used to calculate the height of the ORAM tree. 
			// For example, if CAPACITY = 32, then LOG_CAPACITY = 5
			// Logarithm base 2 of the capacity
			//LOG_CAPACITY = static_cast<number>(ceil(log2(CAPACITY))); 
			LOG_CAPACITY = 1; 
			CAPACITY = (1 << LOG_CAPACITY);; // Total number of buckets in the ORAM												

			storage = make_shared<InMemoryStorageAdapter>(CAPACITY + Z, BLOCK_SIZE, bytes(), Z);
			stash = make_shared<InMemoryStashAdapter>(3 * LOG_CAPACITY * Z);

			// Initialize ORAM with the calculated capacity and other parameters
			this->oram = make_unique<ORAM>(
				LOG_CAPACITY,
				BLOCK_SIZE,
				Z,
				storage,
				make_unique<InMemoryPositionMapAdapter>(CAPACITY * Z + Z),
				stash,
				true,
				BATCH_SIZE);
		}
		void populateStorage()
		{
			std::cout << "Populating storage with " << CAPACITY << " buckets..." << std::endl;
			for (number i = 0; i < CAPACITY; i++)
			{
				bucket bucket;
				for (auto j = 0uLL; j < Z; j++)
				{
					bucket.push_back({i * Z + j, bytes()});
				}
				storage->set(i, bucket);
			}
		}
	};

	TEST_F(ORAMTest, PutGetContainer)
	{
		// Load and store secret shares for one server
		std::vector<std::vector<int64_t>> secretShares = loadSecretShares(1);

		initialize(secretShares.size());
		populateStorage();
		// Computer the MAC for the current buckets in storage
		oram->computeAndStoreAllBucketMACs();
		// Block ID to store the container
		//number allBlocks = (((number)1 << LOG_CAPACITY) * Z);

		// Print the size of secret shares before storing them
		std::cout << "Size of secret shares: " << secretShares.size() << std::endl;

		//int blockID = 1; // Block ID to store the container
		// for (number i = 0; i < allBlocks; i++)
		// {
		// 	std::vector<std::vector<int64_t>> secretSharesPerBlock(secretShares.begin() + (i * 1000), 
		// 		secretShares.begin() + std::min((i + 1) * 1000, (number)secretShares.size())); // Get the shares for the current block
		// 	number blockID = i % CAPACITY; // Block ID to store the container
		// 	std::cout << "Block ID: " << blockID << std::endl;

		// 	// Serialize the secret shares into bytes
		 	//bytes serializedData = serialize(secretShares);

		// 	// Print the size of serialized data
		// 	std::cout << "Size of serialized data: " << serializedData.size() << std::endl;

		// 	// Put the container into ORAM
		// 	ASSERT_NO_THROW(oram->put(blockID, serializedData));
		// }
		// Put the container into ORAM
		number currentIndex = 0;
		number blockID = 0;

		while (currentIndex < secretShares.size())
		{
			// Get the next chunk of secret shares to store in the ORAM
			std::vector<std::vector<int64_t>> secretSharesPerBlock(
				secretShares.begin() + currentIndex,
				secretShares.begin() + std::min(currentIndex + 1000, (number)secretShares.size()));

			// Store the chunk in the ORAM
			ASSERT_NO_THROW(oram->putContainer(blockID, secretSharesPerBlock));

			// Update the current index and block ID
			currentIndex += 1000;
			blockID++;
		}

		// Get the container back from ORAM
		std::vector<std::vector<int64_t>> retrievedShares;
		
		//for (number i; i < totalBlocks; i++)
		//{
			//number blockID = i + 1; // Block ID to store the container
			for (number i = 0; i < CAPACITY; i++)
			{
				std::cout << "retrievedShares for block ID: " << i << std::endl;
				for (auto j = 0uLL; j < Z; j++)
				{
					//bucket.push_back({i * Z + j, bytes()});
					ASSERT_NO_THROW(retrievedShares = oram->getContainer(i * Z + j));
				}
				//storage->set(i, bucket);
			}
			//ASSERT_NO_THROW(retrievedShares = oram->getContainer(1));
		//}

		// Verify that the retrieved shares match the original shares
		ASSERT_EQ(secretShares.size(), retrievedShares.size());
		for (size_t i = 0; i < secretShares.size(); ++i)
		{
			ASSERT_EQ(secretShares[i].size(), retrievedShares[i].size());
			for (size_t j = 0; j < secretShares[i].size(); ++j)
			{
				ASSERT_EQ(secretShares[i][j], retrievedShares[i][j]);
			}
		}
	}

	// TEST_F(ORAMTest, PutGetContainerMultipleORAMs)
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

	// 	for (size_t serverIndex = 0; serverIndex < secretShares.size(); serverIndex++)
	// 	{
	// 		// Initialize ORAM for each server
	// 		initialize(secretShares[serverIndex].size());

	// 		// Print out the size of secret shares for each server
	// 		//std::cout << "Size of secret shares for server " << serverIndex + 1 << ": " << secretShares[serverIndex].size() << std::endl;
	// 		oramInstances.push_back(std::move(oram));
	// 	}

	// 	// Retrieve and verify the secret shares from each ORAM
	// 	for (size_t serverIndex = 0; serverIndex < secretShares.size(); ++serverIndex)
	// 	{
	// 		// Block ID to store the container
	// 		for (size_t i = 0; i < totalBlocks; i++)
	// 		{
	// 			number blockID = i + 1; // Block ID to store the container
	// 			ASSERT_NO_THROW(oramInstances[serverIndex]->putContainer(blockID, secretShares[serverIndex]));
			
	// 		//number blockID = 1;

	// 			// Get the container back from the respective ORAM
	// 			std::vector<std::vector<int64_t>> retrievedShares;
	// 			ASSERT_NO_THROW(retrievedShares = oramInstances[serverIndex]->getContainer(blockID));

	// 			// Verify that the retrieved shares match the original shares
	// 			//std::cout << "Size of secret shares: " << secretShares[0][0].size() << std::endl;
	// 			ASSERT_EQ(retrievedShares.size(), 6);
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
	// 		}
	// 	}
	// }
}


int main(int argc, char **argv)
{
	srand(TEST_SEED);

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}