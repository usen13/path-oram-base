#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <memory>
#include <iostream>
#include <boost/asio.hpp>
#include "sql_handler.h"

class Server {
public:
    Server(unsigned short port);
    void start();
    void stop();

private:
    void handleClient(boost::asio::ip::tcp::socket socket);
    void processQuery(const std::string& query);

    unsigned short port_;
    bool running_;
    std::unique_ptr<SQLHandler> sqlHandler_;
    boost::asio::io_context ioContext_;
};

#endif // SERVER_H