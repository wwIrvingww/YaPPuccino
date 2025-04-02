
# YaPPuccino ☕ - Chat en C++ con WebSockets
Este proyecto implementa un **servidor WebSocket** y un **cliente con interfaz gráfica en Qt**, utilizando un **protocolo binario personalizado** para la comunicación. Permite enviar mensajes generales y privados, gestionar el estado de los usuarios y guardar historial de conversaciones de forma persistente.

---

## ✨ Características

- Comunicación en tiempo real mediante WebSockets (Boost.Beast + Asio)
- Cliente con interfaz gráfica (Qt)
- Protocolo binario personalizado para comandos y mensajes
- Gestión de estados de usuario (ACTIVO, OCUPADO, INACTIVO, DESCONECTADO)
- Soporte para mensajes generales y privados
- Persistencia de historial (hasta 50 mensajes)
- Manejo de concurrencia con `std::thread` y `std::mutex`

---

## 🛠️ Requisitos

### Servidor (Linux o Windows)
- C++17
- Boost (>= 1.82): Asio y Beast
- CMake

### Cliente (Qt)
- Qt >= 6.0 (Qt WebSockets, Qt Widgets)
- CMake

---

## 📦 Instalación

### 🔧 Servidor

```bash
cd YaPPuccino-main/Servidor
mkdir build && cd build
cmake ..
make
./server
```

> El servidor escucha por defecto en el puerto `5000`.

### 💻 Cliente Qt

```bash
cd YaPPuccino-main/Cliente/YaPPuccinoClient
mkdir build && cd build
cmake ..
make
./YaPPuccinoClient
```

---

## 🚀 Ejecución

1. Ejecuta el servidor en la terminal:  
   ```bash
   ./server
   ```

2. Ejecuta el cliente y escribe tu nombre de usuario.

3. El cliente se conectará usando WebSocket a `ws://localhost:5000/?name=TuNombre`.

---

## 📡 Protocolo Binario

Cada mensaje binario comienza con un **opcode** (1 byte) seguido de campos con su respectiva longitud:

### Ejemplos de Opcodes

- `1`: LIST_USERS
- `2`: GET_USER
- `3`: CHANGE_STATUS
- `4`: SEND_MESSAGE
- `5`: GET_HISTORY
- `6`: LIST_ALL_USERS
- `50–57`: Respuestas/Notificaciones

> Ver más en `BinaryMessageHandler.cpp/.h`

---

## 🗂️ Estructura del Proyecto

```
YaPPuccino-main/
├── Cliente/                  # Cliente Qt
│   └── YaPPuccinoClient/
├── Servidor/                # Código del servidor
│   ├── BinaryMessageHandler.*
│   ├── HistoryManager.*
│   └── server.cpp
├── main.cpp                 # Punto de entrada
└── README.md
```

---

## 👨‍💻 Créditos

**Universidad del Valle de Guatemala – Curso de Sistemas Operativos**

- Irving Fabricio Morales Acosta (22781)  
- Madeline Nahomy Castro Morales (22473)  
- Aroldo Xavier López Osoy (22716)

Catedrático: **Sebastián Galindo**

---

## 📄 Licencia

Este proyecto es de uso académico y educativo.
