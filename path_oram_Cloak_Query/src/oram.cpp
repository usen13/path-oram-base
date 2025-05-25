#include "oram.hpp"

#include "utility.hpp"

#include <boost/format.hpp>
#include <filesystem>
#include <fstream>

namespace CloakQueryPathORAM
{
	using namespace std;
	using boost::format;
	bytes ORAM::key;
	bool ORAM::isKeyGenerated = false;

	ORAM::ORAM(
		const number logCapacity,
		const number blockSize,
		const number Z,
		 shared_ptr<AbsStorageAdapter> storage,
		const shared_ptr<AbsPositionMapAdapter> map,
		const shared_ptr<AbsStashAdapter> stash,
		const bool initialize,
		const number batchSize) :
		storage(storage),
		map(map),
		stash(stash),
		dataSize(blockSize),
		Z(Z),
		height(logCapacity),
		buckets((number)1 << logCapacity),
		blocks(((number)1 << logCapacity) * Z),
		batchSize(batchSize),
		isInitializing(initialize)
	{
		//Generate a random key for HMAC
		if (!isKeyGenerated)
		{
			key = generateKey();
			isKeyGenerated = true;
		}
		
		if (initialize)
		{
			// fill all blocks with random bits, marks them as "empty"
			storage->fillWithZeroes();

			// generate random position map
			for (number i = 0; i < blocks; ++i)
			{
				map->set(i, getRandomULong(1 << (height - 1)));
			}
			
			// Compute and store MACs for all buckets
			//computeAndStoreAllBucketMACs();

			// Initialization is complete and the verification of MAC should be done from here on
			//isInitializing = false;
		}
	}

	ORAM::ORAM(const number logCapacity, const number blockSize, const number Z) :
		ORAM(logCapacity,
			 blockSize,
			 Z,
			 make_shared<InMemoryStorageAdapter>((1 << logCapacity), blockSize, bytes(), Z),
			 make_shared<InMemoryPositionMapAdapter>(((1 << logCapacity) * Z) + Z),
			 make_shared<InMemoryStashAdapter>(3 * logCapacity * Z))
	{
	}

	void ORAM::get(const number block, bytes &response)
	{
		bytes data;
		access(true, block, data, response);
		syncCache();
	}

	void ORAM::put(const number block, const bytes &data)
	{
		std::cout << "Put function call" << std::endl;
		std::cout << "Putting data for block: " << block << ", Data size: " << data.size() << std::endl;
		bytes response;
		access(false, block, data, response);
		syncCache();
	}

	void ORAM::multiple(const vector<block> &requests, vector<bytes> &response)
	{
#if INPUT_CHECKS
		if (requests.size() > batchSize)
		{
			throw Exception(boost::format("Too many requests (%1%) for batch size %2%") % requests.size() % batchSize);
		}
#endif

		// populate cache
		unordered_set<number> locations;
		for (auto &&request : requests)
		{
			readPath(map->get(request.first), locations, false);
		}

		vector<block> cacheResponse;
		getCache(locations, cacheResponse, true);

		// run ORAM protocol (will use cache)
		response.resize(requests.size());
		for (auto i = 0u; i < requests.size(); i++)
		{
			access(requests[i].second.size() == 0, requests[i].first, requests[i].second, response[i]);
		}

		// upload resulting new data
		syncCache();
	}

