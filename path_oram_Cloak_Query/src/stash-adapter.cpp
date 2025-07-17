#include "stash-adapter.hpp"

#include "utility.hpp"

#include <algorithm>
#include <boost/format.hpp>
#include <cstring>
#include <fstream>
#include <iterator>

namespace CloakQueryPathORAM
{
	using namespace std;
	using boost::format;

	AbsStashAdapter::~AbsStashAdapter() {}

	InMemoryStashAdapter::~InMemoryStashAdapter() {}

	InMemoryStashAdapter::InMemoryStashAdapter(const number capacity) :
		capacity(capacity)
	{
		this->stash = unordered_map<number, bytes>();
		stash.reserve(capacity);
	}

	void InMemoryStashAdapter::getAll(vector<block> &response) const
	{
		response.insert(response.begin(), stash.begin(), stash.end());

		const uint n = response.size();
		if (n >= 2)
		{
			// Fisher-Yates shuffle
			for (uint i = 0; i < n - 1; i++)
			{
				uint j = i + getRandomUInt(n - i);
				swap(response[i], response[j]);
			}
		}
	}

	void InMemoryStashAdapter::add(const number block, const bytes &data)
	{
		checkOverflow(block);
		// Skip or throw on invalid block IDs
		if (block == ULONG_MAX || block > 1e12) {
			std::cerr << "[Stash add] Skipping suspicious block ID: " << block << std::endl;
			return; // Or throw Exception if you want to be strict
		}
		// Pad or truncate to match the defined block size
		size_t blockSize = 0;
		if (!stash.empty()) {
			blockSize = stash.begin()->second.size();
		} else {
			blockSize = data.size(); // Use first block's size as reference
		}
		bytes adjustedData = data;
		if (data.size() < blockSize) {
			adjustedData.resize(blockSize, 0); // Pad with zeros
		} else if (data.size() > blockSize) {
			adjustedData.resize(blockSize); // Truncate
		}
		stash.insert({block, adjustedData});
	}

	void InMemoryStashAdapter::update(const number block, const bytes &data)
	{
		checkOverflow(block);
		size_t blockSize = 0;
		if (!stash.empty()) {
			blockSize = stash.begin()->second.size();
		} else {
			blockSize = data.size();
		}
		bytes adjustedData = data;
		if (data.size() < blockSize) {
			adjustedData.resize(blockSize, 0);
		} else if (data.size() > blockSize) {
			adjustedData.resize(blockSize);
		}
		stash[block] = adjustedData;
	}

	void InMemoryStashAdapter::get(const number block, bytes &response) const
	{
		const auto found = stash.find(block);
		if (found != stash.end())
		{
			response.insert(response.begin(), (*found).second.begin(), (*found).second.end());
		}
	}

	void InMemoryStashAdapter::deleteBlock(const number block)
	{
		stash.erase(block);
	}

	void InMemoryStashAdapter::checkOverflow(const number block) const
	{
#if INPUT_CHECKS
		if (stash.size() == capacity && stash.count(block) == 0)
		{
			throw Exception(boost::format("trying to insert over capacity (capacity %1%)") % capacity);
		}
#endif
	}

	bool InMemoryStashAdapter::exists(const number block) const
	{
		return stash.count(block) > 0;
	}

	number InMemoryStashAdapter::currentSize()
	{
		return stash.size();
	}

	void InMemoryStashAdapter::storeToFile(const string filename) const
	{
		const auto flags = fstream::out | fstream::binary | fstream::trunc;
		fstream file;

		file.open(filename, flags);
		if (!file)
		{
			throw Exception(boost::format("cannot open %1%: %2%") % filename % strerror(errno));
		}

		if (stash.size() > 0)
		{
			auto blockSize = stash.begin()->second.size();
			auto recordSize = sizeof(number) + blockSize;
			size_t totalSize = stash.size() * recordSize;
			std::vector<unsigned char> buffer(totalSize);

			// Sanity check: ensure all blocks are the same size
			for (const auto& record : stash) {
				if (record.second.size() != blockSize) {
					file.close();
					throw Exception(boost::format("Block size mismatch in stash: expected %1%, got %2% for block %3%") % blockSize % record.second.size() % record.first);
				}
			}

			auto i = 0;
			for (auto &&record : stash)
			{
				number numberBuffer[1] = {record.first};
				// Buffer overflow check
				size_t offset = recordSize * i;
				if (offset + recordSize > buffer.size()) {
					file.close();
					throw Exception("Buffer overflow detected during stash serialization");
				}
				copy((unsigned char *)numberBuffer, (unsigned char *)numberBuffer + sizeof(number), buffer.begin() + offset);
				copy(record.second.begin(), record.second.end(), buffer.begin() + offset + sizeof(number));
				i++;
			}

			file.seekg(0, file.beg);
			file.write((const char *)buffer.data(), stash.size() * recordSize);
			file.close();
		}
	}

	void InMemoryStashAdapter::loadFromFile(const string filename, const int blockSize)
	{
		const auto flags = fstream::in | fstream::binary | fstream::ate;
		fstream file;

		file.open(filename, flags);
		if (!file)
		{
			throw Exception(boost::format("cannot open %1%: %2%") % filename % strerror(errno));
		}
		const auto size = (int)file.tellg();
		file.seekg(0, file.beg);

		if (size > 0)
		{
			const auto recordSize = sizeof(number) + blockSize;
			if (size % recordSize != 0) {
				std::cerr << "[WARNING] Stash file size " << size << " is not a multiple of record size " << recordSize << ". File may be corrupted. Skipping incomplete record." << std::endl;
			}
			std::vector<unsigned char> buffer(size);
			file.read((char *)buffer.data(), size);
			file.close();

			const int numRecords = size / recordSize;
			for (int i = 0; i < numRecords; i++)
			{
				int offset = i * recordSize;
				unsigned char numberBuffer[sizeof(number)];
				copy(buffer.begin() + offset, buffer.begin() + offset + sizeof(number), numberBuffer);
				number block = ((number *)numberBuffer)[0];

				// Print/log block ID for debugging
				std::cout << "[Stash load] Block ID: " << block << std::endl;

				// Optionally, skip obviously invalid block IDs (e.g., > 1e12 or 0xFFFFFFFFFFFF)
				if (block == ULONG_MAX || block > 1e12) {
					std::cerr << "[WARNING] Skipping suspicious block ID: " << block << std::endl;
					continue;
				}

				bytes data;
				data.resize(blockSize);
				copy(buffer.begin() + offset + sizeof(number), buffer.begin() + offset + sizeof(number) + blockSize, data.begin());

				stash.insert({block, data});
			}
		}
	}
}
