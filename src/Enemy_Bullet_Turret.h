// ===== ENEMY.H =====
#ifndef ENEMY_H
#define ENEMY_H

#include <glm/glm.hpp>

class Enemy {
public:
    Enemy(glm::vec3 spawnPos, int hp);
    ~Enemy();

    void Update(float deltaTime);
    void MoveTowards(glm::vec3 target, float speed);
    void TakeDamage(int damage);

    glm::vec3 GetPosition() const { return position; }
    int GetHealth() const { return health; }
    float GetRotation() const { return rotation; }
    
    bool CanShoot() const { return shootCooldown <= 0.0f; }
    void ResetShootCooldown() { shootCooldown = 2.0f; } // Cooldown 2 detik

private:
    glm::vec3 position;
    float rotation;
    int health;
    float moveSpeed;
    float shootCooldown;
};

#endif // ENEMY_H


// ===== BULLET.H =====
#ifndef BULLET_H
#define BULLET_H

#include <glm/glm.hpp>

class Bullet {
public:
    Bullet(glm::vec3 pos, glm::vec3 dir, bool isEnemyBullet = false);
    ~Bullet();

    void Update(float deltaTime);
    void Kill() { alive = false; }

    glm::vec3 GetPosition() const { return position; }
    bool IsAlive() const { return alive; }
    bool IsEnemyBullet() const { return isEnemy; }

private:
    glm::vec3 position;
    glm::vec3 direction;
    float speed;
    float lifetime;
    float maxLifetime;
    bool alive;
    bool isEnemy;
};

#endif // BULLET_H


// ===== TURRET.H =====
#ifndef TURRET_H
#define TURRET_H

#include <glm/glm.hpp>

class Turret {
public:
    Turret(glm::vec3 pos, int hp);
    ~Turret();

    void Update(float deltaTime);
    void TakeDamage(int damage);
    void Destroy();

    glm::vec3 GetPosition() const { return position; }
    int GetHealth() const { return health; }
    bool IsAlive() const { return alive; }

private:
    glm::vec3 position;
    int health;
    int maxHealth;
    bool alive;
};

#endif // TURRET_H
