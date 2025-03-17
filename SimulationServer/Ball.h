#pragma once
#include <memory>
#include <random>
#include <queue>
#include <vector>

class Ball {
public:
    Ball(int startX, int startY, bool redTeam, std::mt19937& rng);

    // Movement methods
    void moveToward(std::shared_ptr<Ball> target);

    // Combat methods
   
    bool takeDamage(int amount);  // Returns true if killed

    // Add accessor for attack range
    int getAttackRange() const { return ATTACK_RANGE; }

    // Existing accessors
    int getX() const { return x; }
    int getY() const { return y; }
    int getHp() const { return hp; }
    bool isRedTeam() const { return isRed; }
    int getID() const { return ID; }
    bool isDead() const { return hp <= 0; }

    void wander();

    // Add cooldown management
    bool canAttack() const { return attackCooldown <= 0; }
    void resetAttackCooldown() { attackCooldown = ATTACK_RATE; }
    void updateCooldowns() { if (attackCooldown > 0) attackCooldown--; }

private:
    static int NextID ; // Static counter for unique IDs
    int ID;
    int x, y;
    int hp;
    bool isRed;
    int attackCooldown;

    // Constants
    static const int ATTACK_RANGE = 1;
    static const int ATTACK_RATE = 3;
    static const int MIN_HP = 2;
    static const int MAX_HP = 5;

    // Pathfinding
    std::queue<std::pair<int, int>> path;  // Stores path to target
    std::vector<std::pair<int, int>> findPath(int startX, int startY, int targetX, int targetY);
};