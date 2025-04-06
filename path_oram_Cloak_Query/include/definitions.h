#pragma once

#include <boost/format.hpp>
#include <climits>
#include <string>
#include <vector>
#include <iostream>

#ifndef INPUT_CHECKS
#define INPUT_CHECKS true
#endif

#ifndef USE_REDIS
#define USE_REDIS true
#endif

// #ifndef USE_AEROSPIKE
// #define USE_AEROSPIKE true
// #endif

// use 256-bit security
#define KEYSIZE 32

// 256 bit hash size (SHA-256)
#define HASHSIZE 256
#define HASH_ALGORITHM EVP_sha256

// change to run all tests from different seed
#define TEST_SEED 0x13

namespace CloakQueryPathORAM
{
	using namespace std;

	// defines the integer type block ID
	// change (e.g. to unsigned int) if needed
	using number = unsigned long long;
	using uchar	 = unsigned char;
	using uint	 = unsigned int;
	using bytes	 = vector<uchar>;
	using block	 = pair<number, bytes>;
	using bucket = vector<block>;

	enum EncryptionMode
	{
		ENCRYPT,
		DECRYPT
	};

	enum BlockCipherMode
	{
		CBC,
		CTR,
		NONE
	};

	// Defining a serialization and deserialization function 
	// for container storing secret shared data
	inline bytes serialize(const vector<vector<int64_t>>& data) {
		bytes serializedData; // vector of uchar
		for (const auto& vec2d : data) {
			for (const auto& vec1d : vec2d) {
					uchar buffer[sizeof(int64_t)];
					memcpy(buffer, &vec1d, sizeof(int64_t));
					// Pointer arithmetic below:
					// buffer is a pointer to the start of the array
					// buffer + sizeof(int64_t) is the range (8 bytes) that needs 
					// to be appended to the serializedData
					serializedData.insert(serializedData.end(), buffer, buffer + sizeof(int64_t));

			}
		}
		return serializedData;
	}

	inline vector<vector<int64_t>> deserialize(const bytes& serializedData) {
		// vector<vector<int64_t>> data;

		// size_t totalTuples = serializedData.size() / 16;
		// std::cout << "Total number of tuples: " << totalTuples << endl;
		// for (int i = 0; i < 6; i++) { // Break the data into 6 parts
		// 	size_t offset = i * totalTuples * 16;
		// 	vector<vector<int64_t>> vec2d;
		// 	for (size_t j = 0; j < totalTuples; j++) {
		// 		vector<int64_t> vec1d;
		// 		for (size_t k = 0; k < 16; k++) {
		// 			int64_t value;
		// 			memcpy(&value, &serializedData[offset], sizeof(int64_t));
		// 			vec1d.push_back(value);
		// 			offset += sizeof(int64_t);
		// 		}
		// 		vec2d.push_back(vec1d);
		// 	}
		// 	data.push_back(vec2d);
		// }
		// std::cout << "Data size: " << data.size() << endl;
		// Total serialized data we receive is from a single server
		// We need to parse the data and store it in a 2D vector
		// Total number of tuples in the data is calculated by dividing the 
		// total size of the serialized data by the size of a single tuple
		// The total number of attrbutes in a tuple is 16
		// So, the total number of tuples is the total size of the serialized data
		// divided by 16
		// Below implementation is based on the above logic
		// TODO:: Implement logic for parsing data when we handle data from only one server.
		vector<vector<int64_t>> data;
		size_t offset = 0;
		// Debug dialog to check the size of serialized data
		//std::cout << "Total size of serialized data: " << serializedData.size() << endl; // 768 = 16 * 8 * 6
		size_t totalTuples = (serializedData.size() / 16) / 8; // 16 for the number of attributes in a tuple, 8 for number of bytes in an int64_t
		std::cout << "Total number of tuples: " << totalTuples << endl;
		for (size_t i = 0; i < totalTuples; i++) {
			vector<int64_t> vec1d;
			for (size_t j = 0; j < 16; j++) {
				int64_t value;
				memcpy(&value, &serializedData[offset], sizeof(int64_t));
				vec1d.push_back(value);
				offset += sizeof(int64_t);
			}
			data.push_back(vec1d); 
		}
		return data;
	}

	/**
	 * @brief Primitive exception class that passes along the excpetion message
	 *
	 * Can consume std::string, C-string and boost::format
	 */
	class Exception : public exception
	{
		public:
		explicit Exception(const char* message) :
			msg_(message)
		{
		}

		explicit Exception(const string& message) :
			msg_(message)
		{
		}

		explicit Exception(const boost::format& message) :
			msg_(boost::str(message))
		{
		}

		virtual ~Exception() throw() {}

		virtual const char* what() const throw()
		{
			return msg_.c_str();
		}

		protected:
		string msg_;
	};

	/**
	 * @brief global setting, block cipher mode which will be used for encryption
	 *
	 */
	inline BlockCipherMode __blockCipherMode = CBC;
}
