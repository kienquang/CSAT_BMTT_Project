#include <iostream>
#include <winsock2.h>
#include "DatabaseHelper.h"
#include "NetworkData.h"

#pragma comment(lib, "ws2_32.lib")

int main() {
    std::cout << "[SERVER] TCP Server starting..." << std::endl;
    
    // TODO: Implement TCP server
    // - Initialize Winsock2
    // - Create listening socket on port 8080
    // - Accept client connections
    // - Receive PacketData structures
    // - Call DatabaseHelper methods
    // - Apply MaskingLogic for User-role requests
    // - Send responses back to client
    
    std::cout << "[SERVER] Server shutting down..." << std::endl;
    return 0;
}
