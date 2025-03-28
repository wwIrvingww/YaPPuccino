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
#include <fstream>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include <chrono> 
#include "BinaryMessageHandler.h"
#include "HistoryManager.h"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace http = beast::http;
using tcp = asio::ip::tcp;

// Estructura para mapear un estado a su valor numerico
enum class UserStatus : uint8_t
{
    DISCONNECTED = 0,
    ACTIVE = 1,
    BUSY = 2,
    INACTIVE = 3
};

static std::string userStatusToString(UserStatus s)
{
    switch(s)
    {
        case UserStatus::DISCONNECTED: return "DESACTIVADO";
        case UserStatus::ACTIVE:       return "ACTIVO";
        case UserStatus::BUSY:         return "OCUPADO";
        case UserStatus::INACTIVE:     return "INACTIVO";
    }
    return "DESCONOCIDO";
}

// Estructura para almacenar la información de cada usuario
struct UserInfo
{
    std::string username;
    std::shared_ptr<websocket::stream<tcp::socket>> ws;
    UserStatus status;
    std::string ipAddress;

    std::chrono::steady_clock::time_point lastActivityTime; // Última vez en que un usuario mandó un mensaje 

    UserStatus previousState; // Estado anterior del usuario
};

std::vector<UserInfo> snapshot;

std::string bytesToHexString(const std::vector<unsigned char> &data)
{
    std::ostringstream oss;
    for (const auto &byte : data)
    {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
    }
    return oss.str();
}

std::unordered_map<std::string, UserInfo> connectedUsers;
std::mutex clients_mutex;

// Función para validar el nombre de usuario (no puede estar vacío ni ser "~")
bool isValidUsername(const std::string &username)
{
    return !username.empty() && username != "~";
}

// Envía un mensaje binario a un cliente específico
void sendBinaryMessage(std::shared_ptr<websocket::stream<tcp::socket>> ws, const std::vector<unsigned char> &message)
{
    ws->binary(true);
    try {
        ws->write(asio::buffer(message));
        std::cerr << "[DEBUG] Texto sendBinaryMessage " << bytesToHexString(message) << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] Texto sendBinaryMessage " <<  bytesToHexString(message) << ": " << e.what() << std::endl;
    }}

// Extrae el parámetro "name" de la URL de la request
std::string extractUsername(const std::string &target)
{
    std::regex name_regex("name=([^& ]+)");
    std::smatch match;
    if (std::regex_search(target, match, name_regex))
    {
        return urlDecode(match[1]);
    }
    return "";
}

std::string extractUserIpAddress(const tcp::socket &socket)
{
    return socket.remote_endpoint().address().to_string();
}

// Envía un mensaje de texto a todos los clientes conectados
void broadcastTextMessage(const std::string &message)
{
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto &[user, info] : connectedUsers)
    {

        std::string decodedUser = urlDecode(user);

        if ((info.status == UserStatus::ACTIVE || info.status == UserStatus::BUSY)
            && info.ws                                    // <-- no sea nullptr
            && info.ws->next_layer().is_open()           // <-- esté abierto
            && !message.empty())
        {
            try {
                info.ws->binary(false);
                info.ws->write(asio::buffer(message));
            } catch(const std::exception &e) {
                std::cerr << "[ERROR] broadcastTextMessage to " << user << ": " << e.what() << std::endl;
            }
        }
    }
}

// Notificar a todos los clientes con ID 53
void broadcastUserJoined(const std::string &username, const std::string &ipAddress)
{
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for(auto &p : connectedUsers)
            snapshot.push_back(p.second);
    }
    for (auto &[user, info] : connectedUsers)
    {
        // Solo se notifica si el usuario está en estado ACTIVE o BUSY.
        if ((info.status == UserStatus::ACTIVE || info.status == UserStatus::BUSY) && !username.empty())
        {
            auto binMsg = buildBinaryMessage(MessageCode::USER_REGISTERED, {std::vector<unsigned char>(username.begin(), username.end()),
                                                                            std::vector<unsigned char>(ipAddress.begin(), ipAddress.end())});
            sendBinaryMessage(info.ws, binMsg); // Envía el mensaje binario
        }
    }
}

