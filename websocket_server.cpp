#include "websocket_server.h"

WebsocketServer::WebsocketServer(int port):
    port_(port), acceptor_(io_context_, tcp::endpoint(tcp::v4(), port)){
}

void WebsocketServer::start(){
    acceptConnection();
    io_context_.run();
}

void WebsocketServer::acceptConnection(){
    auto socket = std::make_shared<tcp::socket>(io_context_);
    acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& ec){
        if (!ec) {
            std::cout << "Conexion aceptada" << std::endl;
            readMessages (socket)
        }
    })
}

void WebsocketServer::readMessages(std::shared_ptr<tcp::socket> socket){

}

void WebsocketServer::closeConnection(const std::string& client_id){

}

void WebsocketServer::desactivateUser(const std::string& client_id){

}

void WebsocketServer::notifyAllClients(const std::string& message){
    
}