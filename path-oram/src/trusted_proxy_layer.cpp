#include "trusted_proxy_layer.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <random>
#include <sodium.h>
#include <string>
#include <cstring>
#include "shamir.h"


void handle_text(int n, int k);
void read_file(const char * efile, int n, int k);
void help(){
    printf("\nWelcome to Splitter!\n\n");
    printf("The (k, n) scheme is defaulted to nSharesTotal = %d and minShares = %d.\n", nSharesTotal, minShares);
    printf("Options:\n./main -config nSharesTotal minShares\n");
    printf("./main -encrypt input_file where input_file is the path to the file you want to encrypt\n");
    printf("./main -decrypt output_file where ouput_file specifies the \n\n");
}

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
        int eof;
        unsigned char tag;
        fp_s = fopen(sourceFile, "rb");
        fp_t = fopen(targetFile, "wb");
        fread(header, 1, sizeof header, fp_s);
        auto close_files_return = [&](){
            fclose(fp_t);
            fclose(fp_s);
            return -1;
        };
        if (crypto_secretstream_xchacha20poly1305_init_pull(&st, header, key) != 0) {
            close_files_return();
        }
        do {
            rlen = fread(buf_in, 1, sizeof buf_in, fp_s);
            eof = feof(fp_s);
            if (crypto_secretstream_xchacha20poly1305_pull(&st, buf_out, &out_len, &tag,
                                                        buf_in, rlen, NULL, 0) != 0) {
                close_files_return();
            }
            if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL && ! eof) {
                close_files_return();
            }
            fwrite(buf_out, 1, (size_t) out_len, fp_t);
        } while (! eof);
        
        // Defined an inline function for the code below
        // ret = 0;
        // ret:
        //     fclose(fp_t);
        //     fclose(fp_s);
        //     return ret;
        return 0;
    }

    void TrustedProxyLayer::split_keys(unsigned char array[])
    {
        vector<SecretPair> vec[nSharesTotal];
        int size = crypto_secretstream_xchacha20poly1305_KEYBYTES, i;
        size = ((size / 7) + 1) * 7;
        uint8_t buffer[size];
        for (unsigned int i = 0; i < crypto_secretstream_xchacha20poly1305_KEYBYTES; ++i){
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
            for (long unsigned int j = 0; j < vec[i - 1].size(); ++j){
                out << vec[i - 1][j].getY() << "\n";
            }
            out.close();
        }
    }

    int TrustedProxyLayer::shamirSecretSharing(int argc, char** argv)
    {
        if (sodium_init() != 0) {
        return 1;
        }  
        if (argc != 3 && argc != 4) {
            help();
        }
        
        if (argc == 4) {
            string option = argv[1];
            if (option == "-config"){
                remove("CONFIG_IMPORTANT.txt");
                ofstream out("CONFIG_IMPORTANT.txt");
                int n = 3, k = 6;
                printf("\nSet (k, n) scheme to (%d, %d)!\n\n", k, n);
                out << n << " " << k;
            }
            else help();
        }
        else {
            printf("\n");
            //TODO:: The set_vals function is not needed since the values are hardcoded
            //set_vals();
            // TODO:: The check function is not needed since the values are hardcoded
            //if (!check()) return 0;
            string option = "-decrypt";
            if (option == "-decrypt") {
                string outp = argv[2];
                clock_t begin = clock(), end;
                unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
                ll val;
                int size = crypto_secretstream_xchacha20poly1305_KEYBYTES, i, j;
                int blocks = (((size / 7) + 1) * 7) / 7;

                printf("Combining key and file according to (%d, %d) scheme...\n", minShares, nSharesTotal);
                
                vector<vector<SecretPair> > ans(blocks);      
                int ct = 0;      
                for (i = 1; i <= nSharesTotal; ++i){
                    string f = "key-" + to_string(i) + ".dat";
                    ifstream in(f);
                    if (!in.good()) continue;
                    for (j = 0; j < blocks; ++j){
                        in >> val;
                        ans[j].push_back(SecretPair((ll)i, val));
                    }
                    ++ct;
                    in.close();
                    remove(f.c_str());
                }

                if (ct < minShares) {
                    printf("Only %d key files found. %d required!\n", ct, minShares);
                    return 0;
                }

                for (i = 0; i < blocks; ++i){
                    if (size < 7){
                        val = restore(minShares, ans[i]);
                        memcpy(&key[i * 7], &val, size);
                        size = 0;
                    }
                    else {
                        val = restore(minShares, ans[i]);
                        memcpy(&key[i * 7], &val, 7);
                        size -= 7;
                    }
                }

                printf("Keys recovered...\n");
                
                handle_text(nSharesTotal, minShares);
                printf("File recovered...\n");

                if (decryptSecretSharedData(outp.c_str(), "combined_shares.dat", key) != 0) {
                    return 1;
                }
                printf("File decrypted...\n");

                remove("combined_shares.dat");
                printf("Cleaned folder...\n");

                end = clock();
                double esecs = double(end - begin) / CLOCKS_PER_SEC;
                printf("\nDecrypted and Combined in %f seconds.\n\n", esecs);
            }  

            else if (option == "-encrypt"){
                clock_t begin = clock(), end;
                unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
                string inp = argv[2];
                crypto_secretstream_xchacha20poly1305_keygen(key);
                
                printf("Splitting key and file according to (%d, %d) scheme...\n", minShares, nSharesTotal);

                if (createSecretSharedData("encrypted.dat", inp.c_str(), key) != 0) {
                    return 1;
                }
                printf("File encrypted...\n");

                split_keys(key);
                printf("Keys split...\n");

                read_file("encrypted.dat", nSharesTotal, minShares);
                printf("File split...\n");

                remove("encrypted.dat");
                printf("Cleaned folder...\n");

                end = clock();
                double esecs = double(end - begin) / CLOCKS_PER_SEC;
                printf("\nEncrypted and Split in %f seconds.\n\n", esecs);
            }
            else help();
        }
        return 0;   
    }
} // namespace PathORAM