	void ORAM::load(vector<block> &data)
	{
		const number maxLocation = 1 << height;
		const auto bucketCount	 = (data.size() + Z - 1) / Z; // for rounding errors
		const auto step			 = maxLocation / (long double)bucketCount;

		if (bucketCount > maxLocation)
		{
			throw Exception("bulk load: too much data for ORAM");
		}

		vector<pair<const number, bucket>> writeRequests;
		writeRequests.reserve(bucketCount);

		// shuffle (such bulk load may leak in part the original order)
		const uint n = data.size();
		if (n >= 2)
		{
			// Fisher-Yates shuffle
			for (uint i = 0; i < n - 1; i++)
			{
				uint j = i + getRandomUInt(n - i);
				swap(data[i], data[j]);
			}
		}

		auto iteration = 0uLL;
		bucket bucket;
		for (auto &&record : data)
		{
			// to disperse locations evenly from 1 to maxLocation
			const auto location	  = (number)floor(1 + iteration * step);
			const auto [from, to] = leavesForLocation(location);
			map->set(record.first, getRandomULong(to - from + 1) + from);

			if (bucket.size() < Z)
			{
				bucket.push_back(record);
			}
			if (bucket.size() == Z)
			{
				writeRequests.push_back({location, bucket});
				iteration++;
				bucket.clear();
			}
		}
		const auto location = (number)floor(1 + iteration * step);
		if (bucket.size() > 0)
		{
			for (auto i = 0u; i < bucket.size() - Z; i++)
			{
				bucket.push_back({ULONG_MAX, bytes()});
			}
			writeRequests.push_back({location, bucket});
		}

		storage->set(boost::make_iterator_range(writeRequests.begin(), writeRequests.end()));
	}

	void ORAM::access(const bool read, const number block, const bytes &data, bytes &response)
	{
		std::cout << "Accessing and remapping block: " << block << std::endl;
		// step 1 from paper: remap block
		const auto previousPosition = map->get(block);
		map->set(block, getRandomULong(1 << (height - 1)));
		// Increment the access count for that block
		accessCount[block]++;

		// step 2 from paper: read path
		unordered_set<number> path;
		readPath(previousPosition, path, true); // stash updated

		// step 3 from paper: update block
		if (!read) // if "write"
		{
			stash->update(block, data);
		}
		stash->get(block, response);

		// step 4 from paper: write path
		writePath(previousPosition); // stash updated
	}

	uint64_t ORAM::getAccessCount(const number block) const
	{
		auto it = accessCount.find(block);
		if (it != accessCount.end())
		{
			return it->second;
		}
		return 0;
	}

	void ORAM::putContainer(const number block, const vector<vector<int64_t>> &container)
	{
		std::cout << "Putting container for block: " << block << std::endl;
		bytes serializedData = serialize(container);

		// Manage the scenario when the data is smaller than the block size
		uint64_t dataSize = serializedData.size();

		bytes macSerializedData = hmac(key, serializedData);

		//Print out the mac of the serialized data
		std::cout << "MAC of serialized data: ";
		for (const auto &byte : macSerializedData)
		{
			std::cout << std::hex << (int)byte;
		}
		std::cout << std::endl << "Size of MAC:" << macSerializedData.size() << std::endl;
		// Create a new variable with the size of the data
		bytes dataWithSize(sizeof(dataSize));
		memcpy(dataWithSize.data(), &dataSize, sizeof(dataSize));
		// Append the serialized data to variable crafted above
		dataWithSize.insert(dataWithSize.end(), serializedData.begin(), serializedData.end());
		
		// Return the data to the ORAM
		put(block, dataWithSize);
	}

	vector<vector<int64_t>> ORAM::getContainer(const number block)
	{
		std::cout << "Getting container for block: " << block << std::endl;
		bytes blockData;
		get(block, blockData);
		std::cout << "Getting container for block: " << block << ", Data size: " << blockData.size() << std::endl;
		// Check if the block is empty
		if (blockData.empty())
		{
			std::cout << "Block data is empty." << std::endl;
			return vector<vector<int64_t>>(); // Return an empty vector if the block is empty
		}
		// Extract the size of the data
		uint64_t dataSize = 0;
		memcpy(&dataSize, blockData.data(), sizeof(dataSize));

		// Extract only the serialized data using the size
    	bytes serializedData(blockData.begin() + sizeof(dataSize), blockData.begin() + sizeof(dataSize) + dataSize);

		return deserialize(serializedData);
	}

