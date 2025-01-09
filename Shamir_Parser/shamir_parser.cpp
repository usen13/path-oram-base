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
    int64_t L_ORDERKEY;
    int64_t L_PARTKEY;
    int64_t L_SUPPKEY;
    int64_t L_LINENUMBER;
    int64_t L_QUANTITY;
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

// Use an extremely large prime number which fits in int64_t for the modulus to avoid overflow
const int64_t MODULUS_HUGE =  9999999967;


int64_t modInverse(int64_t a, int64_t m) {
    int64_t m0 = m, t, q;
    int64_t x0 = 0, x1 = 1;

    if (m == 1)
        return 0;

    while (a > 1) {
        q = a / m;
        t = m;
        m = a % m, a = t;
        t = x0;
        x0 = x1 - q * x0;
        x1 = t;
    }

    if (x1 < 0)
        x1 += m0;

    return x1;
}

std::vector<std::pair<int64_t, int64_t>> shamirSecretSharingDouble(double secret, int n, int k) {
    std::vector<int64_t> coefficients(k);
    std::vector<std::pair<int64_t, int64_t>> shares;

    // Generate random coefficients
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int64_t> dis(0, 100);

    coefficients[0] = static_cast<int64_t>(secret * 100); // Scale the floating point value
    for (int i = 1; i < k; ++i) {
        coefficients[i] = dis(gen);
    }

    // Generate shares
    for (int64_t x = 1; x <= n; ++x) {
        int64_t y = 0;
        for (int i = 0; i < k; ++i) {
            y += coefficients[i] * std::pow(x, i);;
        }
        shares.emplace_back(x,y);
    }

    return shares;
}

std::vector<std::pair<int64_t, int64_t>> shamirSecretSharingString(int64_t secret, int n, int k) {
    std::vector<int64_t> coefficients(k);
    std::vector<std::pair<int64_t, int64_t>> shares;

    // Generate random coefficients
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int64_t> dis(1, MODULUS_HUGE - 1);

    coefficients[0] = secret;
    for (int i = 1; i < k; ++i) {
        coefficients[i] = dis(gen);
    }

    // Generate shares
    for (int64_t x = 1; x <= n; ++x) {
        int64_t y = 0;
        for (int i = 0; i < k; ++i) {
            y = (y + coefficients[i] * static_cast<int64_t>(std::pow(x, i))) % MODULUS_HUGE;
        }
        shares.emplace_back(x, (y + MODULUS_HUGE) % MODULUS_HUGE); // Ensure non-negative values
    }

    return shares;
}

std::vector<std::pair<int64_t, int64_t>> shamirSecretSharing(int64_t secret, int n, int k) {
    std::vector<int64_t> coefficients(k - 1);
    std::vector<std::pair<int64_t, int64_t>> shares;

    // Generate random coefficients
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);

    coefficients[0] = secret;
    for (int i = 1; i < k; ++i) {
        coefficients[i] = dis(gen); // So in the case, for k = 3 we have 2 coefficients, at 0 index we have the secret, at 1 index we have the first coefficient
    }

    // Generate shares
    for (int64_t x = 1; x <= n; ++x) {
        int64_t y = 0;
        for (int i = 0; i < k; ++i) {
            y += coefficients[i] * std::pow(x, i); // For the first share, we have x = 1, so we have y = coefficients[0] + coefficients[1] + coefficients[2]
                                                   // where coefficients[0] is the secret, coefficients[1] is the random integer generated by std::uniform_int_distribution<> dis(1, INT32_MAX)
        }
        shares.emplace_back(x, y);
    }

    return shares;
}

