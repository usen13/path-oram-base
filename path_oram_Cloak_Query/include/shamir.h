#include <iostream>
#include <fstream>
#include <stdlib.h>
#include "helpers.h"

vector<SecretPair> split(long long secret, int n, int k);

long long restore(int k, vector<SecretPair> secrets);


// config.h
#ifndef CONFIG_H
#define CONFIG_H

extern int nSharesTotal; // Declaration of nSharesTotal
extern int minShares;    // Declaration of minShares

#endif // CONFIG_H

