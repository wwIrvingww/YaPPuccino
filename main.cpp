#include <iostream>
#include "websocket_server.h"

int main () {
    try{
        WebsocketServer serve (8080) //no se que puerto vamos a usar asi que para mientras eso
        server.start();
    } catch (const std::exception& e){
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}