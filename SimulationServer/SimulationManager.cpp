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

    const auto targetTimeStep = std::chrono::milliseconds(GameConfig::UPDATE_INTERVAL_MS);
    auto nextUpdateTime = Clock::now() + targetTimeStep;

    std::cout << "[Server] Simulation loop started.\n";

    while (!exitFlag) {
        // Wait until it's time for the next update
        auto timeToWait = nextUpdateTime - Clock::now();
        if (timeToWait.count() > 0) {
            std::this_thread::sleep_for(timeToWait);
        }

        // Process a single step of simulation
        {
            std::lock_guard<std::mutex> lock(ballMutex);
            if (clientConnected) {
                if (!simulationStarted) {
                    std::cout << "[Server] Client connected. Starting simulation in 3 seconds...\n";
                    simulationStarted = true;

                    // Notify client that we're ready to start
                    dataUpdated = true;
                    dataReadyCV.notify_one();

                    std::this_thread::sleep_for(std::chrono::seconds(3));
                    std::cout << "[Server] Simulation started!\n";

                    // Reset next update time after the initial delay
                    nextUpdateTime = Clock::now() + targetTimeStep;
                    continue;
                }

                if (simulationStarted) {
                    // Update simulation state
                    for (auto& ball : balls) {
                        if (!ball->isDead()) {
                            ball->updateCooldowns();
                            std::shared_ptr<Ball> target = findNearestEnemy(ball);
                            if (target) {
                                ball->moveToward(target);
                            }
                            else {
                                ball->wander();
                            }
                        }
                    }

                    handleCombat();
                    removeDeadBalls();
                }

                // Mark data as updated for network thread
                dataUpdated = true;
                dataReadyCV.notify_one();
            }
        }

        // Schedule next update exactly 100ms after the current one
        nextUpdateTime += targetTimeStep;
    }

    std::cout << "[Server] Simulation loop exited.\n";
}



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

void SimulationManager::handleCombat() {
    for (auto& attacker : balls) {
        if (attacker->isDead()) continue;

        // Skip if attacker is on cooldown
        if (!attacker->canAttack()) continue;

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
            // Reset the attack cooldown when an attack is made
            attacker->resetAttackCooldown();

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
    dataReadyCV.wait(lock, [this] { return dataUpdated || exitFlag; });
}

void SimulationManager::resetUpdateFlag() {
    dataUpdated = false;
}