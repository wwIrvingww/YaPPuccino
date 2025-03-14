#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <regex>
#include <algorithm>
#include <string>
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

std::unordered_map<std::string, UserInfo> connectedUsers;
std::mutex clients_mutex;

// Función para validar el nombre de usuario (no puede estar vacío ni ser "~")
bool isValidUsername(const std::string& username) {
    return !username.empty() && username != "~";
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

// Envía un mensaje binario a un cliente específico
void sendBinaryMessage(std::shared_ptr<websocket::stream<tcp::socket>> ws, const std::vector<unsigned char>& message) {
    ws->binary(true);
    ws->write(asio::buffer(message));
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
                connectedUsers[username] = {username, ws, true};
            }
        }

        std::cout << "Usuario " << username << " conectado." << std::endl;

        // Enviar mensaje de bienvenida en modo texto
        ws->binary(false);
        ws->write(asio::buffer("¡Bienvenido YaPPuchino!"));

        // Notificar a los demás usuarios que se ha unido un nuevo usuario
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
                    std::cout << "Mensaje binario recibido de " << username << " con código: " << (int)pm.code << std::endl;

                    // Procesamiento según el código del mensaje
                    switch (pm.code) {
                        case MessageCode::SEND_MESSAGE:
                        {
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
                                        sendBinaryMessage(info.ws, binOut);
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
                            break;
                        }
                        // Aquí podrías agregar casos para LIST_USERS, GET_USER, CHANGE_STATUS, GET_HISTORY, etc.
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
        }
        broadcastTextMessage("Usuario " + username + " se ha desconectado.");

    } catch (const std::exception& e) {
        std::cerr << "Error con el cliente " << username << ": " << e.what() << std::endl;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            if (connectedUsers.count(username))
                connectedUsers[username].isActive = false;
        }
        broadcastTextMessage("Usuario " + username + " se ha desconectado.");
    }
}