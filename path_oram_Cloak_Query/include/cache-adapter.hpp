#pragma once

#include "definitions.h"
#include "storage-adapter.hpp"
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <memory>

namespace CloakQueryPathORAM
{
    using namespace std;

    /**
     * @brief Abstraction over cache
     *
     */
    class AbsCacheAdapter
    {
    public:
        /**
         * @brief get the block from the stash in case it is found
         *
         * @param block block in question
         * @param response response to be populated
         */
        virtual void getCacheBlockEntry(const number block, bytes &response) = 0;

        /**
         * @brief update the cache with new block entry and manage eviction
         *
         * @param block block in question
         * @param data data to be put
         */
        virtual void updateCacheBlockEntry(const number block, const bytes &data) = 0;
        /**
         * @brief remove block from cache incase of eviction
         *
         * @param block block in question
         */
        virtual void evictBlock(const number block) = 0;
        /**
         * @brief write block from cache to binary tree incase of eviction
         *
         * @param block block in question
         */
        virtual void writetoStorage(const number block, const bytes &data) = 0;

        virtual ~AbsCacheAdapter() = 0;
    
    private:
        /* data */
        unordered_map<number, bytes> cache;
        unordered_map<number, chrono::steady_clock::time_point> accessTimes;
        unordered_set<number> dirtyBlocks;
        size_t cacheSizeLimit;
        shared_ptr<AbsStorageAdapter> storage;

        bool isBlockDirty;
    };

} // namespace CloakQueryPathORAM