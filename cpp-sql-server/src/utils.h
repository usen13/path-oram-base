#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <nlohmann/json.hpp> // Include nlohmann/json for JSON parsing

namespace Utils {

void log(const std::string& message);
std::string trim(const std::string& str);

// Structs for select/filter elements
struct SelectItem {
    std::string query_type;
    std::string attribute;
    std::string variable;
};

struct FilterItem {
    std::string attribute;
    std::string condition;
    std::string whereClause;
};

void parseQueryJson(
    const std::string& filename,
    std::vector<Utils::SelectItem>& selectItems,
    std::vector<Utils::FilterItem>& filterItems,
    std::pair<Utils::SelectItem, std::vector<Utils::FilterItem>>& allItems
);

} // namespace Utils

#endif // UTILS_H