	void ORAM::readPath(const number leaf, unordered_set<number> &path, const bool putInStash)
	{
		std::cout << "Reading path for leaf: " << leaf << std::endl;
		// for levels from root to leaf
		for (number level = 0; level < height; level++)
		{
			const auto bucket = bucketForLevelLeaf(level, leaf);
			path.insert(bucket);
		}

		// we may only want to populate cache
		if (putInStash)
		{
			vector<block> blocks;
			getCache(path, blocks, false);
			// print blocks
			for (const auto &block : blocks)
			{
				std::cout << "Block ID::::::: " << block.first << ", Data Size: " << block.second.size() << std::endl;
			}

			for (auto &&[id, data] : blocks)
			{
				// skip "empty" buckets
				if (id != ULONG_MAX)
				{
					stash->add(id, data);
				}
			}

			// Verify the integrity of each bucket in the path
			for (const auto &bucketId : path)
			{
				std::cout << "Verifying bucket integrity for ID: " << bucketId << std::endl;
				const auto &bucketData = cache[bucketId]; // Retrieve the bucket from the cach
				
				number level = 0;
				number currentLeaf = leaf;
				while (bucketForLevelLeaf(level, currentLeaf) != bucketId && level < height)
				{
					level++;
				}

				if (!verifyBucketMAC(level, leaf, bucketData))
				{
					throw Exception("Bucket integrity check failed during readPath for bucket ID: " + std::to_string(bucketId));
				}
			}
		}
	}

	void ORAM::writePath(const number leaf)
	{
		std::cout << "Writing path for leaf: " << leaf << std::endl;
		vector<block> currentStash;
		stash->getAll(currentStash);

		vector<int> toDelete;				   // rember the records that will need to be deleted from stash
		vector<pair<number, bucket>> requests; // storage SET requests (batching)

		// following the path from leaf to root (greedy)
		for (int level = height - 1; level >= 0; level--)
		{
			vector<block> toInsert;		  // block to be insterted in the bucket (up to Z)
			vector<number> toDeleteLocal; // same blocks needs to be deleted from stash (these hold indices of elements in currentStash)

			for (number i = 0; i < currentStash.size(); i++)
			{
				const auto &entry	 = currentStash[i];
				const auto entryLeaf = map->get(entry.first);
				// see if this block from stash fits in this bucket
				if (canInclude(entryLeaf, leaf, level))
				{
					toInsert.push_back(entry);
					toDelete.push_back(entry.first);

					toDeleteLocal.push_back(i);

					// look up to Z
					if (toInsert.size() == Z)
					{
						break;
					}
				}
			}
			// delete inserted blocks from local stash
			// we remove elements by location, so after operation vector shrinks (nasty bug...)
			sort(toDeleteLocal.begin(), toDeleteLocal.end(), greater<number>());
			for (auto &&removed : toDeleteLocal)
			{
				currentStash.erase(currentStash.begin() + removed);
			}

			const auto bucketId = bucketForLevelLeaf(level, leaf);
			bucket bucketData;
			bucketData.resize(Z);

			// write the bucket
			// Fill Z blocks with data or dummy blocks
			for (number i = 0; i < Z; i++)
			{
				// auto block = bucket * Z + i;
				if (toInsert.size() != 0)
				{
					bucketData[i] = toInsert.back();
					toInsert.pop_back();
				}
				else
				{
					// if nothing to insert, insert dummy (for security)
					bucketData[i] = {ULONG_MAX, getRandomBlock(dataSize)};
				}
			}
			//if (!isInitializing) {
				computeAndStoreBucketMAC(level, leaf, bucketData);	
			//}
			

			// Verify the integrity of the bucket before writing it back
			if (!verifyBucketMAC(level, leaf, bucketData))
			{
				throw Exception("Bucket integrity check failed during writePath for bucket ID: " + std::to_string(bucketId));
			}

			requests.push_back({bucketId, bucketData});
		}

		setCache(requests);

		// update the stash adapter, remove newly inserted blocks
		for (auto &&removed : toDelete)
		{
			stash->deleteBlock(removed);
		}

		std::cout << "Write path completed for leaf: " << leaf << std::endl;
	}

