#pragma once 

#include "cache-adapter.hpp"
#include "stash-adapter.hpp"
#include "position-map-adapter.hpp"
#include "utility.hpp"
#include <sodium.h>

#define CHUNK_SIZE 4096

namespace PathORAM
{
    using namespace std;

   	/**
	 * @brief Creation of a Trusted Proxy Layer which takes care of the following:
     * 1. Encrypts and send secret shared data to the servers.
     * 2. Looks up value in the position map and sends the path request to the server.
     * 3. Stores the binary path received from the servers in the stash.
     * 4. Reconstructs the data received from the server.
     * 5. Calculates the query result and sends it to the client.
     * 6. Stores the result in the cache.
     * 7. Manages re write/change of the node in the binary tree.
	 */
    class TrustedProxyLayer : public AbsCacheAdapter, public AbsStashAdapter, public AbsPositionMapAdapter
    {
        public:
        /**
         * @brief create secret shared data for the client
         *
         * @param targetFile file to which the secret shared data needs to be written 
         * @param sourceFile file which needs to be encrypted
         * @param key the key to use AEAD encryption
         */
        static int createSecretSharedData(const char *targetFile, const char *sourceFile, const unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES]);
        /**
         * @brief create secret shared data for the client
         *
         * @param targetFile the decrypted file  
         * @param sourceFile various shares of the secret shared data
         * @param key key shares to decrypt the secret shared data
         */
        static int decryptSecretSharedData(const char *targetFile, const char *sourceFile, const unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES]);
        /**
         * @brief splits the keys to be shared among the servers
         *
         * @param array takes the AEAD key to be split
         */
        void split_keys(unsigned char array[]);
        /**
         * @brief main function which is invoked to create the secret shared data and keys
         * 
         * @param argc number of arguments
         * @param argv arguments passed to the function
         */
        int shamirSecretSharing(int argc, char** argv);
        /**
         * @brief function to check if the total shares are less than the minimum shares required
         *
         */
        void check(); // Probably not needed since the value is hardcoded

        int nSharesTotal = 6; // Total secret shared to be created for the data and keys
        int minShares = 3;  // Minimum number of shares required to reconstruct the data

        // Initialize the constructor with the total shares and minimum shares required.
        TrustedProxyLayer(const int nSharesTotal, const int minShares);

        // The destructor for the Trusted Proxy Layer
        ~TrustedProxyLayer();

        private:

    };
}
