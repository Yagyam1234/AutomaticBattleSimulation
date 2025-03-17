#include "NetworkManager.h"
#include "GameConfig.h"
#include <iostream>
#include <sstream>
#include <iomanip>

NetworkManager::NetworkManager(SimulationManager& simManager)
    : simulationManager(simManager), serverSocket(INVALID_SOCKET),
    clientSocket(INVALID_SOCKET), initialized(false) {}

NetworkManager::~NetworkManager() {
    closeConnection();
}

std::string getCurrentTimestamp() {
    using namespace std::chrono;

    auto now = system_clock::now();
    auto timeT = system_clock::to_time_t(now);
    auto nowMs = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tmTime;
    localtime_s(&tmTime, &timeT);

    std::ostringstream oss;
    oss << std::put_time(&tmTime, "[%Y-%m-%d %H:%M:%S]")
        << "." << std::setw(3) << std::setfill('0') << nowMs.count();

    return oss.str();
}

bool NetworkManager::initialize() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "[Server] WSAStartup failed!\n";
        return false;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "[Server] Failed to create socket.\n";
        WSACleanup();
        return false;
    }

    const char opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddr = { AF_INET, htons(GameConfig::SERVER_PORT), INADDR_ANY };
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[Server] Bind failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }

    if (listen(serverSocket, 1) == SOCKET_ERROR) {
        std::cerr << "[Server] Listen failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return false;
    }

    initialized = true;
    return true;
}

bool NetworkManager::waitForClient() {
    if (!initialized) return false;

    std::cout << "[Server] Waiting for Unreal client...\n";

    clientSocket = accept(serverSocket, nullptr, nullptr);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "[Server] Accept failed.\n";
        return false;
    }

    std::cout << "[Server] Client connected!\n";
    simulationManager.signalClientConnected();
    return true;
}

void NetworkManager::sendInitializationData() {
    if (clientSocket == INVALID_SOCKET) return;

    auto balls = simulationManager.getBalls();
    std::ostringstream ss;

    ss << "GridSize=" << GameConfig::GRID_SIZE << ";BallCount=" << balls.size();

    for (const auto& ball : balls) {
        ss << ";" << ball->getID() << "," << ball->getX() << "," << ball->getY()
            << "," << ball->getHp() << "," << (ball->isRedTeam() ? "1" : "0");
    }

    std::string initMessage = ss.str();
    send(clientSocket, initMessage.c_str(), static_cast<int>(initMessage.length()), 0);
    std::cout << "[Server] Sent initialization data to client.\n";
}

void NetworkManager::sendSimulationData() {
    if (clientSocket == INVALID_SOCKET) return;

    std::string lastSentData;

    while (!simulationManager.shouldExit()) {
        simulationManager.waitForUpdate();

        if (simulationManager.isGameOver()) {
            sendGameOverMessage(simulationManager.getWinningTeam());
            break;
        }

        auto balls = simulationManager.getBalls();
        std::ostringstream ss;
        ss << balls.size();

        for (const auto& ball : balls) {
            ss << ";" << ball->getID() << "," << ball->getX() << ","
                << ball->getY() << "," << ball->getHp() << "," << (ball->isRedTeam() ? "1" : "0");
        }

        std::string updateMessage = ss.str();

        if (updateMessage == lastSentData) continue;

        lastSentData = updateMessage;
        int result = send(clientSocket, updateMessage.c_str(), static_cast<int>(updateMessage.length()), 0);

        if (result == SOCKET_ERROR) {
            std::cerr << "[Server] Client disconnected. Stopping server.\n";
            return;
        }

        std::cout << getCurrentTimestamp() << " [Server] Sent data: " << updateMessage << std::endl;
        simulationManager.resetUpdateFlag();
    }
}




void NetworkManager::sendGameOverMessage(const std::string& message) {
    if (clientSocket == INVALID_SOCKET) return;

    std::string gameOverMessage = "GameOver:" + message;
    send(clientSocket, gameOverMessage.c_str(), static_cast<int>(gameOverMessage.length()), 0);
    std::cout << "[Server] Sent '" << gameOverMessage << "' to client.\n";
}

void NetworkManager::closeConnection() {
    static std::atomic<bool> closed(false);
    if (closed.exchange(true)) return;  // Ensure it runs only once

    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }

    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }

    std::cout << "[Server] All connections closed properly.\n";
}