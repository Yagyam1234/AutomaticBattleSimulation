#pragma once

#include <winsock2.h>
#include <string>
#include <atomic>
#include "SimulationManager.h"

#pragma comment(lib, "ws2_32.lib")

class NetworkManager {
public:
    NetworkManager(SimulationManager& simManager);
    ~NetworkManager();

    bool initialize();
    bool waitForClient();
    void sendInitializationData();
    void sendSimulationData();
    void sendGameOverMessage(const std::string& message);
    void closeConnection();

private:
    SimulationManager& simulationManager;
    SOCKET serverSocket;
    SOCKET clientSocket;
    std::atomic<bool> initialized;
};