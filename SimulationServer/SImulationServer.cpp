#define NOMINMAX
#include "GameConfig.h"
#include "SimulationManager.h"
#include "NetworkManager.h"
#include <iostream>
#include <thread>
#include <random>

int main() {
    // Initialize random number generator with seed
    std::mt19937 rng(42);

    // Create simulation manager
    SimulationManager simulationManager;
    simulationManager.initialize(rng);

    // Create network manager
    NetworkManager networkManager(simulationManager);

    // Initialize network
    if (!networkManager.initialize()) {
        std::cerr << "[Server] Failed to initialize network.\n";
        return -1;
    }

    // Start the simulation thread
    std::thread simThread(&SimulationManager::updateSimulation, &simulationManager);

    // Wait for client connection
    if (!networkManager.waitForClient()) {
        std::cerr << "[Server] Failed to connect with client.\n";
        simulationManager.signalShouldExit();
        simThread.join();
        return -1;
    }

    // Send initial game state
    networkManager.sendInitializationData();

    // Start the data sending thread
    std::thread sendThread(&NetworkManager::sendSimulationData, &networkManager);

    // Wait for threads to complete
    sendThread.join();

    // Ensure simulation thread exits
    simulationManager.signalShouldExit();
    simThread.join();

    // Close network connection
    networkManager.closeConnection();

    std::cout << "[Server] Shutdown complete.\n";
    return 0;
}