// Notificar a todos los clientes con ID 54 cuando un usuario se desconecta
void broadcastUserDisconnected(const std::string &username)
{
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for(auto &p : connectedUsers)
            snapshot.push_back(p.second);
    }
    for (auto &[user, info] : connectedUsers)
    {
        // Notificar solo a los usuarios con estado ACTIVE o BUSY.
        if ((info.status == UserStatus::ACTIVE || info.status == UserStatus::BUSY) && !username.empty())
        {
            auto binMsg = buildBinaryMessage(MessageCode::USER_STATUS_CHANGED, {std::vector<unsigned char>(username.begin(), username.end())});
            sendBinaryMessage(info.ws, binMsg); // Envía el mensaje binario
        }
    }
}

// Notificar a todos los clientes con ID 54 cuando un usuario cambia de estado
void broadcastUserStatusChanged(const std::string &username, UserStatus newStatus)
{
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for(auto &p : connectedUsers)
            snapshot.push_back(p.second);
    }
    
    std::vector<unsigned char> binMsg;
    binMsg.push_back(MessageCode::USER_STATUS_CHANGED); // 0x36
    binMsg.push_back(static_cast<unsigned char>(username.size())); // Len username
    binMsg.insert(binMsg.end(), username.begin(), username.end()); // Username
    binMsg.push_back(static_cast<unsigned char>(newStatus)); // Status (sin longitud)
    
    // Solo se notifica a usuarios en ACTIVE o BUSY
    for (auto &[user, info] : connectedUsers)
    {
        if (info.ws && info.ws->next_layer().is_open())
        {
            try {
                sendBinaryMessage(info.ws, binMsg);
            } catch(const std::exception &e) {
                std::cerr << "[ERROR] broadcastUserStatusChanged to " << user << ": " << e.what() << std::endl;
            }
        }
    }
}

// Cambiar el estado de un usuario y notificar a los demás
void setUserStatus(const std::string &username, UserStatus newStatus, bool forceNotify = false)
{
    if (!connectedUsers.count(username)) return;

    auto &info = connectedUsers[username];
    
    if (info.status == newStatus && !forceNotify)
    {
        // Si no hay cambio, no hacemos nada
        return;
    }

    // Actualizamos el estado anterior solo si el nuevo no es DISCONNECTED
    // (así recordamos el último estado "real" para la reconexión)
    if (newStatus != UserStatus::DISCONNECTED)
    {
        info.previousState = newStatus;
    }

    info.status = newStatus;

    // 1) Notificar por binario (ID 54)
    broadcastUserStatusChanged(username, newStatus);

    // 2) Notificar por texto: "Usuario X se ha cambiado a estado Y"
    //DEBUG LOG: std::cerr << "[DEBUG] setUserStatus START: " << username << " from " << userStatusToString(info.status) << " to " << userStatusToString(newStatus) << std::endl;
    // info.status = newStatus;
    //DEBUG LOG: std::cerr << "[DEBUG] setUserStatus END: " << username << " now " << userStatusToString(info.status) << std::endl; 

    broadcastTextMessage("Usuario " + username + " se ha cambiado a estado " + userStatusToString(newStatus));
}

void markUserDisconnected(const std::string &username)
{
    setUserStatus(username, UserStatus::DISCONNECTED, true);
}

