#pragma once

#include "Ball.h"
#include <vector>
#include <random>
#include <mutex>
#include <condition_variable>
#include <atomic>

class SimulationManager {
public:
    SimulationManager();
    ~SimulationManager();

    void initialize(std::mt19937& rng);
    void updateSimulation();
    std::shared_ptr<Ball> findNearestEnemy(const std::shared_ptr<Ball>& ball);
    void handleCombat();
    void removeDeadBalls();

    std::vector<std::shared_ptr<Ball>> getBalls() const;
    std::string getWinningTeam() const;
    bool isGameOver() const;

    void signalClientConnected();
    void signalShouldExit();
    bool shouldExit() const;

    void waitForUpdate();
    void resetUpdateFlag();

private:
    std::vector<std::shared_ptr<Ball>> balls;
    mutable std::mutex ballMutex;
    std::atomic<bool> clientConnected;
    std::atomic<bool> exitFlag;
    std::condition_variable dataReadyCV;
    bool dataUpdated;
    std::string winningTeam;
};