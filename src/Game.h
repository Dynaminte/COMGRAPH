#ifndef GAME_H
#define GAME_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include "Player.h"
#include "Enemy_Bullet_Turret.h"
#include "Renderer.h"

class Game {
public:
    Game(int width, int height);
    ~Game();

    bool Initialize();
    void ProcessInput(GLFWwindow* window);
    void Update(float deltaTime);
    void Render();
    void Shutdown();
    void Resize(int width, int height);

private:
    // Screen
    int screenWidth, screenHeight;

    // Game state
    enum GameState { MENU, PLAYING, GAME_OVER, HOW_TO_PLAY };
    GameState gameState;
    float gameTimer;
    float waveTimer;
    int currentWave;
    int turretHealth[3]; // health dari 3 turret
    int turretCount;

    // Players
    Player player1, player2;

    // Game objects
    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<Turret> turrets;

    // Renderer
    Renderer renderer;

    // Spawn control
    float spawnTimer;
    float spawnDelay;
    int maxEnemies;
    int spawnedCount;

    // HUD
    int score;
    int stars;
    bool isWin;

    // Game logic
    void UpdateGameplay(float deltaTime);
    void UpdateMenu(float deltaTime);
    void UpdateGameOver(float deltaTime);
    void SpawnEnemy();
    void CheckCollisions();
    void RenderHUD();
    void ResetGame();
};

#endif // GAME_H
