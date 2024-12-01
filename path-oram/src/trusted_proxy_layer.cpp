#include "trusted_proxy_layer.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <random>
#include <sodium.h>
#include <string>
#include <cstring>
#include "shamir.h"

namespace PathORAM
{

    // Creating an object of the TrustedProxyLayer class
    TrustedProxyLayer::TrustedProxyLayer(const int nSharesTotal, const int minShares) : nSharesTotal(nSharesTotal), minShares(minShares) {};

    int TrustedProxyLayer::createSecretSharedData(const char *targetFile, const char *sourceFile, const unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES])
    {
        unsigned char  buf_in[CHUNK_SIZE];
        unsigned char  buf_out[CHUNK_SIZE + crypto_secretstream_xchacha20poly1305_ABYTES];
        unsigned char  header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
        crypto_secretstream_xchacha20poly1305_state st;
        FILE *fp_t, *fp_s;
        unsigned long long out_len;
        size_t rlen;
        int eof;
        unsigned char tag;
        fp_s = fopen(sourceFile, "rb");
        fp_t = fopen(targetFile, "wb");
        crypto_secretstream_xchacha20poly1305_init_push(&st, header, key);
        fwrite(header, 1, sizeof header, fp_t);
        do {
            rlen = fread(buf_in, 1, sizeof buf_in, fp_s);
            eof = feof(fp_s);
            tag = eof ? crypto_secretstream_xchacha20poly1305_TAG_FINAL : 0;
            crypto_secretstream_xchacha20poly1305_push(&st, buf_out, &out_len, buf_in, rlen, NULL, 0, tag);
            fwrite(buf_out, 1, (size_t) out_len, fp_t);
        } while (! eof);
        fclose(fp_t);
        fclose(fp_s);
        return 0;
    }

    int TrustedProxyLayer::decryptSecretSharedData(const char *targetFile, const char *sourceFile, const unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES])
    {
        unsigned char  buf_in[CHUNK_SIZE + crypto_secretstream_xchacha20poly1305_ABYTES], 
        buf_out[CHUNK_SIZE], 
        header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
        crypto_secretstream_xchacha20poly1305_state st;
        FILE *fp_t, *fp_s;
        unsigned long long out_len;
        size_t rlen;
        int eof, ret = -1;
        unsigned char tag;
        fp_s = fopen(sourceFile, "rb");
        fp_t = fopen(targetFile, "wb");
        fread(header, 1, sizeof header, fp_s);
        if (crypto_secretstream_xchacha20poly1305_init_pull(&st, header, key) != 0) {
            //goto ret; /* incomplete header */
            // Define an inline function which closes the file and returns an integer value of -1
            auto ret = [&](){
                fclose(fp_t);
                fclose(fp_s);
                return -1;
            };
        }
        do {
            rlen = fread(buf_in, 1, sizeof buf_in, fp_s);
            eof = feof(fp_s);
            if (crypto_secretstream_xchacha20poly1305_pull(&st, buf_out, &out_len, &tag,
                                                        buf_in, rlen, NULL, 0) != 0) {
                //goto ret; /* corrupted chunk */
                // Define an inline function which closes the file and returns an integer value of -1
                auto ret = [&](){
                    fclose(fp_t);
                    fclose(fp_s);
                    return -1;
                };
            }
            if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL && ! eof) {
                //goto ret; /* premature end (end of file reached before the end of the stream) */
                // Define an inline function which closes the file and returns an integer value of -1
                auto ret = [&](){
                    fclose(fp_t);
                    fclose(fp_s);
                    return -1;
                };
            }
            fwrite(buf_out, 1, (size_t) out_len, fp_t);
        } while (! eof);
        
        // Defined an inline function for the code below
        // ret = 0;
        // ret:
        //     fclose(fp_t);
        //     fclose(fp_s);
        //     return ret;
    }

    void TrustedProxyLayer::split_keys(unsigned char array[])
    {
        vector<SecretPair> vec[nSharesTotal];
        int size = crypto_secretstream_xchacha20poly1305_KEYBYTES, i, j;
        size = ((size / 7) + 1) * 7;
        uint8_t buffer[size];
        for (i = 0; i < crypto_secretstream_xchacha20poly1305_KEYBYTES; ++i){
            if (i < crypto_secretstream_xchacha20poly1305_KEYBYTES) buffer[i] = array[i];
            else buffer[i] = 0;
        }
        ll tmp;
        uint8_t *ptr = &buffer[0];
        while (size){
            tmp = 0;
            memcpy(&tmp, ptr, 7);
            vector<SecretPair> tmpvec = split(tmp, nSharesTotal, minShares);
            for (i = 0; i < nSharesTotal; ++i) vec[i].push_back(tmpvec[i]);
            ptr += 7;
            size -= 7;
        }
        for (i = 1; i <= nSharesTotal; ++i){
            string rm = "key-" + to_string(i) + ".dat";
            remove(rm.c_str());
        }
        for (i = 1; i <= nSharesTotal; ++i){
            ofstream out("key-" + to_string(i) + ".dat");
            for (j = 0; j < vec[i - 1].size(); ++j){
                out << vec[i - 1][j].getY() << "\n";
            }
            out.close();
        }
    }
} // namespace PathORAM