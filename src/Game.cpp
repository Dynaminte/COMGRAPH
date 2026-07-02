#include "Game.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <algorithm>

// Cooldown tracking untuk shooting
static float shootCooldown1 = 0.0f;
static float shootCooldown2 = 0.0f;
const float SHOOT_COOLDOWN = 0.15f;  // detik antar tembakan


const float WAVE_DURATION = 30.0f;
const float ARENA_SIZE = 25.0f;
const glm::vec2 P1_SPAWN(-10.0f, -10.0f);
const glm::vec2 P2_SPAWN(10.0f, 10.0f);

Game::Game(int width, int height)
    : screenWidth(width), screenHeight(height),
      gameState(MENU), gameTimer(0), waveTimer(0), currentWave(1),
      turretCount(3), spawnTimer(0), spawnDelay(1.5f), maxEnemies(8),
      spawnedCount(0), score(0), stars(0),
      player1(P1_SPAWN.x, P1_SPAWN.y, true),
      player2(P2_SPAWN.x, P2_SPAWN.y, false),
      renderer(width, height) {

    // Initialize turrets agak berjarak (menyebar di arena)
    glm::vec3 turretPositions[3] = {
        glm::vec3(0.0f, 0.0f, 10.0f),
        glm::vec3(-12.0f, 0.0f, -6.0f),
        glm::vec3(12.0f, 0.0f, -6.0f)
    };

    for (int i = 0; i < 3; i++) {
        turrets.push_back(Turret(turretPositions[i], 100));
        turretHealth[i] = 100;
    }
}

Game::~Game() {}

bool Game::Initialize() {
    if (!renderer.Initialize()) {
        std::cerr << "Renderer initialization failed!" << std::endl;
        return false;
    }
    ResetGame();
    return true;
}

void Game::ProcessInput(GLFWwindow* window) {
    // deltaTime approx untuk input — movement berbasis deltaTime
    // Diambil dari waktu nyata agar smooth
    static double lastInputTime = glfwGetTime();
    double now = glfwGetTime();
    float dt = (float)(now - lastInputTime);
    if (dt > 0.05f) dt = 0.05f;  // cap agar tidak loncat jika lag
    lastInputTime = now;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    static bool mousePressed = false;
    bool mouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    if (gameState == MENU) {
        if (mouseDown && !mousePressed) {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            
            int winW, winH;
            glfwGetWindowSize(window, &winW, &winH);
            if (winW <= 0 || winH <= 0) { mousePressed = mouseDown; return; }
            
            float virtualX = (float)(xpos / winW) * screenWidth;
            float virtualY = (float)(ypos / winH) * screenHeight;

            float cx = screenWidth / 2.0f;
            float cy = screenHeight / 2.0f;

            // PLAY Button (diperbesar hit area +10px tiap sisi)
            if (virtualX >= cx - 70.0f && virtualX <= cx + 70.0f &&
                virtualY >= cy + 85.0f  && virtualY <= cy + 135.0f) {
                gameState = PLAYING;
                ResetGame();
            }
            // HOW TO PLAY Button (diperbesar)
            else if (virtualX >= cx - 130.0f && virtualX <= cx + 130.0f &&
                     virtualY >= cy + 145.0f   && virtualY <= cy + 195.0f) {
                gameState = HOW_TO_PLAY;
            }
            // QUIT Button (diperbesar)
            else if (virtualX >= cx - 70.0f && virtualX <= cx + 70.0f &&
                     virtualY >= cy + 205.0f  && virtualY <= cy + 255.0f) {
                glfwSetWindowShouldClose(window, true);
            }
        }
    } else if (gameState == HOW_TO_PLAY) {
        if (mouseDown && !mousePressed) {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            
            int winW, winH;
            glfwGetWindowSize(window, &winW, &winH);
            if (winW <= 0 || winH <= 0) { mousePressed = mouseDown; return; }
            
            float virtualX = (float)(xpos / winW) * screenWidth;
            float virtualY = (float)(ypos / winH) * screenHeight;

            float cx = screenWidth / 2.0f;
            float cy = screenHeight / 2.0f;

            // BACK Button (diperbesar)
            if (virtualX >= cx - 70.0f && virtualX <= cx + 70.0f &&
                virtualY >= cy + 145.0f  && virtualY <= cy + 195.0f) {
                gameState = MENU;
            }
        }
    }

    mousePressed = mouseDown;

    if (gameState == PLAYING) {
        // === PLAYER 1 (WASD) — 8-arah absolut ===
        float dx1 = 0.0f, dz1 = 0.0f;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) dz1 -= 1.0f;  // maju  (world -Z)
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) dz1 += 1.0f;  // mundur(world +Z)
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) dx1 -= 1.0f;  // kiri  (world -X)
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) dx1 += 1.0f;  // kanan (world +X)
        player1.MoveInDirection(dx1, dz1, dt);

        // Tembak P1 — tombol F, arah = arah hadap tank
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && shootCooldown1 <= 0.0f) {
            glm::vec3 pos = player1.GetPosition();
            float rot = player1.GetRotation().y;
            glm::vec3 dir(-sinf(rot), 0.0f, -cosf(rot));
            glm::vec3 spawnPos = pos + dir * 1.5f + glm::vec3(0.0f, 0.5f, 0.0f);
            bullets.push_back(Bullet(spawnPos, dir));
            shootCooldown1 = SHOOT_COOLDOWN;
        }

        // === PLAYER 2 (IJKL) — 8-arah absolut ===
        float dx2 = 0.0f, dz2 = 0.0f;
        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) dz2 -= 1.0f;
        if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) dz2 += 1.0f;
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) dx2 -= 1.0f;
        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) dx2 += 1.0f;
        player2.MoveInDirection(dx2, dz2, dt);

        // Tembak P2 — tombol Enter / Numpad Enter / N
        if ((glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS ||
             glfwGetKey(window, GLFW_KEY_KP_ENTER) == GLFW_PRESS ||
             glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) && shootCooldown2 <= 0.0f) {
            glm::vec3 pos = player2.GetPosition();
            float rot = player2.GetRotation().y;
            glm::vec3 dir(-sinf(rot), 0.0f, -cosf(rot));
            glm::vec3 spawnPos = pos + dir * 1.5f + glm::vec3(0.0f, 0.5f, 0.0f);
            bullets.push_back(Bullet(spawnPos, dir));
            shootCooldown2 = SHOOT_COOLDOWN;
        }

        // Boundary check
        player1.ClampPosition(-ARENA_SIZE, ARENA_SIZE);
        player2.ClampPosition(-ARENA_SIZE, ARENA_SIZE);
    }

    if (gameState == GAME_OVER) {
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            gameState = MENU;
        }
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            gameState = PLAYING;
            ResetGame();
        }
    }
}