double reconstructSecretFloat(const std::vector<std::pair<int64_t, int64_t>>& shares, int k) {
    double secret = 0;

    for (int i = 0; i < k; ++i) {
        double lagrange_coeff = 1.0;

        for (int j = 0; j < k; ++j) {
            if (i != j) {
                lagrange_coeff *= static_cast<double>(-shares[j].first) / (shares[i].first - shares[j].first);
            }
        }

        secret = secret + shares[i].second * lagrange_coeff;
    }

    return static_cast<double>((secret ) / 100.0); // Scale back to floating point
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
int64_t reconstructSecret(const std::vector<std::pair<int64_t, int64_t>>& shares, int k) {
    double secret = 0;
    // Size of double is 64 bits, so it can store 15 decimal digits
    // Size of int64_t is 64 bits, so it can store 18 decimal digits

    for (int i = 0; i < k; ++i) {
        double lagrange_coeff = 1.0;

        for (int j = 0; j < k; ++j) {
            if (i != j) {
                lagrange_coeff *= static_cast<double>(-shares[j].first) / (shares[i].first - shares[j].first);
            }
        }

        secret += shares[i].second * lagrange_coeff;
    }

    return static_cast<int64_t>(std::round(secret));
}

// Function to convert date string to Unix timestamp
int64_t dateToTimestamp(const std::string& date) {
    std::tm tm = {};
    std::istringstream ss(date);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    return std::mktime(&tm);
}

// Convert Unix timestamp to date string
std::string timestampToDate(int64_t timestamp) {
    std::time_t time = timestamp;
    std::tm* tm = std::localtime(&time);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%d");
    return ss.str();
}

// Convert string to vector of integers (each character separately)
std::vector<int64_t> stringToIntVector(const std::string& str) {
    std::vector<int64_t> result;
    for (char c : str) {
        result.push_back(static_cast<int64_t>(c));
    }
    return result;
}

// Convert vector of integers to string
std::string intVectorToString(const std::vector<int64_t>& vec) {
    std::string result;
    for (int64_t val : vec) {
        result += static_cast<char>(val);
    }
    return result;
}

// Function to convert string to integer (simple example using ASCII values)
int64_t stringToInt(const std::string& str) {
    int64_t result = 0;
    for (char c : str) {
        result = result * 256 + static_cast<int>(c);
    }
    return result;
    // Max value of int64_t is 2^63 - 1 = 9223372036854775807
    // value of TRUCK =               1381319499
}

// Convert integer to string
std::string intToString(int64_t value) {
    std::string result;
    while (value > 0) {
        result = static_cast<char>(value % 256) + result;
        value /= 256;
    }
    return result;
}

std::vector<std::vector<std::pair<int64_t, int64_t>>> shamirSecretSharingAllAttributes(const LineItem& item, int n, int k) {
    std::vector<std::vector<std::pair<int64_t, int64_t>>> allShares(16);
    // Max value of int64_t is 2^63 - 1 = 9223372036854775807

    auto shareAttributeInt = [&](int64_t secret) {
        return shamirSecretSharing(secret, n, k);
    };

    auto shareAttributeDouble = [&](double secret) {
        return shamirSecretSharingDouble(secret, n, k);
    };

    auto shareAttribute = [&](int64_t secret) {
        return shamirSecretSharingString(secret, n, k);
    };

    allShares[0] = shareAttributeInt(item.L_ORDERKEY);
    allShares[1] = shareAttributeInt(item.L_PARTKEY);
    allShares[2] = shareAttributeInt(item.L_SUPPKEY);
    allShares[3] = shareAttributeInt(item.L_LINENUMBER);
    allShares[4] = shareAttributeInt(item.L_QUANTITY);
    allShares[5] = shareAttributeInt(item.L_EXTENDEDPRICE);
    allShares[6] = shareAttributeDouble(item.L_DISCOUNT);
    allShares[7] = shareAttributeDouble(item.L_TAX);
    allShares[8] = shareAttributeInt(item.L_RETURNFLAG[0]);
    allShares[9] = shareAttributeInt(item.L_LINESTATUS[0]);
    allShares[10] = shareAttributeInt(dateToTimestamp(item.L_SHIPDATE));
    allShares[11] = shareAttributeInt(dateToTimestamp(item.L_COMMITDATE));
    allShares[12] = shareAttributeInt(dateToTimestamp(item.L_RECEIPTDATE));
    allShares[13] = shareAttributeInt(stringToInt(item.L_SHIPINSTRUCT));
     allShares[14] = shareAttributeInt(stringToInt(item.L_SHIPMODE));
     allShares[15] = shareAttributeInt(stringToInt(item.L_COMMENT));

        // Convert each character of the string separately
    // auto shipInstructVec = stringToIntVector(item.L_SHIPINSTRUCT);
    // for (int64_t val : shipInstructVec) {
    //     allShares[13].push_back(shareAttribute(val));
    // }

    // auto shipModeVec = stringToIntVector(item.L_SHIPMODE);
    // for (int64_t val : shipModeVec) {
    //     allShares[14].push_back(shareAttribute(val));
    // }

    // auto commentVec = stringToIntVector(item.L_COMMENT);
    // for (int64_t val : commentVec) {
    //     allShares[15].push_back(shareAttribute(val));
    // }

    return allShares;
}

void saveAllShares(const std::vector<std::vector<std::pair<int64_t, int64_t>>>& allShares, int tupleId) {
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
            std::vector<std::vector<std::pair<int64_t, int64_t>>> allShares(16);
            bool fileExists = true;

            for (int serverIndex = 1; serverIndex <= 6; ++serverIndex) {
                std::ifstream file("server_" + std::to_string(serverIndex) + "_tuple_" + std::to_string(tupleIndex) + ".txt");
                if (file.is_open()) {
                    for (auto& shares : allShares) {
                        int64_t x, y;
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
            item.L_DISCOUNT = reconstructSecretFloat(allShares[6], 3);
            item.L_TAX = reconstructSecretFloat(allShares[7], 3);
            item.L_RETURNFLAG = static_cast<char>(reconstructSecret(allShares[8], 3));
            item.L_LINESTATUS = static_cast<char>(reconstructSecret(allShares[9], 3));
            item.L_SHIPDATE = timestampToDate(reconstructSecret(allShares[10], 3));
            item.L_COMMITDATE = timestampToDate(reconstructSecret(allShares[11], 3));
            item.L_RECEIPTDATE = timestampToDate(reconstructSecret(allShares[12], 3));
            item.L_SHIPINSTRUCT = intToString(reconstructSecret(allShares[13], 3));
            item.L_SHIPMODE = intToString(reconstructSecret(allShares[14], 3));
            item.L_COMMENT = intToString(reconstructSecret(allShares[15], 3));

            // Reconstruct each character of the string separately
            // std::vector<int64_t> shipInstructVec;
            // for (const auto& share : allShares[13]) {
            //     shipInstructVec.push_back(reconstructSecret({share}, 1));
            // }
            // item.L_SHIPINSTRUCT = intVectorToString(shipInstructVec);

            // std::vector<int64_t> shipModeVec;
            // for (const auto& share : allShares[14]) {
            //     shipModeVec.push_back(reconstructSecret({share}, 1));
            // }
            // item.L_SHIPMODE = intVectorToString(shipModeVec);

            // std::vector<int64_t> commentVec;
            // for (const auto& share : allShares[15]) {
            //     commentVec.push_back(reconstructSecret({share}, 1));
            // }
            // item.L_COMMENT = intVectorToString(commentVec);

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