	void ORAM::computeAndStoreAllBucketMACs()
	{
		std::cout << "Computing and storing MACs for all buckets..." << std::endl;
		std::cout << "Number of buckets: " << buckets << std::endl;

		// Iterate through all the buckets in the ORAM
		// for (number bucketID = 0; bucketID < buckets; ++bucketID)
		// {
		// 	unordered_set<number> bucketLocation = {bucketID};
		// 	vector<block> bucketBlocks;
		// 	std::cout << "Entered the for loop..." << std::endl;

		// 	getCache(bucketLocation, bucketBlocks, false);
		// 	// print bucketBlocks
		// 	for (const auto &block : bucketBlocks)
		// 	{
		// 		std::cout << "Block ID: " << block.first << ", Data Size: " << block.second.size() << std::endl;
		// 	}

		// 	// Compute the MAC for the current bucket

		// 	computeAndStoreBucketMAC(level, leaf, bucketBlocks);
			
		// 	// Update the bucket in the cache
		// 	setCache({{bucketID, bucketBlocks}});
		// }
		std::cout << "Height of the ORAM: " << height << std::endl;
		for (number level = 0; level <= height; ++level)
		{
			const number numBucketsAtLevel = 1 << level; // Number of buckets at the current level
			for (number bucketIndex = 0; bucketIndex < numBucketsAtLevel; ++bucketIndex)
			{
				const number leaf = bucketIndex << (height - level - 1); // Calculate the leaf for the bucket
				const number bucketId = bucketForLevelLeaf(level, leaf);

				unordered_set<number> bucketLocation = {bucketId};
				vector<block> bucketBlocks;

				getCache(bucketLocation, bucketBlocks, false);

				// Compute and store the MAC for the current bucket
				computeAndStoreBucketMAC(level, leaf, bucketBlocks);

				// Update the bucket in the cache
				setCache({{bucketId, bucketBlocks}});
			}
		}

		syncCache();
		isInitializing = false;
	}

	number ORAM::bucketForLevelLeaf(const number level, const number leaf) const
	{
		return (leaf + (1 << (height - 1))) >> (height - 1 - level);
	}

	bool ORAM::canInclude(const number pathLeaf, const number blockPosition, const number level) const
	{
		// on this level, do these paths share the same bucket
		return bucketForLevelLeaf(level, pathLeaf) == bucketForLevelLeaf(level, blockPosition);
	}

	pair<number, number> ORAM::leavesForLocation(const number location)
	{
		const auto level	= (number)floor(log2(location));
		const auto toLeaves = height - level - 1;
		return {location * (1 << toLeaves) - (1 << (height - 1)), (location + 1) * (1 << toLeaves) - 1 - (1 << (height - 1))};
	}

	void ORAM::getCache(const unordered_set<number> &locations, vector<block> &response, const bool dryRun)
	{
		// get those locations not present in the cache
		vector<number> toGet;
		for (auto &&location : locations)
		{
			const auto bucketIt = cache.find(location);
			if (bucketIt == cache.end())
			{
				toGet.push_back(location);
			}
			else if (!dryRun)
			{
				response.insert(response.begin(), (*bucketIt).second.begin(), (*bucketIt).second.end());
			}
		}

		if (toGet.size() > 0)
		{
			// download those blocks
			vector<block> downloaded;
			storage->get(toGet, downloaded);

			// add them to the cache and the result
			bucket bucket;
			for (auto i = 0uLL; i < downloaded.size(); i++)
			{
				if (!dryRun)
				{
					response.push_back(downloaded[i]);
				}
				bucket.push_back(downloaded[i]);
				if (i % Z == Z - 1)
				{
					// Calculate the level and leaf for the current bucket
					number bucketId = toGet[i / Z];
					number level = 0;
					number leaf = 0;
					while (bucketForLevelLeaf(level, bucketId) != toGet[i / Z] && level < height)
					{
						level++;
						leaf = bucketId << (height - level - 1);
					}
					// Verify the integrity of the bucket before writing it back
					if (!verifyBucketMAC(level, leaf, bucket))
					{
						throw Exception("Bucket integrity check failed during getCache for bucket ID: " + std::to_string(toGet[i / Z]));
					}
					cache[toGet[i / Z]] = bucket;
					bucket.clear();
				}
			}
		}
	}