void Game::Update(float deltaTime) {
    switch (gameState) {
        case MENU:
            UpdateMenu(deltaTime);
            break;
        case PLAYING:
            UpdateGameplay(deltaTime);
            break;
        case GAME_OVER:
            UpdateGameOver(deltaTime);
            break;
        case HOW_TO_PLAY:
            break; // no update needed
    }
}

void Game::UpdateMenu(float deltaTime) {
    // Menu logic - tunggu input
}

void Game::UpdateGameplay(float deltaTime) {
    gameTimer += deltaTime;
    waveTimer += deltaTime;
    spawnTimer += deltaTime;

    // Kurangi cooldown tembak
    if (shootCooldown1 > 0.0f) shootCooldown1 -= deltaTime;
    if (shootCooldown2 > 0.0f) shootCooldown2 -= deltaTime;

    // Spawn enemy
    if (spawnedCount < maxEnemies && spawnTimer >= spawnDelay) {
        SpawnEnemy();
        spawnTimer = 0;
    }

    // Update players
    player1.Update(deltaTime);
    player2.Update(deltaTime);

    // Update enemies
    for (auto& enemy : enemies) {
        enemy.Update(deltaTime);
        
        // Musuh mengejar turret atau tank terdekat yang masih aktif
        float minDist = 1000.0f;
        glm::vec3 targetPos(0.0f, 0.0f, 0.0f);
        bool hasTarget = false;

        for (auto& turret : turrets) {
            if (!turret.IsAlive()) continue; // Skip turret yang sudah hancur
            float dist = glm::distance(enemy.GetPosition(), turret.GetPosition());
            if (dist < minDist) {
                minDist = dist;
                targetPos = turret.GetPosition();
                hasTarget = true;
            }
        }

        // Cek player juga
        float p1Dist = glm::distance(enemy.GetPosition(), player1.GetPosition());
        float p2Dist = glm::distance(enemy.GetPosition(), player2.GetPosition());

        if (p1Dist < minDist) {
            minDist = p1Dist;
            targetPos = player1.GetPosition();
            hasTarget = true;
        }
        if (p2Dist < minDist) {
            minDist = p2Dist;
            targetPos = player2.GetPosition();
            hasTarget = true;
        }

        if (hasTarget) {
            enemy.MoveTowards(targetPos, 5.0f);
            
            // Enemy shooting logic - shoots a bullet towards the target
            if (enemy.CanShoot()) {
                glm::vec3 enemyPos = enemy.GetPosition();
                glm::vec3 shootDir = glm::normalize(targetPos - enemyPos);
                bullets.push_back(Bullet(enemyPos, shootDir, true)); // true = isEnemyBullet
                enemy.ResetShootCooldown();
            }
        }
    }

    // Update bullets
    for (auto& bullet : bullets) {
        bullet.Update(deltaTime);
    }

    // Check collisions
    CheckCollisions();

    // Wave management - Escalates enemies and speeds up spawning
    if (waveTimer >= WAVE_DURATION) {
        if (enemies.empty() && spawnedCount >= maxEnemies) {
            currentWave++;
            maxEnemies = 8 + currentWave * 4;
            spawnDelay = std::max(0.4f, 1.5f - currentWave * 0.15f);
            spawnedCount = 0;
            waveTimer = 0;
            spawnTimer = 0;
        }
    }

    // Game over check
    bool allTurretsDead = true;
    for (int i = 0; i < 3; i++) {
        if (turretHealth[i] > 0) {
            allTurretsDead = false;
            break;
        }
    }

    bool playersDead = (player1.GetHealth() <= 0 && player1.GetLives() <= 0 &&
                        player2.GetHealth() <= 0 && player2.GetLives() <= 0);

    bool timeUp = (gameTimer >= 60.0f); // 60 detik (1 menit) waktu bertahan

    if (playersDead || allTurretsDead || timeUp) {
        gameState = GAME_OVER;
        isWin = timeUp && !allTurretsDead;
        stars = 0;
        
        if (isWin) {
            for (int i = 0; i < 3; i++) {
                if (turretHealth[i] > 0) stars++;
            }
        }
    }

    // Clean up dead objects
    enemies.erase(
        std::remove_if(enemies.begin(), enemies.end(),
                      [](const Enemy& e) { return e.GetHealth() <= 0; }),
        enemies.end()
    );
    bullets.erase(
        std::remove_if(bullets.begin(), bullets.end(),
                      [](const Bullet& b) { return b.IsAlive() == false; }),
        bullets.end()
    );
}