// Manejo de la conexión de un cliente mediante WebSockets
void handleClient(std::shared_ptr<websocket::stream<tcp::socket>> ws, std::string username)
{
    try
    {
        {
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                for(auto &p : connectedUsers)
                    snapshot.push_back(p.second);
            }            
            auto it = connectedUsers.find(username);
            if (it != connectedUsers.end())
            {
                // El usuario ya existía
                auto &info = it->second;

                // Si estaba DISCONNECTED, volvemos al estado anterior
                if (info.status == UserStatus::DISCONNECTED)
                {
                    // Regresamos al estado que tenía antes de desconectarse
                    // (Si era INACTIVE, se vuelve ACTIVE según tu regla)
                    if (info.previousState == UserStatus::BUSY)
                    {
                        setUserStatus(username, UserStatus::BUSY, true);
                    }
                    else if (info.previousState == UserStatus::ACTIVE, true)
                    {
                        setUserStatus(username, UserStatus::ACTIVE, true);
                    }
                    else if (info.previousState == UserStatus::INACTIVE, true)
                    {
                        setUserStatus(username, UserStatus::ACTIVE, true);
                    }
                    else
                    {
                        // Por defecto, si no teníamos nada, lo ponemos en ACTIVE
                        setUserStatus(username, UserStatus::ACTIVE, true);
                    }
                }
                // Si no estaba DISCONNECTED, no forzamos nada. (Si estaba BUSY, se queda BUSY,
                // si estaba ACTIVE, se queda ACTIVE, etc.)

                // Actualizamos el socket y la hora de actividad
                info.ws = ws;
                info.lastActivityTime = std::chrono::steady_clock::now();
            }
            else
            {
                // Usuario nuevo, lo creamos por primera vez
                UserInfo newUser {
                    username,
                    ws,
                    UserStatus::ACTIVE, // estado inicial
                    extractUserIpAddress(ws->next_layer()),
                    std::chrono::steady_clock::now(),
                    UserStatus::ACTIVE // previousState inicial
                };
                connectedUsers[username] = newUser;

                // Notificamos su estado inicial (ACTIVO)
                setUserStatus(username, UserStatus::ACTIVE, true);
                
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
        while (true)
        {
            buffer.consume(buffer.size());
            try {
                ws->read(buffer);
            } catch(const std::exception &e) {
                std::cerr << "[ERROR] read for " << username << ": " << e.what() << std::endl;
                break;
            }

            {
                // Usamos un lock temporal solo para obtener la información necesaria
                bool needReactivation = false;
                {
                    {
                        std::lock_guard<std::mutex> lock(clients_mutex);
                        for(auto &p : connectedUsers)
                            snapshot.push_back(p.second);
                    }
                    auto &info = connectedUsers[username];
                    info.lastActivityTime = std::chrono::steady_clock::now();

                    if (info.status == UserStatus::INACTIVE && ws->got_text())
                    {
                        std::string msg = beast::buffers_to_string(buffer.data());
                        if (!msg.empty() && !std::all_of(msg.begin(), msg.end(), ::isspace)) {
                            needReactivation = true;
                        }
                    }
                } // Aquí se libera el mutex

                if (needReactivation)
                {
                    std::cout << "Reactivando usuario " << username << std::endl;
                    // Reactivamos al usuario; setUserStatus internamente adquiere el mutex
                    setUserStatus(username, UserStatus::ACTIVE, true);

                    // Ahora, fuera del mutex, enviar un mensaje directo al cliente para confirmar la reactivación
                    ws->binary(false);
                    std::string reactivationMsg = "Se ha reactivado el estado de " + username + " a ACTIVO.";
                    try {
                        ws->write(asio::buffer(reactivationMsg));
                        //DEBUG LOG: std::cerr << "[DEBUG] Enviado texto a " << reactivationMsg << std::endl;
                    } catch (const std::exception &e) {
                        //DEBUG LOG: std::cerr << "[ERROR] send to " << reactivationMsg << ": " << e.what() << std::endl;
                    }                
                }
            }

            // Diferenciar si el mensaje recibido es de texto o binario
            if (ws->got_text())
            {
                std::string msg = beast::buffers_to_string(buffer.data());
                // Evitar procesar mensajes vacíos o compuestos únicamente de espacios
                if (msg.empty() || std::all_of(msg.begin(), msg.end(), ::isspace))
                {
                    auto errMsg = buildRawBinaryMessage(MessageCode::ERROR_RESPONSE, {{ErrorCode::EMPTY_MESSAGE}});
                    sendBinaryMessage(ws, errMsg);
                    continue;
                }
                if (msg == "/exit")
                {
                    std::cout << "Usuario " << username << " ha solicitado desconexión." << std::endl;
                    websocket::close_reason cr;
                    cr.code = websocket::close_code::normal;
                    cr.reason = "El usuario solicitó desconexión voluntaria";
                    ws->close(cr);
                    ws->next_layer().close();
                    break;
                }
                std::cout << "Mensaje de texto recibido de " << username << ": " << msg << std::endl;
                appendToHistory(username, msg);
                broadcastTextMessage(username + ": " + msg);
            }
            else
            {
                // Procesamiento de mensaje binario
                auto data = buffer.data();

                std::vector<unsigned char> binMsg(
                    static_cast<const unsigned char *>(data.data()),
                    static_cast<const unsigned char *>(data.data()) + data.size());

                try
                {
                    ParsedMessage pm = parseBinaryMessage(binMsg);

                    // Procesamiento según el código del mensaje
                    switch (pm.code)
                    {
                    case MessageCode::SEND_MESSAGE:
                    {

                        auto binMsg = buildBinaryMessage(MessageCode::SEND_MESSAGE, {pm.fields[0], pm.fields[1]});
                        // sendBinaryMessage(ws, binMsg);

                        // Se esperan dos campos: destinatario y contenido del mensaje
                        if (pm.fields.size() < 2)
                        {
                            auto errMsg = buildRawBinaryMessage(MessageCode::ERROR_RESPONSE, {{ErrorCode::EMPTY_MESSAGE}});
                            sendBinaryMessage(ws, errMsg);
                            break;
                        }
                        std::string dest(pm.fields[0].begin(), pm.fields[0].end());
                        std::string message(pm.fields[1].begin(), pm.fields[1].end());

                        appendToHistory(username, message);

                        std::string mensajeTexto = username + ": " + message;

                        if (message.empty())
                        {
                            auto errMsg = buildRawBinaryMessage(MessageCode::ERROR_RESPONSE, {{ErrorCode::EMPTY_MESSAGE}});
                            sendBinaryMessage(ws, errMsg);
                            break;
                        }

                        bool needReactivate = false;
                        {
                            std::lock_guard<std::mutex> lock(clients_mutex);
                            auto &info = connectedUsers[username];
                            if (info.status == UserStatus::INACTIVE) {
                                needReactivate = true;
                            }
                        }
                        
                        if (needReactivate) {
                            std::cout << "Reactivando usuario " << username << " por mensaje SEND_MESSAGE" << std::endl;
                            setUserStatus(username, UserStatus::ACTIVE, true);
                        }

                        // Si el mensaje es para el chat general, el destino es "~"
                        if (dest == "~")
                        {
                            {
                                std::lock_guard<std::mutex> lock(clients_mutex);
                                for(auto &p : connectedUsers)
                                    snapshot.push_back(p.second);
                            }
                            for (auto &[user, info] : connectedUsers)
                            {
                                // Enviar el mensaje solo a usuarios que estén en estado ACTIVE o BUSY.
                                if (info.status == UserStatus::ACTIVE || info.status == UserStatus::BUSY)
                                {
                                    auto binOut = buildBinaryMessage(MessageCode::MESSAGE_RECEIVED, {std::vector<unsigned char>(username.begin(), username.end()),
                                                                                                     std::vector<unsigned char>(message.begin(), message.end())});
                                    sendBinaryMessage(info.ws, binOut);
                                }
                            }
                        }

                        else
                        {
                            // Mensaje privado
                            {
                                std::lock_guard<std::mutex> lock(clients_mutex);
                                for(auto &p : connectedUsers)
                                    snapshot.push_back(p.second);
                            }

                            appendPrivateHistory(username, dest, message);

                            // Solo envia mensajes a los activos y ocupadsos
                            if (connectedUsers.count(dest) && connectedUsers[dest].status == UserStatus::ACTIVE || connectedUsers[dest].status == UserStatus::BUSY || connectedUsers[dest].status == UserStatus::INACTIVE)
                            {
                                auto binOut = buildBinaryMessage(MessageCode::MESSAGE_RECEIVED, {std::vector<unsigned char>(username.begin(), username.end()),
                                                                                                 std::vector<unsigned char>(message.begin(), message.end())});
                                sendBinaryMessage(connectedUsers[dest].ws, binOut);
                                sendBinaryMessage(ws, binOut);
                            }
                            else
                            {
                                auto errMsg = buildRawBinaryMessage(MessageCode::ERROR_RESPONSE, {{static_cast<unsigned char>(ErrorCode::USER_DISCONNECTED)}});
                                sendBinaryMessage(ws, errMsg);
                                std::cerr << "[INFO] Usuario " << username << " intentó enviar mensaje a usuario desconectado: " << dest << std::endl;
                            }
                        }


                        if (dest == "~"){
                            std::cout << "→ Mensaje de " << username << " enviado al chat general: " << message << std::endl;    
                            broadcastTextMessage(mensajeTexto);
                        } else {
                            std::cout << "→ Mensaje de " << username << " enviado a " << dest << ": " << message << std::endl;

                        }

                        break;
                    }
                    // Aquí  agregar casos para LIST_USERS, GET_USER, CHANGE_STATUS, GET_HISTORY, etc.
                    // List User: retorna el listado de usuarios y sus estados
                    case MessageCode::LIST_USERS:
                    {
                        std::lock_guard<std::mutex> lock(clients_mutex);

                        std::vector<unsigned char> resp;
                        resp.push_back(MessageCode::RESPONSE_LIST_USERS); // Código 0x33

                        size_t count = 0;
                        for (const auto &[_, info] : connectedUsers) {
                            if (info.status != UserStatus::DISCONNECTED)
                                ++count;
                        }
                        resp.push_back(static_cast<unsigned char>(count));

                        for (const auto &[_, info] : connectedUsers) {
                            if (info.status == UserStatus::DISCONNECTED)
                                continue;

                            resp.push_back(static_cast<unsigned char>(info.username.size()));
                            resp.insert(resp.end(), info.username.begin(), info.username.end());

                            resp.push_back(static_cast<unsigned char>(info.status)); // casteo a byte
                        }

                        sendBinaryMessage(ws, resp);
                        std::cout << "→ Enviado listado de " << count << " usuarios a " << username << "\n";
                        break;
                    }

                    case MessageCode::LIST_ALL_USERS:
                    {
                        std::lock_guard<std::mutex> lock(clients_mutex);

                        std::vector<unsigned char> resp;
                        resp.push_back(MessageCode::RESPONSE_ALL_USERS); // o RESPONSE_LIST_ALL_USERS si querés diferenciar

                        size_t count = connectedUsers.size();
                        resp.push_back(static_cast<unsigned char>(count));

                        for (const auto &[_, info] : connectedUsers) {
                            resp.push_back(static_cast<unsigned char>(info.username.size()));
                            resp.insert(resp.end(), info.username.begin(), info.username.end());
                            resp.push_back(static_cast<unsigned char>(info.status));
                        }

                        sendBinaryMessage(ws, resp);
                        std::cout << "→ Enviado listado completo de " << count << " usuarios a " << username << "\n";
                        break;
                    }

                    case MessageCode::GET_USER:
                    {
                        std::string target(pm.fields[0].begin(), pm.fields[0].end());
                        std::lock_guard<std::mutex> lock(clients_mutex);
                        auto it = connectedUsers.find(target);

                        if (it == connectedUsers.end() || it->second.status == UserStatus::DISCONNECTED) {
                            std::vector<unsigned char> errResp;
                            errResp.push_back(MessageCode::ERROR_RESPONSE);       
                            errResp.push_back(ErrorCode::USER_NOT_FOUND);         
                            sendBinaryMessage(ws, errResp);
                        } else {
                            std::vector<unsigned char> resp;
                            resp.push_back(MessageCode::RESPONSE_GET_USER);          // TIPO
                            resp.push_back(static_cast<unsigned char>(target.size())); // LEN_USER
                            resp.insert(resp.end(), target.begin(), target.end());   // USERNAME
                            resp.push_back(static_cast<unsigned char>(it->second.status)); // STATUS 

                            sendBinaryMessage(ws, resp);
                            std::cout << "→ GET_USER: enviado info de " << target << std::endl;
                        }
                        break;
                    }

                    case MessageCode::GET_HISTORY:
                    {

                        std::string target(pm.fields[0].begin(), pm.fields[0].end());

                        std::vector<std::pair<std::string, std::string>> history;

                        if (target == "~"){
                            history = loadHistory();
                        } else {
                            // Sólo permite historial si quien pide es parte de la conversación
                            if (username != target && !connectedUsers.count(username)) {
                                // Usuario no existe o no es parte → error
                                std::vector<unsigned char> err = { MessageCode::ERROR_RESPONSE, ErrorCode::USER_NOT_FOUND };
                                sendBinaryMessage(ws, err);
                                break;
                            }
                            std::string path = privateHistoryPath(username, target);
                            std::ifstream fin(path);
                            if (!fin.good()) {
                                std::vector<unsigned char> err = { MessageCode::ERROR_RESPONSE, ErrorCode::USER_NOT_FOUND };
                                sendBinaryMessage(ws, err);
                                break;
                            }
                            history = loadPrivateHistory(username, target);
                        }



                        std::vector<std::vector<unsigned char>> fields;


                        fields.push_back({ static_cast<unsigned char>(history.size()) });

                        for (auto &hm : history)
                        {

                            fields.push_back(
                                std::vector<unsigned char>(hm.first.begin(), hm.first.end())
                            );
                            fields.push_back(
                                std::vector<unsigned char>(hm.second.begin(), hm.second.end())
                            );
                        }

                        auto responseMsg = buildBinaryMessage(MessageCode::RESPONSE_HISTORY, fields, true);

                        sendBinaryMessage(ws, responseMsg);

                        std::cout << "→ Historial de " << history.size() 
                                << " mensajes enviado a " << username
                                << " target=" << target << ")" << std::endl;

                        break;
                    }

                    case MessageCode::CHANGE_STATUS:
                    {
                        std::cerr << "[DEBUG] CHANGE_STATUS fields.size(): " << pm.fields.size()
                        << " field[0].size(): " << pm.fields[0].size()
                        << " field[1].size(): " << pm.fields[1].size() << std::endl;

                        if (pm.fields.size() < 2 || pm.fields[0].empty() || pm.fields[1].empty()) {
                            auto errMsg = buildRawBinaryMessage(MessageCode::ERROR_RESPONSE, {{ErrorCode::EMPTY_MESSAGE}});
                            sendBinaryMessage(ws, errMsg);
                            break;
                        }

                        std::string targetUser(pm.fields[0].begin(), pm.fields[0].end());
                        uint8_t rawStatus = pm.fields[1][0];

                        // Validar status: solo 1 (ACTIVO), 2 (OCUPADO) o 3 (INACTIVO)
                        if (rawStatus < 1 || rawStatus > 3) {
                            auto errMsg = buildRawBinaryMessage(MessageCode::ERROR_RESPONSE, {{ErrorCode::INVALID_STATUS}});
                            sendBinaryMessage(ws, errMsg);
                            std::cerr << "[ERROR] Usuario " << targetUser << " envió estado inválido: " << (int)rawStatus << std::endl;
                            break;
                        }

                        UserStatus newStatus = static_cast<UserStatus>(rawStatus);

                        if (!connectedUsers.count(targetUser)) {
                            auto errMsg = buildRawBinaryMessage(MessageCode::ERROR_RESPONSE, {{ErrorCode::USER_NOT_FOUND}});
                            sendBinaryMessage(ws, errMsg);
                            break;
                        }

                        setUserStatus(targetUser, newStatus, true);
                        break;
                    }

                    default:
                    {
                        std::cout << "Código de mensaje binario no reconocido: " << (int)pm.code << std::endl;
                        auto errMsg = buildRawBinaryMessage(MessageCode::ERROR_RESPONSE, {{ErrorCode::EMPTY_MESSAGE}});
                        sendBinaryMessage(ws, errMsg);
                        break;
                    }
                    }
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error al procesar mensaje binario de " << username << ": " << e.what() << std::endl;
                    auto errMsg = buildRawBinaryMessage(MessageCode::ERROR_RESPONSE, {{ErrorCode::EMPTY_MESSAGE}});
                    sendBinaryMessage(ws, errMsg);
                }
            }
        }

        {
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                for(auto &p : connectedUsers)
                    snapshot.push_back(p.second);
            }
            if (connectedUsers.count(username))
            {
                setUserStatus(username, UserStatus::DISCONNECTED, true);
                connectedUsers[username].ws.reset();
            }
        }
        broadcastTextMessage("Usuario " + username + " se ha desconectado.");
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error con el cliente " << username << ": " << e.what() << std::endl;
        {
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                for(auto &p : connectedUsers)
                    snapshot.push_back(p.second);
            }
            if (connectedUsers.count(username))
            {
                setUserStatus(username, UserStatus::DISCONNECTED, true);
                connectedUsers[username].ws.reset();
            }
        }
        broadcastTextMessage("Usuario " + username + " se ha desconectado.");
    }
}