	void ORAM::setCache(const vector<pair<number, bucket>> &requests)
	{
		// std::cout << "Setting cache..." << std::endl;
		for (auto &&request : requests)
		{
			// Print the size of the data being set in the cache
			std::cout << "Setting cache for bucket ID: " << request.first << ", Number of blocks: " << request.second.size() << std::endl;
			cache[request.first] = request.second;
		}
	}

	void ORAM::syncCache()
	{
		storage->set(boost::make_iterator_range(cache.begin(), cache.end()));

		cache.clear();
	}

	void ORAM::computeAndStoreBucketMAC(const number level, const number leaf, const bucket &bucketData)
	{
		std::cout << "Currently at level: " << level << ", leaf: " << leaf << std::endl;
		//Dynamically calculate the bucket ID
		const number bucketId = bucketForLevelLeaf(level, leaf);

		bytes concatenatedData;
		std::cout << "Computing MAC for bucket..." << bucketId << std::endl;

		// Compute the MAC for each block in the bucket
		for (size_t i = 0; i < Z; i++)
		{
				concatenatedData.insert(concatenatedData.end(), bucketData[i].second.begin(), bucketData[i].second.end());
		}

		// Print the block ID in the bucket and the corresponding data size
		for (const auto &block : bucketData)
		{
			std::cout << "Block ID durin MAC computing: " << block.first << ", Data Size: " << block.second.size() << std::endl;
		}

		// Compute MAC using the stored key 
		bytes hash = hmac(key, concatenatedData);
		
		macMap[bucketId] = hash; // Store the MAC in the map
		// Print macMap
		std::cout << "MAC for bucket ID " << bucketId << ": ";
		for (const auto &byte : hash)
		{
			std::cout << std::hex << static_cast<int>(byte);
		}
		std::cout << std::endl;
		//hash.resize(dataSize, 0x00); // Resize to match the block size
		//bucketData[Z-1].first = ULONG_MAX; // Use ULONG_MAX as the block ID for the hash
		//bucketData[Z-1].second = hash; 
		//std::cout << "Block hash size for bucket: " << bucketData[Z-1].second.size() << std::endl;
		// Debug: Print the size of the MAC
		std::cout << "Stored MAC for bucket ID " << bucketId << ": " << hash.size() << " bytes" << std::endl;
	}

	bytes loadKeyFromFile(const std::string &filename)
	{
		if (!std::filesystem::exists(filename))
		{
			throw std::runtime_error("Key file does not exist: " + filename);
		}
		// Read the key from the file
		std::ifstream file(filename, std::ios::binary);
		bytes key(KEYSIZE);
		file.read(reinterpret_cast<char *>(key.data()), KEYSIZE);
		file.close();
		return key;
	}

	void saveKeyToFile (const bytes &key, const std::string &filename)
	{
		std::string keyDir = "../key";
		if (!std::filesystem::exists(keyDir))
		{
			std::filesystem::create_directory(keyDir);
		}
		std::string keyPath = keyDir + "/key.bin";
		std::ofstream file(keyPath, std::ios::binary);
		if (!file)
		{
			throw std::runtime_error("Error opening file for generated key to path: " + keyPath);
		}
		file.write(reinterpret_cast<const char *>(key.data()), key.size());
		file.close();
	}
	
