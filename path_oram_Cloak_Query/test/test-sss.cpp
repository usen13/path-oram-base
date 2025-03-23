#include "definitions.h"
#include "oram.hpp"
#include "utility.hpp"
#include <filesystem>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace fs = std::filesystem;

namespace PathORAM
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
		inline static const number LOG_CAPACITY = 5;
		inline static const number Z			= 3;
		inline static const number BLOCK_SIZE	= 32;
		inline static const number BATCH_SIZE	= 10;

		inline static const number CAPACITY = (1 << LOG_CAPACITY);

		protected:
		unique_ptr<ORAM> oram;
		shared_ptr<AbsStorageAdapter> storage = make_shared<InMemoryStorageAdapter>(CAPACITY + Z, BLOCK_SIZE, bytes(), Z);
		shared_ptr<AbsStashAdapter> stash	  = make_shared<InMemoryStashAdapter>(3 * LOG_CAPACITY * Z);

		ORAMTest()
		{
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
		
		// Load shares should parse each server file and load the share from that file in to a vector,
	// then store that vector in ORAM block after which it should parse the next server file and load the share from that file in to a vector,
	// then store that vector in ORAM block and so on.

	void loadSecretShares(int n) {
		std::vector<std::vector<int64_t>> allShares;
		
		std::string baseDir = "../../shares"; // Relative path to the shares directory

		std::filesystem::path path = std::filesystem::current_path();
		//fs::path path = fs::current_path();
		std::cout << "Current path is: " << path << std::endl;

		for (int serverIndex = 1; serverIndex <= n; ++serverIndex) {
			std::string filePath = baseDir + "/server_" + std::to_string(serverIndex) + ".txt";
			// Pass file path to ifstream
			std::ifstream file(filePath, std::ios::in);
			if (file.is_open()) {
				std::string line;
				while (std::getline(file, line)) {
					std::istringstream iss(line);
					std::string y_str;
					std::vector<int64_t> tupleShares;
					// while (std::getline(iss, y_str, '|')) {
					//     // Trim leading and trailing whitespacet
					//     y_str.erase(0, y_str.find_first_not_of(" \t\n\r"));
					//     y_str.erase(y_str.find_last_not_of(" \t\n\r") + 1);

					//     if (!y_str.empty()) {
					//         tupleShares.push_back(std::stoll(y_str));
					//     }
					// }
					allShares.push_back({tupleShares});
				}
				file.close();
			} else {
				std::cerr << "Error opening file: server_" << serverIndex << ".txt" << std::endl;
			}
		}
	}

		void populateStorage()
		{
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
}

// long long int logCapcityCalulator()
// {

// 	return 10;
// }

std::vector<std::vector<std::vector<int64_t>>> loadSecretShares(int n) {
	std::vector<std::vector<std::vector<int64_t>>> allShares;
	
	std::string baseDir = "../shares"; // Relative path to the shares directory

	std::filesystem::path path = std::filesystem::current_path();
		//fs::path path = fs::current_path();
		std::cout << "Current path is: " << path << std::endl;

	for (int serverIndex = 1; serverIndex <= n; ++serverIndex) {

		std::string filePath = baseDir + "/server_" + std::to_string(serverIndex) + ".txt";
		// Pass file path to ifstream
		std::ifstream file(filePath, std::ios::in);

		std::cout << "Attempting to open file: " << filePath << std::endl;

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
			std::cerr << "Error opening file: server_" << serverIndex << ".txt" << std::endl;
		}
	}
	return allShares;
}

// The total size of each tuple is:
// 	- 8 bytes * 16 attributes = 128 bytes
// So for each block we can

int main(int argc, char **argv)
{
	// Call the loadSecretShares function with the number of servers and check if the shares are loaded correctly
	// by printing the shares to the console
	std::vector<std::vector<std::vector<int64_t>>> allShares = loadSecretShares(6);
	// Print out allShares
	for (size_t i = 0; i < allShares.size(); ++i) {
		for (size_t j = 0; j < allShares[i].size(); ++j) {
			for (size_t k = 0; k < allShares[i][j].size(); ++k) {
				std::cout << allShares[i][j][k] << " ";
			}
			std::cout << std::endl;
		}
		std::cout << std::endl;
	}
	return 0;
}