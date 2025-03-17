#include "Ball.h"
#include "GameConfig.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <unordered_map>

int Ball::NextID = 1;

// Constructor with Proper Random Initialization
Ball::Ball(int startX, int startY, bool redTeam, std::mt19937& rng)
    : x(startX), y(startY), isRed(redTeam), attackCooldown(0), path() {
    ID = NextID++;
    // Generate random HP in range [MIN_HP, MAX_HP]
    std::uniform_int_distribution<int> hpDist(MIN_HP, MAX_HP);
    hp = hpDist(rng); // Properly randomized HP
    std::cout << "[Ball] Created " << (isRed ? "Red" : "Blue")
        << " Ball at (" << x << ", " << y << ") with HP: " << hp << std::endl;
}

struct Node {
    int x, y;
    int g, h, f;
    std::shared_ptr<Node> parent;
    Node(int x, int y, int g, int h, std::shared_ptr<Node> parent = nullptr)
        : x(x), y(y), g(g), h(h), f(g + h), parent(parent) {}
};

struct Compare {
    bool operator()(const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
        return a->f > b->f;  // Min-heap based on f cost
    }
};


std::vector<std::pair<int, int>> Ball::findPath(int startX, int startY, int targetX, int targetY) {
    std::priority_queue<std::shared_ptr<Node>, std::vector<std::shared_ptr<Node>>, Compare> openSet;
    std::unordered_map<int, std::unordered_map<int, bool>> closedSet;
    auto heuristic = [](int x1, int y1, int x2, int y2) {
        return std::abs(x1 - x2) + std::abs(y1 - y2);  // Manhattan distance
    };
    openSet.push(std::make_shared<Node>(startX, startY, 0, heuristic(startX, startY, targetX, targetY)));
    std::vector<std::pair<int, int>> directions = { {0, 1}, {1, 0}, {0, -1}, {-1, 0} };
    while (!openSet.empty()) {
        auto current = openSet.top();
        openSet.pop();
        if (current->x == targetX && current->y == targetY) {
            std::vector<std::pair<int, int>> path;
            while (current) {
                path.emplace_back(current->x, current->y);
                current = current->parent;
            }
            std::reverse(path.begin(), path.end());
            return path;
        }
        closedSet[current->x][current->y] = true;
        for (const auto& dir : directions) {
            int nx = current->x + dir.first;
            int ny = current->y + dir.second;
            if (nx < 0 || ny < 0 || nx >= GameConfig::GRID_SIZE || ny >= GameConfig::GRID_SIZE) continue;
            if (closedSet[nx][ny]) continue;
            auto neighbor = std::make_shared<Node>(nx, ny, current->g + 1, heuristic(nx, ny, targetX, targetY), current);
            openSet.push(neighbor);
        }
    }
    return {};  // Return empty if no path found
}

void Ball::moveToward(std::shared_ptr<Ball> target) {
    if (hp <= 0 || !target || target->hp <= 0) return;

    // Always recalculate the path if target moved or if we're close to completing the path
    // This prevents getting stuck when targets move during path following
    if (path.empty() || path.size() < 3 ||
        (std::abs(target->getX() - path.back().first) > 1 ||
            std::abs(target->getY() - path.back().second) > 1)) {

        // Clear the old path
        path = std::queue<std::pair<int, int>>();

        // Calculate new path
        auto newPath = findPath(x, y, target->getX(), target->getY());

        // Skip the first node (current position)
        if (!newPath.empty()) newPath.erase(newPath.begin());

        // Add path steps to queue
        for (const auto& step : newPath) {
            path.push(step);
        }
    }

    // Take next step on path if available
    if (!path.empty()) {
        auto nextMove = path.front();
        path.pop();

        // Only move if this actually brings us closer to target
        int currentDist = std::abs(x - target->getX()) + std::abs(y - target->getY());
        int newDist = std::abs(nextMove.first - target->getX()) +
            std::abs(nextMove.second - target->getY());

        if (newDist < currentDist) {
            x = nextMove.first;
            y = nextMove.second;
        }
        else {
            // If the next step doesn't bring us closer, recalculate path next time
            path = std::queue<std::pair<int, int>>();
        }
    }
    else {
        // Direct movement if no path or path is invalid
        int dx = (target->getX() > x) ? 1 : (target->getX() < x) ? -1 : 0;
        int dy = (target->getY() > y) ? 1 : (target->getY() < y) ? -1 : 0;

        x = std::clamp(x + dx, 0, GameConfig::GRID_SIZE - 1);
        y = std::clamp(y + dy, 0, GameConfig::GRID_SIZE - 1);
    }

    // Keep within grid boundaries
    x = std::clamp(x, 0, GameConfig::GRID_SIZE - 1);
    y = std::clamp(y, 0, GameConfig::GRID_SIZE - 1);

    // Update cooldowns
    if (attackCooldown > 0) attackCooldown--;
}

void Ball::wander() {
    // Simple wandering movement if no valid enemy found
    int dx = (rand() % 3) - 1;  // Random -1, 0, or 1
    int dy = (rand() % 3) - 1;

    x = std::clamp(x + dx, 0, GameConfig::GRID_SIZE - 1);
    y = std::clamp(y + dy, 0, GameConfig::GRID_SIZE - 1);
}


bool Ball::takeDamage(int amount) {
    hp -= amount;
    return isDead();
}