void Game::UpdateGameOver(float deltaTime) {
    // Wait untuk input restart
}

void Game::SpawnEnemy() {
    float angle = (rand() % 360) * 3.14159f / 180.0f;
    float distance = 50.0f;
    glm::vec3 spawnPos(
        distance * cosf(angle),
        1.6f,  // Ketinggian terbang pesawat
        distance * sinf(angle)
    );
    enemies.push_back(Enemy(spawnPos, 3));  // 3 HP (butuh 3 kali hit)
    spawnedCount++;
}

void Game::CheckCollisions() {
    for (auto& bullet : bullets) {
        if (!bullet.IsAlive()) continue;

        if (bullet.IsEnemyBullet()) {
            // Enemy bullet vs Turrets - Increased collision radius to 2.2f
            for (int i = 0; i < 3; i++) {
                if (!turrets[i].IsAlive()) continue;
                float dist = glm::distance(
                    glm::vec3(bullet.GetPosition().x, 0.0f, bullet.GetPosition().z),
                    glm::vec3(turrets[i].GetPosition().x, 0.0f, turrets[i].GetPosition().z)
                );
                if (dist < 2.2f) {
                    turretHealth[i] -= 5;
                    turrets[i].TakeDamage(5);
                    if (turretHealth[i] <= 0) {
                        turretHealth[i] = 0;
                        turrets[i].Destroy();
                    }
                    bullet.Kill();
                    break;
                }
            }
            if (!bullet.IsAlive()) continue;

            // Enemy bullet vs Player 1
            if (player1.GetHealth() > 0) {
                float dist = glm::distance(
                    glm::vec3(bullet.GetPosition().x, 0.0f, bullet.GetPosition().z),
                    glm::vec3(player1.GetPosition().x, 0.0f, player1.GetPosition().z)
                );
                if (dist < 1.5f) {
                    player1.TakeDamage(2);
                    bullet.Kill();
                    continue;
                }
            }

            // Enemy bullet vs Player 2
            if (player2.GetHealth() > 0) {
                float dist = glm::distance(
                    glm::vec3(bullet.GetPosition().x, 0.0f, bullet.GetPosition().z),
                    glm::vec3(player2.GetPosition().x, 0.0f, player2.GetPosition().z)
                );
                if (dist < 1.5f) {
                    player2.TakeDamage(2);
                    bullet.Kill();
                    continue;
                }
            }
        } else {
            // Player bullet vs Enemy
            for (auto& enemy : enemies) {
                if (enemy.GetHealth() <= 0) continue;
                float dist = glm::distance(
                    glm::vec3(bullet.GetPosition().x, 0.5f, bullet.GetPosition().z),
                    glm::vec3(enemy.GetPosition().x, 0.5f, enemy.GetPosition().z)
                );
                if (dist < 1.8f) {
                    enemy.TakeDamage(1);
                    bullet.Kill();
                    if (enemy.GetHealth() <= 0) {
                        score += 10 + currentWave * 5;
                    }
                    break;
                }
            }
        }
    }

    // Crash collisions
    for (auto& enemy : enemies) {
        if (enemy.GetHealth() <= 0) continue;

        // Crash vs Turrets - Increased collision radius to 2.2f
        for (int i = 0; i < 3; i++) {
            if (!turrets[i].IsAlive()) continue;
            if (glm::distance(enemy.GetPosition(), turrets[i].GetPosition()) < 2.2f) {
                turretHealth[i] -= 10;
                turrets[i].TakeDamage(10);
                if (turretHealth[i] <= 0) {
                    turretHealth[i] = 0;
                    turrets[i].Destroy();
                }
                enemy.TakeDamage(2);
            }
        }

        // Crash vs Player 1
        if (player1.GetHealth() > 0 && glm::distance(enemy.GetPosition(), player1.GetPosition()) < 1.5f) {
            player1.TakeDamage(5);
            enemy.TakeDamage(2);
        }

        // Crash vs Player 2
        if (player2.GetHealth() > 0 && glm::distance(enemy.GetPosition(), player2.GetPosition()) < 1.5f) {
            player2.TakeDamage(5);
            enemy.TakeDamage(2);
        }
    }
}

