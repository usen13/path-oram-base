#ifndef SQL_HANDLER_H
#define SQL_HANDLER_H

#include <string>
#include <vector>
#include <variant>
#include "../Shamir_Parser/shamir_parser.h"
#include "utils.h" // Include for SelectItem/FilterItem and parseQueryJson

class SQLHandler {
public:
    SQLHandler();
    ~SQLHandler();

    std::vector<std::pair<int64_t, int64_t>> shamirSecretSharing(int64_t& secret, int n, int k);

    // Load query info from JSON file
    void loadQueryFromJson(const std::string& jsonFile);

    // Accessors for select/filter items
    const std::vector<Utils::SelectItem>& getSelectItems() const { return selectItems; }
    const std::vector<Utils::FilterItem>& getFilterItems() const { return filterItems; }
    const std::vector<std::string>& getAttributeSecrets() const { return attributeSecrets; }
    const std::vector<int64_t>& getConditionSecrets() const { return conditionSecrets; }
    const std::string& getWhereClauses() const { return whereClauses; }
    void setSelectItems(const std::vector<Utils::SelectItem>& items);
    void setFilterItems(const std::vector<Utils::FilterItem>& items);
    void setAttributeSecrets(const std::string& secrets);
    void setConditionSecrets(const int64_t& secrets);
    void setWhereClauses(const std::string& clauses);
    AtrributeID attributeStringToEnum(const std::string& attr) ;

private:

    std::vector<std::vector<int64_t>> results;
    std::vector<Utils::SelectItem> selectItems;
    std::vector<Utils::FilterItem> filterItems;
    std::vector<std::string> attributeSecrets;
    std::vector<int64_t> conditionSecrets;
    std::string whereClauses;
};

#endif // SQL_HANDLER_H