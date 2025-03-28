#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iostream>

static const std::string HISTORY_FILE = "/home/ubuntu/YaPPuccino/Servidor/general.txt";

// Cada línea del archivo: "username|mensaje"
void appendToHistory(const std::string &user, const std::string &msg)
{
    std::cerr << "[DEBUG] appendToHistory: Iniciando actualización del historial para usuario: " << user << std::endl;
    
    // 1) Cargar todo el historial actual
    std::ifstream fin(HISTORY_FILE);
    if (!fin) {
        std::cerr << "[ERROR] appendToHistory: No se pudo abrir " << HISTORY_FILE << " para lectura." << std::endl;
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(fin, line))
    {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    fin.close();
    std::cerr << "[DEBUG] appendToHistory: Leídas " << lines.size() << " líneas." << std::endl;

    // 2) Si ya hay 50, borrar la más antigua (posición 0)
    if (lines.size() >= 50)
    {
        std::cerr << "[DEBUG] appendToHistory: Historial tiene " << lines.size() << " entradas, eliminando la más antigua." << std::endl;
        lines.erase(lines.begin());
    }

    // 3) Añadir la nueva línea al final
    {
        std::ostringstream oss;
        oss << user << "|" << msg;
        std::string newLine = oss.str();
        lines.push_back(newLine);
        std::cerr << "[DEBUG] appendToHistory: Añadida nueva línea: " << newLine << std::endl;
    }

    // 4) Reescribir el archivo con el contenido actualizado
    std::ofstream fout(HISTORY_FILE, std::ios::trunc);
    if (!fout) {
        std::cerr << "[ERROR] appendToHistory: No se pudo abrir " << HISTORY_FILE << " para escritura." << std::endl;
    }
    for (auto &l : lines)
    {
        fout << l << "\n";
    }
    fout.close();
    std::cerr << "[DEBUG] appendToHistory: Historial actualizado, ahora tiene " << lines.size() << " entradas." << std::endl;
}

// Carga todo el historial del archivo (hasta 50 líneas).
// Retorna un vector de pares <user, mensaje>.
std::vector<std::pair<std::string, std::string>> loadHistory()
{
    std::cerr << "[DEBUG] loadHistory: Cargando historial desde " << HISTORY_FILE << std::endl;
    std::vector<std::pair<std::string, std::string>> result;
    std::ifstream fin(HISTORY_FILE);
    if (!fin) {
        std::cerr << "[ERROR] loadHistory: No se pudo abrir " << HISTORY_FILE << " para lectura." << std::endl;
    }
    std::string line;
    while (std::getline(fin, line))
    {
        if (!line.empty())
        {
            auto pos = line.find('|');
            if (pos != std::string::npos)
            {
                std::string u = line.substr(0, pos);
                std::string m = line.substr(pos + 1);
                result.push_back({u, m});
            }
        }
    }
    fin.close();
    std::cerr << "[DEBUG] loadHistory: Cargado historial con " << result.size() << " entradas." << std::endl;
    return result;
}

std::string privateHistoryPath(const std::string &u1, const std::string &u2) {
    auto a = std::min(u1, u2), b = std::max(u1, u2);
    return "/home/ubuntu/YaPPuccino/Servidor/History/private/" + a + "_" + b + ".txt";
}

void appendPrivateHistory(const std::string &from, const std::string &to, const std::string &msg) {
    std::string path = privateHistoryPath(from, to);
    std::ofstream fout(path, std::ios::app);
    fout << from << "|" << msg << "\n";
}

std::vector<std::pair<std::string,std::string>> loadPrivateHistory(const std::string &u1, const std::string &u2) {
    std::string path = privateHistoryPath(u1, u2);
    std::ifstream fin(path);
    std::vector<std::pair<std::string,std::string>> result;
    std::string line;
    while(std::getline(fin,line)) {
        auto pos = line.find('|');
        result.emplace_back(line.substr(0,pos), line.substr(pos+1));
    }
    return result;
}