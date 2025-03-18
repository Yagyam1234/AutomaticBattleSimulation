#include "SimulationManager.h"
#include "GameConfig.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>

SimulationManager::SimulationManager()
    : clientConnected(false), exitFlag(false), dataUpdated(false), simulationStarted(false) {}

SimulationManager::~SimulationManager() {}

void SimulationManager::initialize(std::mt19937& rng) {
    std::lock_guard<std::mutex> lock(ballMutex);
    balls.clear();
    std::uniform_int_distribution<int> posDist(1, GameConfig::GRID_SIZE - 2); // Avoid spawning at edges

    for (int i = 0; i < GameConfig::MAX_UNITS / 2; ++i) {
        balls.push_back(std::make_shared<Ball>(posDist(rng), posDist(rng), true, rng));
        balls.push_back(std::make_shared<Ball>(posDist(rng), posDist(rng), false, rng));
    }

    std::cout << "[Server] Balls initialized within grid boundaries.\n";
}

void SimulationManager::updateSimulation() {
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    auto simulationTime = Clock::now();
    const auto timeStep = std::chrono::milliseconds(GameConfig::UPDATE_INTERVAL_MS);
    int stepCount = 0;

    std::cout << "[Server] Simulation loop started.\n";

    while (!exitFlag) {
        simulationTime += timeStep;

        {
            std::lock_guard<std::mutex> lock(ballMutex);
            if (clientConnected) {
                // Start the simulation after client connects with a 3-second delay
                if (!simulationStarted) {
                    std::cout << "[Server] Client connected. Starting simulation in 3 seconds...\n";
                    simulationStarted = true;

                    // Set the dataUpdated flag to true so client can see the initial state
                    dataUpdated = true;
                    dataReadyCV.notify_one();

                   
                    std::this_thread::sleep_for(std::chrono::seconds(3));

                   
                    std::cout << "[Server] Simulation started!\n";
                }

                // Only update ball positions if simulation has started
                if (simulationStarted) {
                    // Simplified ball movement logic
                    for (auto& ball : balls) {
                        if (!ball->isDead()) {
                            std::shared_ptr<Ball> target = findNearestEnemy(ball);
                            if (target) {
                                ball->moveToward(target);
                            }
                            else {
                                // If no valid target, make sure they still move slightly
                                ball->wander();
                            }
                        }
                    }

                    handleCombat();
                    removeDeadBalls();
                }

                dataUpdated = true;
                dataReadyCV.notify_one();
            }
        }

        auto now = Clock::now();
        if (now < simulationTime) {
            std::this_thread::sleep_until(simulationTime);
        }
        else if (now > simulationTime + 5ms) {
            std::cout << "[Server] Warning: Simulation running behind schedule by "
                << std::chrono::duration_cast<std::chrono::milliseconds>(now - simulationTime).count()
                << "ms on step " << stepCount << std::endl;

            simulationTime = now;
        }

        stepCount++;
    }

    std::cout << "[Server] Simulation loop exited after " << stepCount << " steps.\n";
}


// Helper method to find nearest enemy - simplified to handle the last enemy case once
std::shared_ptr<Ball> SimulationManager::findNearestEnemy(const std::shared_ptr<Ball>& ball) {
    std::shared_ptr<Ball> target = nullptr;
    int minDist = GameConfig::GRID_SIZE * 2;

    int enemyCount = 0;
    std::shared_ptr<Ball> lastEnemy = nullptr;

    for (auto& other : balls) {
        if (ball->isRedTeam() != other->isRedTeam() && !other->isDead()) {
            enemyCount++;
            lastEnemy = other;  // Track last seen enemy

            int dist = std::abs(ball->getX() - other->getX()) +
                std::abs(ball->getY() - other->getY());

            if (dist < minDist) {
                minDist = dist;
                target = other;
            }
        }
    }

    // If only one enemy remains, force all to target it
    if (enemyCount == 1 && lastEnemy) {
        return lastEnemy;
    }

    return target;
}


// Handle combat interactions
void SimulationManager::handleCombat() {
    for (auto& attacker : balls) {
        if (attacker->isDead()) continue;

        std::shared_ptr<Ball> bestTarget = nullptr;
        int bestDistance = GameConfig::GRID_SIZE * 2;

        for (auto& defender : balls) {
            if (defender->isDead() || attacker->isRedTeam() == defender->isRedTeam()) continue;

            int distance = std::abs(attacker->getX() - defender->getX()) +
                std::abs(attacker->getY() - defender->getY());

            if (distance <= attacker->getAttackRange() && distance < bestDistance) {
                bestTarget = defender;
                bestDistance = distance;
            }
        }

        if (bestTarget) {
            bool targetKilled = bestTarget->takeDamage(1);
            std::string teamName = attacker->isRedTeam() ? "Red" : "Blue";
            std::cout << "[Server] " << teamName << " Ball attacked! Target HP: " << bestTarget->getHp() << std::endl;
        }
    }
}


void SimulationManager::removeDeadBalls() {
    balls.erase(
        std::remove_if(balls.begin(), balls.end(),
            [](const std::shared_ptr<Ball>& b) { return b->isDead(); }
        ),
        balls.end()
    );

    bool redExists = false, blueExists = false;
    for (const auto& ball : balls) {
        if (ball->isRedTeam()) redExists = true;
        else blueExists = true;
    }

    if (!redExists || !blueExists) {
        winningTeam = redExists ? "Red Team Wins!" : "Blue Team Wins!";
        std::cout << "[Server] Game Over! " << winningTeam << std::endl;

        exitFlag = true;  // Stops simulation loop
        dataUpdated = true;
        dataReadyCV.notify_all();  // Notifies network threads to stop
    }
}

std::vector<std::shared_ptr<Ball>> SimulationManager::getBalls() const {
    std::lock_guard<std::mutex> lock(ballMutex);
    return balls;
}

std::string SimulationManager::getWinningTeam() const {
    return winningTeam;
}

bool SimulationManager::isGameOver() const {
    return !winningTeam.empty();
}

void SimulationManager::signalClientConnected() {
    clientConnected = true;
}

void SimulationManager::signalShouldExit() {
    exitFlag = true;
}

bool SimulationManager::shouldExit() const {
    return exitFlag;
}

void SimulationManager::waitForUpdate() {
    std::unique_lock<std::mutex> lock(ballMutex);
    dataReadyCV.wait_for(lock, std::chrono::milliseconds(GameConfig::UPDATE_INTERVAL_MS),
        [this] { return dataUpdated || exitFlag; });
}

void SimulationManager::resetUpdateFlag() {
    dataUpdated = false;
}