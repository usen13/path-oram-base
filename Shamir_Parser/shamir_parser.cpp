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

struct LineItem {
    int L_ORDERKEY;
    int L_PARTKEY;
    int L_SUPPKEY;
    int L_LINENUMBER;
    int L_QUANTITY;
    double L_EXTENDEDPRICE;
    double L_DISCOUNT;
    double L_TAX;
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

std::vector<LineItem> parseLineItemFile(const std::string& filename) {
    std::vector<LineItem> lineItems;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;
        LineItem item;

        std::getline(iss, token, '|'); item.L_ORDERKEY = std::stoi(token);
        std::getline(iss, token, '|'); item.L_PARTKEY = std::stoi(token);
        std::getline(iss, token, '|'); item.L_SUPPKEY = std::stoi(token);
        std::getline(iss, token, '|'); item.L_LINENUMBER = std::stoi(token);
        std::getline(iss, token, '|'); item.L_QUANTITY = std::stoi(token);
        std::getline(iss, token, '|'); item.L_EXTENDEDPRICE = std::stod(token);
        std::getline(iss, token, '|'); item.L_DISCOUNT = std::stod(token);
        std::getline(iss, token, '|'); item.L_TAX = std::stod(token);
        std::getline(iss, token, '|'); item.L_RETURNFLAG = token;
        std::getline(iss, token, '|'); item.L_LINESTATUS = token;
        std::getline(iss, token, '|'); item.L_SHIPDATE = token;
        std::getline(iss, token, '|'); item.L_COMMITDATE = token;
        std::getline(iss, token, '|'); item.L_RECEIPTDATE = token;
        std::getline(iss, token, '|'); item.L_SHIPINSTRUCT = token;
        std::getline(iss, token, '|'); item.L_SHIPMODE = token;
        std::getline(iss, token, '|'); item.L_COMMENT = token;

        lineItems.push_back(item);
    }

    return lineItems;
}

template <typename T>
std::unordered_map<T, int> mapUniqueValues(const std::vector<T>& values) {
    std::unordered_map<T, int> value_map;
    int counter = 1;

    for (const auto& value : values) {
        if (value_map.find(value) == value_map.end()) {
            value_map[value] = counter++;
        }
    }

    return value_map;
}

std::vector<std::pair<int, int>> shamirSecretSharing(int secret, int n, int k) {
    std::vector<int> coefficients(k - 1);
    std::vector<std::pair<int, int>> shares;

    // Generate random coefficients
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);

    coefficients[0] = secret;
    for (int i = 1; i < k; ++i) {
        coefficients[i] = dis(gen);
    }

    // Generate shares
    for (int x = 1; x <= n; ++x) {
        int y = 0;
        for (int i = 0; i < k; ++i) {
            y += coefficients[i] * std::pow(x, i);
        }
        shares.emplace_back(x, y);
    }

    return shares;
}

void saveShares(const std::vector<std::pair<int, int>>& shares, int tupleId) {
    for (size_t i = 0; i < shares.size(); ++i) {
        std::ofstream file("server_" + std::to_string(i + 1) + "_tuple_" + std::to_string(tupleId) + ".txt");
        if (file.is_open()) {
            file << shares[i].first << " " << shares[i].second << "\n";
            file.close();
        }
    }
}

// Reconstruction (using Lagrange interpolation)
int reconstructSecret(const std::vector<std::pair<int, int>>& shares, int k) {
    int secret = 0;

    for (size_t i = 0; i < k; ++i) {
        double lagrange_coeff = 1.0;

        for (size_t j = 0; j < k; ++j) {
            if (i != j) {
                lagrange_coeff *= static_cast<double>(-shares[j].first) / (shares[i].first - shares[j].first);
            }
        }

        secret += shares[i].second * lagrange_coeff;
    }

    return static_cast<int>(std::round(secret));
}

// Function to convert date string to Unix timestamp
int dateToTimestamp(const std::string& date) {
    std::tm tm = {};
    std::istringstream ss(date);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    return std::mktime(&tm);
}

// Convert Unix timestamp to date string
std::string timestampToDate(int timestamp) {
    std::time_t time = timestamp;
    std::tm* tm = std::localtime(&time);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%d");
    return ss.str();
}

// Function to convert string to integer (simple example using ASCII values)
int stringToInt(const std::string& str) {
    int result = 0;
    for (char c : str) {
        result = result * 256 + static_cast<int>(c);
    }
    return result;
}

// Convert integer to string
std::string intToString(int value) {
    std::string result;
    uint64_t modulus = 997;
    while (value > 0) {
        result = static_cast<char>(value % 256) + result;
        value /= 256;
    }
    return result;
}

