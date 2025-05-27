#ifndef SQL_HANDLER_H
#define SQL_HANDLER_H

#include <string>
#include <vector>

class SQLHandler {
public:
    SQLHandler();
    ~SQLHandler();

    bool executeQuery(const std::string& query);
    std::vector<std::string> getResults() const;

private:
    // Add any private members needed for query execution and result storage
    std::vector<std::string> results;
};

#endif // SQL_HANDLER_H