	bytes ORAM::generateKey() const
	{
		std::cout << "Generating key..." << std::endl;
		if (std::filesystem::exists("../key/key.bin"))
		{
			return loadKeyFromFile("../key/key.bin"); // Load the key from a file if it exists
		}

		bytes key(KEYSIZE, 0);
		for (size_t i = 0; i < KEYSIZE; ++i)
		{
			key[i] = static_cast<uchar>(rand() % 256); // Generate a random byte
		}
		saveKeyToFile(key, "../key/key.bin"); // Save the key to a file
		return key;
	}

	// Getter for the key
	const bytes& ORAM::getKey() const
	{
		return key;
	}

	bool ORAM::verifyBucketMAC(const number level, const number leaf, const bucket &bucketData) const
	{
		// Dynamically calculate the bucket ID
		const number bucketId = bucketForLevelLeaf(level, leaf);

		// During intitlization the verification should be skipped
		if (isInitializing)	{
			std::cout << "Ignore verification during initilaization " << std::endl;
			return true; 
		}

		// Retrieve the stored MAC for the bucket
		auto it = macMap.find(bucketId);
		
		// Check if the MAC exists for the given bucket ID
		if (it == macMap.end())
		{
			throw Exception("MAC not found for bucket ID: " + std::to_string(bucketId));
		}
		const bytes &storedMAC = it->second;
		// Print the bucket ID
		std::cout << "Verifying MAC for bucket ID: " << bucketId << std::endl;

		// Concatenate the data from the other blocks in the bucket
		bytes concatenatedData;
		for (size_t i = 0; i < Z; i++)
		{
				// Concatenate the data from the blocks
				concatenatedData.insert(concatenatedData.end(), bucketData[i].second.begin(), bucketData[i].second.end());
		}
		// Compute the MAC for the concatenated data using the stored key
		bytes computedMAC = hmac(key, concatenatedData);
		std::cout << "Stored MAC: " << storedMAC.size() << std::endl;
		// Print Stored MAC
		for (const auto &byte : storedMAC)
		{
			std::cout << std::hex << static_cast<int>(byte);
		}
		std::cout << std::endl;
		std::cout << "Computed MAC: " << computedMAC.size() << std::endl;
		// Print Computed MAC
		for (const auto &byte : computedMAC)
		{
			std::cout << std::hex << static_cast<int>(byte);
		}
		std::cout << std::endl;

		if (computedMAC != computedMAC)
		{
			throw Exception("Bucket integrity check failed: MAC mismatch");
			return false; // Integrity check failed
		}
		else return true; // Integrity check passed
	}

	void ORAM::saveMacMap(const std::string &filename) const
	{
		std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);
		uint64_t count = macMap.size();
		ofs.write(reinterpret_cast<const char*>(&count), sizeof(count));
		for (const auto &pair : macMap) {
			ofs.write(reinterpret_cast<const char*>(&pair.first), sizeof(pair.first));
			uint64_t size = pair.second.size();
			ofs.write(reinterpret_cast<const char*>(&size), sizeof(size));
			ofs.write(reinterpret_cast<const char*>(pair.second.data()), size);
		}
	}

	void ORAM::loadMacMap(const std::string &filename)
	{
		std::ifstream ifs(filename, std::ios::binary);
		uint64_t count;
		ifs.read(reinterpret_cast<char*>(&count), sizeof(count));
		for (uint64_t i = 0; i < count; ++i) {
			number key;
			ifs.read(reinterpret_cast<char*>(&key), sizeof(key));
			uint64_t size;
			ifs.read(reinterpret_cast<char*>(&size), sizeof(size));
			bytes value(size);
			ifs.read(reinterpret_cast<char*>(value.data()), size);
			macMap[key] = value;
		}
	}
}
