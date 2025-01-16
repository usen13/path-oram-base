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
    int64_t L_ORDERKEY; // Needed
    int64_t L_PARTKEY;  // Needed
    int64_t L_SUPPKEY;  // Needed
    int64_t L_LINENUMBER;   // Needed
    int64_t L_QUANTITY; // Needed
    double L_EXTENDEDPRICE; // Needed
    double L_DISCOUNT;  // Needed
    double L_TAX;   // Needed
    std::string L_RETURNFLAG;
    std::string L_LINESTATUS;
    std::string L_SHIPDATE;
    std::string L_COMMITDATE;
    std::string L_RECEIPTDATE;
    std::string L_SHIPINSTRUCT;
    std::string L_SHIPMODE;
    std::string L_COMMENT;
};

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
        void saveAllShares(const std::vector<std::vector<std::pair<int64_t, int64_t>>>& allShares, int tupleId);
        std::vector<LineItem> parseLineItemFile(const std::string& filename);

    private:
        // Function for maping unique values which takes a template argument
        template <typename T> std::unordered_map<T, int> mapUniqueValues(const std::vector<T>& values);
        
};