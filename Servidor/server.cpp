#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <regex>
#include <algorithm>
#include <string>
#include <iomanip>
#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include "BinaryMessageHandler.h"

namespace asio    = boost::asio;
namespace beast   = boost::beast;
namespace websocket = beast::websocket;
namespace http    = beast::http;
using tcp = asio::ip::tcp;

// Estructura para almacenar la información de cada usuario
struct UserInfo {
    std::string username;
    std::shared_ptr<websocket::stream<tcp::socket>> ws;
    bool isActive;
};

std::string bytesToHexString(const std::vector<unsigned char>& data) {
    std::ostringstream oss;
    for (const auto& byte : data) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
    }
    return oss.str();
}

std::unordered_map<std::string, UserInfo> connectedUsers;
std::mutex clients_mutex;

// Función para validar el nombre de usuario (no puede estar vacío ni ser "~")
bool isValidUsername(const std::string& username) {
    return !username.empty() && username != "~";
}

// Envía un mensaje binario a un cliente específico
void sendBinaryMessage(std::shared_ptr<websocket::stream<tcp::socket>> ws, const std::vector<unsigned char>& message) {
    ws->binary(true);
    ws->write(asio::buffer(message));
}

// Extrae el parámetro "name" de la URL de la request
std::string extractUsername(const std::string& target) {
    std::regex name_regex("name=([^& ]+)");
    std::smatch match;
    if (std::regex_search(target, match, name_regex)) {
        return match[1];
    }
    return "";
}

// Envía un mensaje de texto a todos los clientes conectados
void broadcastTextMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto& [user, info] : connectedUsers) {
        if (info.isActive && !message.empty()) {
            info.ws->binary(false); // Modo texto
            info.ws->write(asio::buffer(message));
        }
    }
}


