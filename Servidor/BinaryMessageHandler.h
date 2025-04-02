#ifndef BINARY_MESSAGE_HANDLER_H
#define BINARY_MESSAGE_HANDLER_H

#include <vector>
#include <cstdint>
#include <stdexcept>

// Función para decodificar una cadena URL
std::string urlDecode(const std::string &value);

// Estructura para representar un mensaje binario parseado
struct ParsedMessage {
    uint8_t code;
    // Cada campo es un vector de bytes
    std::vector<std::vector<unsigned char>> fields;
};

// Construye un mensaje binario a partir de un código y una lista de campos
std::vector<unsigned char> buildBinaryMessage(uint8_t code, const std::vector<std::vector<unsigned char>>& fields, bool omitFirstLength = false);

// Construye un mensaje binario sin longitud de campo
std::vector<unsigned char> buildRawBinaryMessage( uint8_t code, const std::vector<std::vector<unsigned char>>& fields);

// Parsea un buffer de mensaje binario y devuelve la estructura ParsedMessage
ParsedMessage parseBinaryMessage(const std::vector<unsigned char>& buffer);

// Códigos de mensaje según el protocolo
namespace MessageCode {
    // Mensajes enviados por el cliente al servidor
    const uint8_t LIST_USERS     = 0x01;
    const uint8_t GET_USER       = 0x02;
    const uint8_t CHANGE_STATUS  = 0x03;
    const uint8_t SEND_MESSAGE   = 0x04;
    const uint8_t GET_HISTORY    = 0x05;
    const uint8_t LIST_ALL_USERS = 0x06;

    // Respuestas y notificaciones del servidor
    const uint8_t ERROR_RESPONSE       = 0x50;
    const uint8_t RESPONSE_LIST_USERS  = 0x51;
    const uint8_t RESPONSE_GET_USER    = 0x52;
    const uint8_t USER_REGISTERED      = 0x53;
    const uint8_t USER_STATUS_CHANGED  = 0x54;
    const uint8_t MESSAGE_RECEIVED     = 0x55;
    const uint8_t RESPONSE_HISTORY     = 0x56;
    const uint8_t RESPONSE_ALL_USERS   = 0x57;
}

// Códigos de error definidos en el protocolo
namespace ErrorCode {
    const uint8_t USER_NOT_FOUND     = 0x01;
    const uint8_t INVALID_STATUS     = 0x02;
    const uint8_t EMPTY_MESSAGE      = 0x03;
    const uint8_t USER_DISCONNECTED  = 0x04;
}

#endif // BINARY_MESSAGE_HANDLER_H