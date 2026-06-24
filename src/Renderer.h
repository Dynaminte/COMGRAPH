#ifndef RENDERER_H
#define RENDERER_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>


class Player;
class Enemy;
class Bullet;
class Turret;

class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();

    bool Initialize();
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    // Drawing functions
    void DrawGround(glm::mat4 projection, glm::mat4 view);
    void DrawPlayer(const Player& player, glm::mat4 projection, glm::mat4 view);
    void DrawEnemy(const Enemy& enemy, glm::mat4 projection, glm::mat4 view);
    void DrawBullet(const Bullet& bullet, glm::mat4 projection, glm::mat4 view);
    void DrawTurret(const Turret& turret, glm::mat4 projection, glm::mat4 view);
    void DrawShadows(const Player& p1, const Player& p2, 
                    const std::vector<Enemy>& enemies,
                    const std::vector<Turret>& turrets,
                    glm::mat4 projection, glm::mat4 view);

    // HUD
    void DrawHUD(const Player& p1, const Player& p2, float gameTimer, 
                float waveDuration, int* turretHealth, int score, int wave);
    void DrawGameOverScreen(int score, int stars, int wave);

private:
    int screenWidth, screenHeight;
    GLuint shaderProgram;
    GLuint shadowShaderProgram;

    // VAO/VBO untuk shapes
    GLuint cubeVAO, sphereVAO, cylinderVAO, quadVAO;
    unsigned int cubeFaceCount, sphereFaceCount, cylinderFaceCount, quadFaceCount;

    void SetupShapes();
    void DrawCube(glm::mat4 model, glm::vec3 color);
    void DrawSphere(glm::mat4 model, glm::vec3 color, float radius = 1.0f);
    void DrawCylinder(glm::mat4 model, glm::vec3 color);
    void DrawQuad(glm::mat4 model, glm::vec3 color);
    void DrawText2D(const std::string& text, float x, float y, float scale);

    GLuint CompileShader(const char* source, GLenum shaderType);
    GLuint CreateShaderProgram(const char* vertexSrc, const char* fragmentSrc);
};

#endif // RENDERER_H