int main()
{
    try
    {
        asio::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 5000));

        std::cout << "Servidor WebSockets en ws://localhost:5000" << std::endl;

        const int INACTIVITY_THRESHOLD = 25;

        std::thread([&]{
            while (true)
            {
                std::this_thread::sleep_for(std::chrono::seconds(5)); // cada 5 seg
                std::vector<std::string> usersToSetInactive;  // lista de usuarios a actualizar
        
                {
                    {
                        std::lock_guard<std::mutex> lock(clients_mutex);
                        for(auto &p : connectedUsers)
                            snapshot.push_back(p.second);
                    }
                    auto now = std::chrono::steady_clock::now();
        
                    for (auto &[u, info] : connectedUsers)
                    {
                        // Solo los que están en ACTIVE o BUSY se vuelven INACTIVE tras X seg
                        if (info.status == UserStatus::ACTIVE)
                        {
                            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                                now - info.lastActivityTime
                            ).count();
        
                            if (elapsed >= INACTIVITY_THRESHOLD)
                            {
                                usersToSetInactive.push_back(u);
                            }
                        }
                    }
                }
        
                // Fuera del lock, se actualiza el estado para cada usuario identificado.
                for (const auto &username : usersToSetInactive)
                {
                    setUserStatus(username, UserStatus::INACTIVE, true);
                    std::cout << "Usuario " << username << " pasó a INACTIVE por inactividad.\n";
                }
            }
        }).detach();

        while (true)
        {
            auto socket = std::make_shared<tcp::socket>(io_context);
            acceptor.accept(*socket);

            // Leer la request HTTP para obtener la información del handshake
            beast::flat_buffer buffer;
            http::request<http::string_body> req;
            http::read(*socket, buffer, req);
            std::string target = req.target().to_string();
            std::string username = extractUsername(target);

            auto connHdr = req[http::field::connection].to_string();
            auto upgHdr  = req[http::field::upgrade].to_string();
            if (upgHdr.empty() || connHdr.find("Upgrade") == std::string::npos) {
                http::response<http::string_body> res{http::status::ok, req.version()};
                if (connectedUsers.count(username) && connectedUsers[username].status != UserStatus::DISCONNECTED) {
                    res.result(http::status::bad_request);
                    res.body() = "Usuario ya conectado";
                }
                res.prepare_payload();
                http::write(*socket, res);
                continue;
            }


            // Validar el nombre de usuario
            if (!isValidUsername(username) ||
            (connectedUsers.count(username) && connectedUsers[username].status != UserStatus::DISCONNECTED)) 
            {
                http::response<http::string_body> res{http::status::bad_request, req.version()};
                res.body() = "Usuario ya conectado";
                res.prepare_payload();
                http::write(*socket, res);
                continue;
            }

            {
                {
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    for(auto &p : connectedUsers)
                        snapshot.push_back(p.second);
                }
                if (connectedUsers.count(username))
                {
                    auto &info = connectedUsers[username];
                    // Si el usuario NO está en estado DISCONNECTED y el socket existe y está abierto, se rechaza la conexión.
                    if (info.status != UserStatus::DISCONNECTED && info.ws && info.ws->next_layer().is_open())
                    {
                        http::response<http::string_body> res{http::status::bad_request, req.version()};
                        res.set(http::field::content_type, "text/plain");
                        res.body() = "Usuario ya conectado";
                        res.prepare_payload();
                        http::write(*socket, res);
                        continue;
                    }
                }
            }                 

            // Crear un hilo para manejar la conexión del cliente
            std::thread([socket, req, username]() mutable
            {
                try 
                {
                    auto ws = std::make_shared<websocket::stream<tcp::socket>>(std::move(*socket));
                    ws->accept(req);
                    handleClient(ws, username);
                } 
                catch (const std::exception& e) 
                {
                    std::cerr << "Error en el hilo para " << username << ": " << e.what() << std::endl;
                } 
            }).detach();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error en el servidor: " << e.what() << std::endl;

    }
    return 0;
}