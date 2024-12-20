#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <random>
#include <cmath>

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

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) {
        std::cout << "Usage: " << argv[0] << " <encrypt/decrypt> <attributeID> <file>" << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "1: Encrpypt" << std::endl;
        std::cout << "2: Reconstruct secret (Decrypt)" << std::endl;
        std::cout << "Possible attributeID options are: " << std::endl;
        std::cout << "1: orderkey" << std::endl;
        std::cout << "2: partkey" << std::endl;
        std::cout << "3: suppkey" << std::endl;
        std::cout << "4: linenumber" << std::endl;
        std::cout << "5: quantity" << std::endl;
        std::cout << "6: extendedprice" << std::endl;
        std::cout << "7: discount" << std::endl;
        std::cout << "8: tax" << std::endl;
        std::cout << "9: returnflag" << std::endl;
        std::cout << "10: linestatus" << std::endl;
        std::cout << "11: shipdate" << std::endl;
        std::cout << "12: commitdate" << std::endl;
        std::cout << "13: receiptdate" << std::endl;
        std::cout << "14: shipinstruct" << std::endl;
        std::cout << "15: shipmode" << std::endl;
        std::cout << "16: comment" << std::endl;
        std::cout << "17: all" << std::endl;

        return 1;
    }

    std::string option = argv[1];
    int attributeID = std::stoi(argv[2]);

    if (option == "encrypt" && argc == 4) {
        std::string filename = argv[3];
        auto lineItems = parseLineItemFile(filename);

        std::vector<std::string> orderkey, partkey, suppkey, linenumber, returnFlag, lineStatus, shipDate, commitDate, receiptDate, shipInstruct, shipMode, comment;
        for (const auto& item : lineItems) {
            orderkey.push_back(std::to_string(item.L_ORDERKEY));
            partkey.push_back(std::to_string(item.L_PARTKEY));
            suppkey.push_back(std::to_string(item.L_SUPPKEY));
            linenumber.push_back(std::to_string(item.L_LINENUMBER));
            returnFlag.push_back(item.L_RETURNFLAG);
            lineStatus.push_back(item.L_LINESTATUS);
            shipDate.push_back(item.L_SHIPDATE);
            commitDate.push_back(item.L_COMMITDATE);
            receiptDate.push_back(item.L_RECEIPTDATE);
            shipInstruct.push_back(item.L_SHIPINSTRUCT);
            shipMode.push_back(item.L_SHIPMODE);
            comment.push_back(item.L_COMMENT);
        }

        auto orderkeyMap = mapUniqueValues(orderkey); 
        auto partkeyMap = mapUniqueValues(partkey);
        auto suppkeyMap = mapUniqueValues(suppkey);
        auto linenumberMap = mapUniqueValues(linenumber);
        auto returnFlagMap = mapUniqueValues(returnFlag);
        auto lineStatusMap = mapUniqueValues(lineStatus);
        auto shipDateMap = mapUniqueValues(shipDate);
        auto commitDateMap = mapUniqueValues(commitDate);
        auto receiptDateMap = mapUniqueValues(receiptDate);
        auto shipInstructMap = mapUniqueValues(shipInstruct);
        auto shipModeMap = mapUniqueValues(shipMode);
        auto commentMap = mapUniqueValues(comment);

        switch (attributeID) {
            case orderKeyID:
                for (size_t i = 0; i < orderkey.size(); ++i) {
                    int secret = orderkeyMap[orderkey[i]];
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
            case partKeyID:
                for(size_t i = 0; i < partkey.size(); ++i) {
                    int secret = partkeyMap[partkey[i]];
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
            case suppKeyID:
                for(size_t i = 0; i < suppkey.size(); ++i) {
                    int secret = lineItems[i].L_SUPPKEY;
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
            case lineNumberID:
                for(size_t i = 0; i < lineItems.size(); ++i) {
                    int secret = lineItems[i].L_LINENUMBER;
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
            case quantityID:
                for(size_t i = 0; i < lineItems.size(); ++i) {
                    int secret = lineItems[i].L_QUANTITY;
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
            case extendedPriceID:
                for(size_t i = 0; i < lineItems.size(); ++i) {
                    int secret = lineItems[i].L_EXTENDEDPRICE;
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
            case discountID:
                for(size_t i = 0; i < lineItems.size(); ++i) {
                    int secret = lineItems[i].L_DISCOUNT;
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
            case taxID:
                for(size_t i = 0; i < lineItems.size(); ++i) {
                    int secret = lineItems[i].L_TAX;
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
            case returnFlagID:
                for(size_t i = 0; i < lineItems.size(); ++i) {
                    int secret = lineItems[i].L_RETURNFLAG[0];
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
            case lineStatusID:
                for(size_t i = 0; i < lineItems.size(); ++i) {
                    int secret = lineItems[i].L_LINESTATUS[0];
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
            case shipDateID:
                for(size_t i = 0; i < lineItems.size(); ++i) {
                    int secret = std::stoi(lineItems[i].L_SHIPDATE);
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
            case commitDateID:
                for(size_t i = 0; i < lineItems.size(); ++i) {
                    int secret = std::stoi(lineItems[i].L_COMMITDATE);
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
            case receiptDateID:
                for(size_t i = 0; i < lineItems.size(); ++i) {
                    int secret = std::stoi(lineItems[i].L_RECEIPTDATE);
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
            case shipInStructID:
                for(size_t i = 0; i < lineItems.size(); ++i) {
                    int secret = lineItems[i].L_SHIPINSTRUCT[0];
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
            case shipModeID:
                for(size_t i = 0; i < lineItems.size(); ++i) {
                    int secret = shipModeMap[lineItems[i].L_SHIPMODE];
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
            case commentID:
                for(size_t i = 0; i < lineItems.size(); ++i) {
                    int secret = lineItems[i].L_COMMENT[0];
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
            default:
                for (size_t i = 0; i < lineItems.size(); ++i) {
                    int secret = lineItems[i].L_ORDERKEY;
                    auto shares = shamirSecretSharing(secret, 6, 3);
                    saveShares(shares, i + 1);
                }
                break;
        }
    } else if (option == "decrypt" && argc == 4) {
        std::vector<std::pair<int, int>> shares;
        int attributeID = std::stoi(argv[2]);
        std::string outputFilename = argv[3];
        std::vector<int> reconstructedSecrets;

        for (size_t tupleIndex = 1; ; ++tupleIndex) {
            shares.clear();
            bool fileExists = true;
            for (int serverIndex = 1; serverIndex <= 3; ++serverIndex) {
                std::ifstream file("server_" + std::to_string(serverIndex) + "_tuple_" + std::to_string(tupleIndex) + ".txt");
                if (file.is_open()) {
                    int x, y;
                    if (file >> x >> y) {
                        shares.emplace_back(x, y);
                    } else {
                        std::cerr << "Error reading from file: server_" << serverIndex << "_tuple_" << tupleIndex << ".txt" << std::endl;
                        return 1;
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
            if (shares.size() < 3) {
                std::cerr << "Not enough shares to reconstruct the secret for tuple " << tupleIndex << "." << std::endl;
                return 1;
            }
            int secret = reconstructSecret(shares, 3);
            reconstructedSecrets.push_back(secret);
        }

        std::ofstream outputFile(outputFilename);
        if (outputFile.is_open()) {
            for (const auto& secret : reconstructedSecrets) {
                outputFile << secret << std::endl;
            }
            outputFile.close();
        } else {
            std::cerr << "Error opening output file: " << outputFilename << std::endl;
            return 1;
        }
        std::cout << "Reconstructed secrets written to: " << outputFilename << std::endl;
    } else {
        std::cout << "Invalid arguments." << std::endl;
        return 1;
    }

    return 0;
}