void Game::Render() {
    glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                                           (float)screenWidth / screenHeight, 0.1f, 100.0f);
    glm::vec3 cameraPos(0.0f, 30.0f, 30.0f);
    glm::mat4 view = glm::lookAt(
        cameraPos,                      // camera position
        glm::vec3(0.0f, 0.0f, 0.0f),    // look at
        glm::vec3(0.0f, 1.0f, 0.0f)     // up
    );
    
    // Light is sun-like, coming from an angle
    glm::vec3 lightPosition = glm::vec3(20.0f, 40.0f, 10.0f);

    // 1. SHADOW PASS
    renderer.BeginShadowPass(lightPosition);
    renderer.DrawGround(projection, view);
    for (auto& turret : turrets) renderer.DrawTurret(turret, projection, view);
    renderer.DrawPlayer(player1, projection, view);
    renderer.DrawPlayer(player2, projection, view);
    for (auto& enemy : enemies) renderer.DrawEnemy(enemy, projection, view);
    for (auto& bullet : bullets) renderer.DrawBullet(bullet, projection, view);

    // 2. MAIN PASS
    renderer.BeginMainPass(projection, view, cameraPos);
    renderer.DrawGround(projection, view);
    for (auto& turret : turrets) renderer.DrawTurret(turret, projection, view);
    renderer.DrawPlayer(player1, projection, view);
    renderer.DrawPlayer(player2, projection, view);
    for (auto& enemy : enemies) renderer.DrawEnemy(enemy, projection, view);
    for (auto& bullet : bullets) renderer.DrawBullet(bullet, projection, view);

    // 3. UI PASS
    if (gameState == PLAYING) {
        RenderHUD();
    } else if (gameState == GAME_OVER) {
        renderer.DrawGameOverScreen(isWin, score, stars, currentWave);
    } else if (gameState == MENU) {
        renderer.DrawMenuScreen();
    } else if (gameState == HOW_TO_PLAY) {
        renderer.DrawHowToPlayScreen();
    }

    renderer.EndFrame();
}

void Game::RenderHUD() {
    // Render HUD elements
    // - P1 health bar
    // - P2 health bar
    // - Timer
    // - Turret status
    // - Score
    renderer.DrawHUD(player1, player2, gameTimer, WAVE_DURATION, 
                    turretHealth, score, currentWave);
}

void Game::Shutdown() {
    renderer.Shutdown();
}

void Game::ResetGame() {
    gameTimer = 0;
    waveTimer = 0;
    currentWave = 1;
    maxEnemies = 8;
    spawnDelay = 1.5f;
    spawnedCount = 0;
    spawnTimer = 0;
    score = 0;
    stars = 0;
    isWin = false;

    player1.Reset(P1_SPAWN.x, P1_SPAWN.y);
    player2.Reset(P2_SPAWN.x, P2_SPAWN.y);

    enemies.clear();
    bullets.clear();
    turrets.clear(); // Hapus turret lama

    // Re-initialize turret agar kembali aktif dengan posisi berjarak
    glm::vec3 turretPositions[3] = {
        glm::vec3(0.0f, 0.0f, 10.0f),
        glm::vec3(-12.0f, 0.0f, -6.0f),
        glm::vec3(12.0f, 0.0f, -6.0f)
    };

    for (int i = 0; i < 3; i++) {
        turrets.push_back(Turret(turretPositions[i], 100));
        turretHealth[i] = 100;
    }
}

void Game::Resize(int width, int height) {
    screenWidth = width;
    screenHeight = height;
    renderer.Resize(width, height);
}
