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

/**
 * @brief Genera la ruta del historial privado entre dos usuarios.
 *
 * @param u1 Primer usuario.
 * @param u2 Segundo usuario.
 * @return std::string Ruta del archivo de historial privado.
 */
std::string privateHistoryPath(const std::string &u1, const std::string &u2);

/**
 * @brief Añade un mensaje al historial privado entre dos usuarios.
 *
 * @param from Usuario que envía el mensaje.
 * @param to Usuario que recibe el mensaje.
 * @param msg Texto del mensaje.
 */
void appendPrivateHistory(const std::string &from, const std::string &to, const std::string &msg);

/**
 * @brief Carga el historial privado de mensajes entre dos usuarios.
 *
 * @param u1 Primer usuario.
 * @param u2 Segundo usuario.
 * @return std::vector<std::pair<std::string, std::string>> Vector de pares <usuario, mensaje>.
 */
std::vector<std::pair<std::string, std::string>> loadPrivateHistory(const std::string &u1, const std::string &u2);