#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <boost/asio.hpp>
#include <set>
#include <string>
#include <map>

using boost::asio::ip::tcp;

class WebsocketServer {

    public:
        WebsocketServer (int port): port_(port), acceptor_(io_context_, tcp::endpoint(tcp::v4(), port)) {}
        void start();

    private:
        boost::asio::io_context io_context_;
        tcp::acceptor acceptor_;
        std::map<std::string, tcp::socket> clients_;
        int port_;

        //Funcion para establacer la conexion
        void acceptConnection();

        //FUncion para finalizar la conexion
        void closeConnection(const std::string& client_id);

        //Funcion para leer mensajes
        void readMessages(const std::string& client_id);

        //FUncion para cambiar el estado a Desactivado
        void desactivateUser(const std::string& client_id);

        //Funcion para notificar al resto
        void notifyAllClients(const std::string& message);
};

#endif