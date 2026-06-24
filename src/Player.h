#ifndef PLAYER_H
#define PLAYER_H

#include <glm/glm.hpp>

class Player {
public:
    Player(float x, float z, bool isPlayer1);
    ~Player();

    // Movement - 8-arah absolut (world space)
    void MoveInDirection(float dx, float dz, float deltaTime);
    void ClampPosition(float minBound, float maxBound);

    // Combat
    void TakeDamage(int damage);
    void Respawn();
    void Reset(float x, float z);

    // Getters
    glm::vec3 GetPosition() const { return position; }
    glm::vec3 GetRotation() const { return glm::vec3(0.0f, rotation, 0.0f); }
    int GetHealth() const { return health; }
    int GetLives() const { return lives; }
    bool IsPlayer1() const { return player1; }
    float GetTurretAngle() const { return turretAngle; }

    // Setters
    void SetTurretAngle(float angle) { turretAngle = angle; }

    void Update(float deltaTime);

private:
    glm::vec3 position;
    float rotation;       // Y rotation (radian)
    float turretAngle;
    int health;           // 0-100
    int lives;            // max 3
    bool player1;
    float moveSpeed;
    glm::vec3 spawnPos;
    int maxHealth;
};

#endif // PLAYER_H