std::vector<std::vector<std::pair<int, int>>> shamirSecretSharingAllAttributes(const LineItem& item, int n, int k) {
    std::vector<std::vector<std::pair<int, int>>> allShares(16);

    auto shareAttribute = [&](int secret) {
        return shamirSecretSharing(secret, n, k);
    };

    allShares[0] = shareAttribute(item.L_ORDERKEY);
    allShares[1] = shareAttribute(item.L_PARTKEY);
    allShares[2] = shareAttribute(item.L_SUPPKEY);
    allShares[3] = shareAttribute(item.L_LINENUMBER);
    allShares[4] = shareAttribute(item.L_QUANTITY);
    allShares[5] = shareAttribute(static_cast<int>(item.L_EXTENDEDPRICE));
    allShares[6] = shareAttribute(static_cast<int>(item.L_DISCOUNT));
    allShares[7] = shareAttribute(static_cast<int>(item.L_TAX));
    allShares[8] = shareAttribute(item.L_RETURNFLAG[0]);
    allShares[9] = shareAttribute(item.L_LINESTATUS[0]);
    allShares[10] = shareAttribute(dateToTimestamp(item.L_SHIPDATE));
    allShares[11] = shareAttribute(dateToTimestamp(item.L_COMMITDATE));
    allShares[12] = shareAttribute(dateToTimestamp(item.L_RECEIPTDATE));
    allShares[13] = shareAttribute(stringToInt(item.L_SHIPINSTRUCT));
    allShares[14] = shareAttribute(stringToInt(item.L_SHIPMODE));
    allShares[15] = shareAttribute(stringToInt(item.L_COMMENT));

    return allShares;
}

void saveAllShares(const std::vector<std::vector<std::pair<int, int>>>& allShares, int tupleId) {
    for (size_t i = 0; i < allShares[0].size(); ++i) {
        std::ofstream file("server_" + std::to_string(i + 1) + "_tuple_" + std::to_string(tupleId) + ".txt");
        if (file.is_open()) {
            for (const auto& shares : allShares) {
                file << shares[i].first << " " << shares[i].second << " ";
            }
            file << "\n";
            file.close();
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <encrypt/decrypt> <file>" << std::endl;
        return 1;
    }

    std::string option = argv[1];
    std::string filename = argv[2];

    if (option == "encrypt") {
        auto lineItems = parseLineItemFile(filename);

        for (size_t i = 0; i < lineItems.size(); ++i) {
            auto allShares = shamirSecretSharingAllAttributes(lineItems[i], 6, 3);
            saveAllShares(allShares, i + 1);
        }
    } else if (option == "decrypt") {
        std::vector<LineItem> reconstructedItems;

        for (size_t tupleIndex = 1; ; ++tupleIndex) {
            std::vector<std::vector<std::pair<int, int>>> allShares(16);
            bool fileExists = true;

            for (int serverIndex = 1; serverIndex <= 6; ++serverIndex) {
                std::ifstream file("server_" + std::to_string(serverIndex) + "_tuple_" + std::to_string(tupleIndex) + ".txt");
                if (file.is_open()) {
                    for (auto& shares : allShares) {
                        int x, y;
                        if (file >> x >> y) {
                            shares.emplace_back(x, y);
                        } else {
                            std::cerr << "Error reading from file: server_" << serverIndex << "_tuple_" << tupleIndex << ".txt" << std::endl;
                            return 1;
                        }
                    }
                    file.close();
                } else {
                    fileExists = false;
                    break;
                }
            }
            if (!fileExists) {
                break;
            }

            LineItem item;
            item.L_ORDERKEY = reconstructSecret(allShares[0], 3);
            item.L_PARTKEY = reconstructSecret(allShares[1], 3);
            item.L_SUPPKEY = reconstructSecret(allShares[2], 3);
            item.L_LINENUMBER = reconstructSecret(allShares[3], 3);
            item.L_QUANTITY = reconstructSecret(allShares[4], 3);
            item.L_EXTENDEDPRICE = reconstructSecret(allShares[5], 3);
            item.L_DISCOUNT = reconstructSecret(allShares[6], 3);
            item.L_TAX = reconstructSecret(allShares[7], 3);
            item.L_RETURNFLAG = static_cast<char>(reconstructSecret(allShares[8], 3));
            item.L_LINESTATUS = static_cast<char>(reconstructSecret(allShares[9], 3));
            item.L_SHIPDATE = timestampToDate(reconstructSecret(allShares[10], 3));
            item.L_COMMITDATE = timestampToDate(reconstructSecret(allShares[11], 3));
            item.L_RECEIPTDATE = timestampToDate(reconstructSecret(allShares[12], 3));
            item.L_SHIPINSTRUCT = intToString(reconstructSecret(allShares[13], 3));
            item.L_SHIPMODE = intToString(reconstructSecret(allShares[14], 3));
            item.L_COMMENT = intToString(reconstructSecret(allShares[15], 3));

            reconstructedItems.push_back(item);
        }

        std::ofstream outputFile(filename);
        if (outputFile.is_open()) {
            for (const auto& item : reconstructedItems) {
                outputFile << item.L_ORDERKEY << "|" << item.L_PARTKEY << "|" << item.L_SUPPKEY << "|" << item.L_LINENUMBER << "|"
                           << item.L_QUANTITY << "|" << item.L_EXTENDEDPRICE << "|" << item.L_DISCOUNT << "|" << item.L_TAX << "|"
                           << item.L_RETURNFLAG << "|" << item.L_LINESTATUS << "|" << item.L_SHIPDATE << "|" << item.L_COMMITDATE << "|"
                           << item.L_RECEIPTDATE << "|" << item.L_SHIPINSTRUCT << "|" << item.L_SHIPMODE << "|" << item.L_COMMENT << "\n";
            }
            outputFile.close();
        } else {
            std::cerr << "Error opening output file: " << filename << std::endl;
            return 1;
        }
        std::cout << "Reconstructed tuples written to: " << filename << std::endl;
    } else {
        std::cout << "Invalid option." << std::endl;
        return 1;
    }

    return 0;
}