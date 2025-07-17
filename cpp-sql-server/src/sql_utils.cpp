#include "sql_utils.h"
#include <ctime>

namespace Utils {

void log(const std::string& message) {
    std::ofstream logFile("server.log", std::ios_base::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
    } else {
        std::cerr << "Unable to open log file." << std::endl;
    }
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    size_t last = str.find_last_not_of(' ');
    return (first == std::string::npos || last == std::string::npos) ? "" : str.substr(first, last - first + 1);
}

// Function to parse JSON query file
void parseQueryJson(
    const std::string& filename,
    std::vector<Utils::SelectItem>& selectItems,
    std::vector<Utils::FilterItem>& filterItems,
    std::pair<Utils::SelectItem, std::vector<Utils::FilterItem>>& allItems
) {
    std::ifstream in(filename);
    if (!in.is_open()) {
        throw std::runtime_error("Cannot open JSON file: " + filename);
    }
    nlohmann::json j;
    in >> j;
    selectItems.clear();
    filterItems.clear();

    if (j.contains("select")) {
        for (const auto& s : j["select"]) {
            Utils::SelectItem item;
            item.query_type = s.value("query_type", "");
            item.attribute = s.value("attribute", "");
            item.variable = s.value("variable", "");
            selectItems.push_back(item);
        }
    }
    // Parse filters (including whereClause)
    if (j.contains("filters")) {
        for (const auto& f : j["filters"]) {
            Utils::FilterItem item;
            if (f.contains("whereClause")) {
                item.attribute = f.value("attribute", "");
                item.condition = f.value("condition", "");
                item.whereClause = f.value("whereClause", "");
            }
            else {
                item.attribute = f.value("attribute", "");
                item.condition = f.value("condition", "");
                item.whereClause = ""; // Default to empty if not present
            }
            
            filterItems.push_back(item);
        }
    }

    // Combine select and filter items into allItems
    for (const auto& selectItem : selectItems) {
        // Populate allItems with each select item and its associated filter items
        allItems = std::make_pair(selectItem, filterItems);
    }
    std::cout << "finishing the parsing of JSON file: " << filename << std::endl;
    // Print out the parsed items for debugging
    // for (const auto& item : allItems) {
    //     std::cout << "Select Item: " << item.first.query_type << ", "
    //               << item.first.attribute << ", " << item.first.variable << std::endl;
    // }

    } // namespace Utils
}