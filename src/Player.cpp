#include "Player.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

const float MOVE_SPEED = 12.0f;  // unit per detik

Player::Player(float x, float z, bool isPlayer1)
    : position(x, 0.5f, z),
      rotation(0.0f),
      turretAngle(0.0f),
      health(100),
      lives(3),
      player1(isPlayer1),
      moveSpeed(MOVE_SPEED),
      spawnPos(x, 0.5f, z),
      maxHealth(100) {}

Player::~Player() {}

// Gerak 8-arah absolut di world space.
// dx = horizontal (D=+1, A=-1), dz = vertikal (S=+1, W=-1)
// Tank otomatis menghadap ke arah gerak.
void Player::MoveInDirection(float dx, float dz, float deltaTime) {
    if (dx == 0.0f && dz == 0.0f) return;

    // Normalisasi diagonal agar speed sama di semua arah
    float len = sqrtf(dx * dx + dz * dz);
    dx /= len;
    dz /= len;

    position.x += dx * moveSpeed * deltaTime;
    position.z += dz * moveSpeed * deltaTime;

    // Auto-rotate tank menghadap arah gerak
    // Negasi dx karena OpenGL: forward=-Z, right=+X, tapi atan2 pakai konvensi berbeda
    float targetRot = atan2f(-dx, -dz);
    rotation = targetRot;
}

void Player::ClampPosition(float minBound, float maxBound) {
    position.x = std::clamp(position.x, minBound, maxBound);
    position.z = std::clamp(position.z, minBound, maxBound);
}

void Player::TakeDamage(int damage) {
    health -= damage;
    if (health < 0) health = 0;

    if (health == 0 && lives > 0) {
        Respawn();
    }
}

void Player::Respawn() {
    if (lives > 0) {
        lives--;
        health = maxHealth;
        position = spawnPos;
        rotation = 0.0f;
    }
}

void Player::Reset(float x, float z) {
    position = glm::vec3(x, 0.5f, z);
    rotation = 0.0f;
    health = maxHealth;
    lives = 3;
    spawnPos = glm::vec3(x, 0.5f, z);
}

void Player::Update(float deltaTime) {
    // placeholder
}

