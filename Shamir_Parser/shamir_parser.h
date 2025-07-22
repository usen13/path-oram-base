#ifndef SHAMIR_PARSER_H
#define SHAMIR_PARSER_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <random>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <utility>

struct LineItem {
    int64_t L_ORDERKEY; // Needed size of order key is 8 bytes
    int64_t L_PARTKEY;  // Needed size of part key is 8 bytes
    int64_t L_SUPPKEY;  // Needed size of supp key is 8 bytes
    int64_t L_LINENUMBER;   // Needed size of line number is 8 bytes
    int64_t L_QUANTITY; // Needed size of quantity is 8 bytes
    double L_EXTENDEDPRICE; // Needed size of extended price is 8 bytes
    double L_DISCOUNT;  // Needed size of discount is 8 bytes
    double L_TAX;   // Needed size of tax is 8 bytes
    std::string L_RETURNFLAG;
    std::string L_LINESTATUS;
    std::string L_SHIPDATE;
    std::string L_COMMITDATE;
    std::string L_RECEIPTDATE;
    std::string L_SHIPINSTRUCT;
    std::string L_SHIPMODE;
    std::string L_COMMENT;
}; // total size of the struct is 13 * 8 + 4 * 4 = 116 bytes

enum AtrributeID {
    orderKeyID = 1,
    partKeyID = 2,
    suppKeyID = 3,
    lineNumberID = 4,
    quantityID = 5,
    extendedPriceID = 6,
    discountID = 7,
    taxID = 8,
    returnFlagID = 9,
    lineStatusID = 10,
    shipDateID = 11,
    commitDateID = 12,
    receiptDateID = 13,
    shipInStructID = 14,
    shipModeID = 15,
    commentID = 16,
    allID = 17
};

class ShamirParser {
    public:
        
        std::vector<std::vector<std::pair<int64_t, int64_t>>> shamirSecretSharingAllAttributes(const LineItem& item, int n, int k);
        std::vector<std::pair<int64_t, int64_t>> shamirSecretSharingDouble(double& secret, int n, int k);
        std::vector<std::pair<int64_t, int64_t>> shamirSecretSharing(int64_t& secret, int n, int k);
        int64_t reconstructSecret(const std::vector<std::pair<int64_t, int64_t>>& shares, int k);
        double reconstructSecretFloat(const std::vector<std::pair<int64_t, int64_t>>& shares, int k);
        std::string reconstructSecretString(const std::vector<std::pair<int64_t, int64_t>>& shares, int k);
        std::string intToString(int64_t value);
        int64_t stringToInt(const std::string& str);
        std::string timestampToDate(int64_t timestamp);
        int64_t dateToTimestamp(const std::string& date);
        void saveAllShares(const std::vector<std::vector<std::pair<int64_t, int64_t>>>& allShares);
        std::vector<std::vector<std::vector<int64_t>>> loadAllShares(int n);
        std::vector<LineItem> parseLineItemFile(const std::string& filename);
        std::vector<std::vector<std::vector<std::pair<int64_t, int64_t>>>> transformShares(const std::vector<std::vector<std::vector<int64_t>>>& allShares);

    private:
        // Function for maping unique values which takes a template argument
        template <typename T> std::unordered_map<T, int> mapUniqueValues(const std::vector<T>& values);
        
};

#endif // SHAMIR_PARSER_H