#include "server.h"
#include "sql_handler.h"
#include <iostream>
#include <string>
#include <thread>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

Server::Server(unsigned short port) : acceptor_(io_service_, tcp::endpoint(tcp::v4(), port)) {}

void Server::start() {
    std::cout << "Server started, waiting for connections..." << std::endl;
    acceptConnections();
    io_service_.run();
}

void Server::stop() {
    io_service_.stop();
    std::cout << "Server stopped." << std::endl;
}

void Server::acceptConnections() {
    auto socket = std::make_shared<tcp::socket>(io_service_);
    acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& error) {
        if (!error) {
            std::cout << "Client connected: " << socket->remote_endpoint() << std::endl;
            handleClient(socket);
        }
        acceptConnections();
    });
}

void Server::handleClient(std::shared_ptr<tcp::socket> socket) {
    std::thread([this, socket]() {
        try {
            char data[1024];
            boost::system::error_code error;

            while (true) {
                std::size_t length = socket->read_some(boost::asio::buffer(data), error);
                if (error == boost::asio::error::eof) {
                    break; // Connection closed cleanly by peer.
                } else if (error) {
                    throw boost::system::system_error(error); // Some other error.
                }

                std::string query(data, length);
                SQLHandler sqlHandler;
                sqlHandler.executeQuery(query);
                std::string results = sqlHandler.getResults();
                boost::asio::write(*socket, boost::asio::buffer(results), error);
            }
        } catch (std::exception& e) {
            std::cerr << "Exception in client handler: " << e.what() << std::endl;
        }
    }).detach();
}