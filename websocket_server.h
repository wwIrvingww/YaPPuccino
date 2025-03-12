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
        void closeConnection(const std::string& client_id){
            auto it = clients_.find(client_id);
            if (it != clients_.end()){
                boost::system::error_code ec;
                it->second.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                if (ec) {
                    std::cerr << "Error de cierre: " << ec.message() << std::endl;
                }
                it->second.close();
                clients_.erase(it);
                desactivateUser(client_id);
                notifyAllClients(client_id + "Ha abandonado el chat")
            }
        };


        //Funcion para leer mensajes
        void readMessages(const std::string& client_id);

        //FUncion para cambiar el estado a Desactivado
        void desactivateUser(const std::string& client_id){
            //aun no se en donde almacenamos el estado de los usuarios
        };

        //Funcion para notificar al resto
        void notifyAllClients(const std::string& message){
            for (auto& client : clients_){
                boost::asio::async_write(client.second, boost::asio::buffer(message),[](const boost::system::error_code& ec, std::size_t){
                    if (ec) {
                        std::cerr << "Error al enviar la notificacion: " << ec.message() << std::endl;
                    }
                }
            }
        };
};

#endif