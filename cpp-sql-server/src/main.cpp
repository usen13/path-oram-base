#include <iostream>
#include "server.h"

int main() {
    Server sqlServer;

    // Start the server
    sqlServer.start();

    std::cout << "SQL Server is running. Waiting for queries..." << std::endl;

    // Keep the server running to accept incoming queries
    std::string query;
    while (true) {
        std::getline(std::cin, query);
        if (query == "exit") {
            break; // Exit the loop and stop the server
        }
        sqlServer.handleClient(query);
    }

    // Stop the server
    sqlServer.stop();
    std::cout << "SQL Server has stopped." << std::endl;

    return 0;
}