// Manejo de la conexión de un cliente mediante WebSockets
void handleClient(std::shared_ptr<websocket::stream<tcp::socket>> ws, std::string username) {
    try {
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            // Si el usuario ya existía, se reactiva; si no, se registra como nuevo.
            if (connectedUsers.find(username) != connectedUsers.end()) {
                connectedUsers[username].isActive = true;
            } else {
                connectedUsers[username] = {username, ws, true, extractUserIpAddress(ws->next_layer())};
            }
        }

        std::cout << "Usuario " << username << " conectado." << std::endl;

        // Enviar mensaje de bienvenida en modo texto
        ws->binary(false);
        ws->write(asio::buffer("¡Bienvenido a YaPPuchino!"));

        // Notificar a los demás usuarios que se ha unido un nuevo usuario
        broadcastUserJoined(username, connectedUsers[username].ipAddress);
        broadcastTextMessage("Usuario " + username + " se ha unido.");

        beast::flat_buffer buffer;
        while (true) {
            buffer.consume(buffer.size());
            ws->read(buffer);

            // Diferenciar si el mensaje recibido es de texto o binario
            if (ws->got_text()) {
                std::string msg = beast::buffers_to_string(buffer.data());
                // Evitar procesar mensajes vacíos o compuestos únicamente de espacios
                if (msg.empty() || std::all_of(msg.begin(), msg.end(), ::isspace)) {
                    continue;
                }
                if (msg == "/exit") {
                    std::cout << "Usuario " << username << " ha solicitado desconexión." << std::endl;
                    websocket::close_reason cr;
                    cr.code = websocket::close_code::normal;
                    cr.reason = "El usuario solicitó desconexión voluntaria";
                    ws->close(cr);
                    break;
                }
                std::cout << "Mensaje de texto recibido de " << username << ": " << msg << std::endl;
                broadcastTextMessage(username + ": " + msg);
            } else {
                // Procesamiento de mensaje binario
                auto data = buffer.data();

                std::vector<unsigned char> binMsg(
                    static_cast<const unsigned char*>(data.data()),
                    static_cast<const unsigned char*>(data.data()) + data.size()
                );

             
                try {
                    ParsedMessage pm = parseBinaryMessage(binMsg);

                    // Procesamiento según el código del mensaje
                    switch (pm.code) {
                        case MessageCode::SEND_MESSAGE:
                        {

                            auto binMsg = buildBinaryMessage(MessageCode::SEND_MESSAGE, { pm.fields[0], pm.fields[1] });
                            //sendBinaryMessage(ws, binMsg);

                            // Se esperan dos campos: destinatario y contenido del mensaje
                            if (pm.fields.size() < 2) {
                                auto errMsg = buildBinaryMessage(MessageCode::ERROR_RESPONSE, { {ErrorCode::EMPTY_MESSAGE} });
                                sendBinaryMessage(ws, errMsg);
                                break;
                            }
                            std::string dest(pm.fields[0].begin(), pm.fields[0].end());
                            std::string message(pm.fields[1].begin(), pm.fields[1].end());
                            if (message.empty()) {
                                auto errMsg = buildBinaryMessage(MessageCode::ERROR_RESPONSE, { {ErrorCode::EMPTY_MESSAGE} });
                                sendBinaryMessage(ws, errMsg);
                                break;
                            }
                            // Si el mensaje es para el chat general, el destino es "~"
                            if (dest == "~") {
                                std::lock_guard<std::mutex> lock(clients_mutex);
                                for (auto& [user, info] : connectedUsers) {
                                    if (info.isActive) {
                                        auto binOut = buildBinaryMessage(MessageCode::MESSAGE_RECEIVED, {
                                            std::vector<unsigned char>(username.begin(), username.end()),
                                            std::vector<unsigned char>(message.begin(), message.end())
                                        });


                                        //sendBinaryMessage(info.ws, binOut);
                                    }
                                }
                            } else {
                                // Mensaje privado
                                std::lock_guard<std::mutex> lock(clients_mutex);
                                if (connectedUsers.count(dest) && connectedUsers[dest].isActive) {
                                    auto binOut = buildBinaryMessage(MessageCode::MESSAGE_RECEIVED, {
                                        std::vector<unsigned char>(username.begin(), username.end()),
                                        std::vector<unsigned char>(message.begin(), message.end())
                                    });
                                    sendBinaryMessage(connectedUsers[dest].ws, binOut);
                                    // También se envía una copia al remitente
                                    sendBinaryMessage(ws, binOut);
                                } else {
                                    auto errMsg = buildBinaryMessage(MessageCode::ERROR_RESPONSE, { {ErrorCode::USER_DISCONNECTED} });
                                    sendBinaryMessage(ws, errMsg);
                                }
                            }

                            std::string mensajeTexto = username + ": " + message;
                            std::cout << "Mensaje parseado a texto: " << mensajeTexto << std::endl;

                            broadcastTextMessage(mensajeTexto);


                            break;
                        }
                        // Aquí  agregar casos para LIST_USERS, GET_USER, CHANGE_STATUS, GET_HISTORY, etc.
                        default:
                        {
                            std::cout << "Código de mensaje binario no reconocido: " << (int)pm.code << std::endl;
                            auto errMsg = buildBinaryMessage(MessageCode::ERROR_RESPONSE, { {ErrorCode::EMPTY_MESSAGE} });
                            sendBinaryMessage(ws, errMsg);
                            break;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error al procesar mensaje binario de " << username << ": " << e.what() << std::endl;
                    auto errMsg = buildBinaryMessage(MessageCode::ERROR_RESPONSE, { {ErrorCode::EMPTY_MESSAGE} });
                    sendBinaryMessage(ws, errMsg);
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            if (connectedUsers.count(username))
                connectedUsers[username].isActive = false;
            connectedUsers.erase(username);

        }

        broadcastTextMessage("Usuario " + username + " se ha desconectado.");

    } catch (const std::exception& e) {
        std::cerr << "Error con el cliente " << username << ": " << e.what() << std::endl;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            if (connectedUsers.count(username))
                connectedUsers[username].isActive = false;
            connectedUsers.erase(username);


        }
        broadcastTextMessage("Usuario " + username + " se ha desconectado.");
    }
}

int main() {
    try {
        asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 5000));

        std::cout << "Servidor WebSockets en ws://localhost:5000" << std::endl;

        while (true) {
            auto socket = std::make_shared<tcp::socket>(io_context);
            acceptor.accept(*socket);

            // Leer la request HTTP para obtener la información del handshake
            beast::flat_buffer buffer;
            http::request<http::string_body> req;
            http::read(*socket, buffer, req);

            std::string username = extractUsername(req.target().to_string());

            // Validar el nombre de usuario
            if (!isValidUsername(username)) {
                http::response<http::string_body> res{ http::status::bad_request, req.version() };
                res.set(http::field::content_type, "text/plain");
                res.body() = "Nombre de usuario inválido";
                res.prepare_payload();
                http::write(*socket, res);
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                if (connectedUsers.count(username) && connectedUsers[username].isActive) {
                    http::response<http::string_body> res{ http::status::bad_request, req.version() };
                    res.set(http::field::content_type, "text/plain");
                    res.body() = "Usuario ya conectado";
                    res.prepare_payload();
                    http::write(*socket, res);
                    continue;
                }
            }

            // Crear un hilo para manejar la conexión del cliente
            std::thread([socket, req, username]() mutable {
                try {
                    auto ws = std::make_shared<websocket::stream<tcp::socket>>(std::move(*socket));
                    ws->accept(req);
                    handleClient(ws, username);
                } catch (const std::exception& e) {
                    std::cerr << "Error en el hilo para " << username << ": " << e.what() << std::endl;
                }
            }).detach();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error en el servidor: " << e.what() << std::endl;
    }
    return 0;
}
