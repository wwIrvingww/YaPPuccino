
#include "BinaryMessageHandler.h"
#include <sstream>
#include <iomanip>

// Función para construir un mensaje binario:
// Se inserta primero el código (1 byte), luego para cada campo se agrega 1 byte con la longitud y finalmente los datos.
std::vector<unsigned char> buildBinaryMessage(uint8_t code, 
    const std::vector<std::vector<unsigned char>>& fields, bool omitFirstLength) {
    
    std::vector<unsigned char> message;
    message.push_back(code);
    
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i == 0 && omitFirstLength) {
            // Para el primer campo, omitir el byte de longitud
            message.insert(message.end(), fields[i].begin(), fields[i].end());
        } else {
            if (fields[i].size() > 255)
                throw std::runtime_error("El tamaño del campo excede 255 bytes.");
            message.push_back(static_cast<unsigned char>(fields[i].size()));
            message.insert(message.end(), fields[i].begin(), fields[i].end());
        }
    }
    return message;
}

// Función para parsear un mensaje binario:
// Lee el primer byte como código y luego recorre el buffer para extraer cada campo usando el byte de longitud.
ParsedMessage parseBinaryMessage(const std::vector<unsigned char>& buffer) {
    if (buffer.empty()) {
        throw std::runtime_error("Buffer vacío.");
    }
    ParsedMessage parsed;
    size_t pos = 0;
    parsed.code = buffer[pos++];
    while (pos < buffer.size()) {
        uint8_t len = buffer[pos++];
        if (pos + len > buffer.size()) {
            throw std::runtime_error("Longitud de campo inválida.");
        }
        std::vector<unsigned char> field(buffer.begin() + pos, buffer.begin() + pos + len);
        parsed.fields.push_back(field);
        pos += len;
    }
    return parsed;
}