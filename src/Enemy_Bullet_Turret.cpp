// ===== ENEMY.CPP =====
#include "Enemy_Bullet_Turret.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

Enemy::Enemy(glm::vec3 spawnPos, int hp, float speed)
    : position(spawnPos), rotation(0.0f), health(hp), moveSpeed(speed) {
    // Randomize initial cooldown between 0.5 and 2.5 seconds to stagger shots
    shootCooldown = 0.5f + (float)(rand() % 200) / 100.0f;
}

Enemy::~Enemy() {}

void Enemy::Update(float deltaTime) {
    if (shootCooldown > 0.0f) {
        shootCooldown -= deltaTime;
    }
}

void Enemy::MoveTowards(glm::vec3 target, float speed) {
    // Proyeksikan target ke ketinggian yang sama dengan musuh agar terbang stabil
    target.y = position.y;

    if (glm::distance(target, position) > 0.01f) {
        glm::vec3 direction = glm::normalize(target - position);
        position += direction * speed;
        rotation = atan2(direction.x, direction.z);
    }
}

void Enemy::TakeDamage(int damage) {
    health -= damage;
}


// ===== BULLET.CPP =====

Bullet::Bullet(glm::vec3 pos, glm::vec3 dir, bool isEnemyBullet)
    : position(pos), direction(glm::normalize(dir)), speed(20.0f),
      lifetime(0.0f), maxLifetime(3.0f), alive(true), isEnemy(isEnemyBullet) {}

Bullet::~Bullet() {}

void Bullet::Update(float deltaTime) {
    position += direction * speed * deltaTime;
    lifetime += deltaTime;

    if (lifetime >= maxLifetime) {
        alive = false;
    }

    // Out of bounds check
    if (position.x > 30.0f || position.x < -30.0f ||
        position.z > 30.0f || position.z < -30.0f) {
        alive = false;
    }
}


// ===== TURRET.CPP =====

Turret::Turret(glm::vec3 pos, int hp)
    : position(pos), health(hp), maxHealth(hp), alive(true) {}

Turret::~Turret() {}

void Turret::Update(float deltaTime) {
    // Update turret logic jika perlu
}

void Turret::TakeDamage(int damage) {
    health -= damage;
    if (health <= 0) {
        health = 0;
        alive = false;
    }
}

void Turret::Destroy() {
    alive = false;
    health = 0;
}
