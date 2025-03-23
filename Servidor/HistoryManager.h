#pragma once

#include <string>
#include <vector>
#include <utility>


/**
 * @brief Agrega un mensaje al historial, manteniendo solo los últimos 50.
 * 
 * @param user Nombre del usuario que envía el mensaje
 * @param msg  Texto del mensaje
 */
void appendToHistory(const std::string &user, const std::string &msg);

/**
 * @brief Carga los mensajes almacenados en el historial.
 * 
 * @return Vector de pares <username, mensaje>, en el orden en que se guardaron.
 */
std::vector<std::pair<std::string, std::string>> loadHistory();
