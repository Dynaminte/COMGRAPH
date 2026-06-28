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

    void BeginShadowPass(glm::vec3 lightPosition);
    void BeginMainPass(glm::mat4 projection, glm::mat4 view, glm::vec3 cameraPos);
    void EndFrame();

    // Drawing functions
    void DrawGround(glm::mat4 projection, glm::mat4 view);
    void DrawPlayer(const Player& player, glm::mat4 projection, glm::mat4 view);
    void DrawEnemy(const Enemy& enemy, glm::mat4 projection, glm::mat4 view);
    void DrawBullet(const Bullet& bullet, glm::mat4 projection, glm::mat4 view);
    void DrawTurret(const Turret& turret, glm::mat4 projection, glm::mat4 view);
    // Removed fake DrawShadows function

    // HUD
    void DrawHUD(const Player& p1, const Player& p2, float gameTimer, 
                float waveDuration, int* turretHealth, int score, int wave);
    void DrawGameOverScreen(bool isWin, int score, int stars, int wave);
    void DrawMenuScreen();
    void DrawHowToPlayScreen();

private:
    int screenWidth, screenHeight;
    GLuint shaderProgram;
    GLuint shadowShaderProgram; // Used for depth map generation
    GLuint depthMapFBO;
    GLuint depthMap;
    const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;

    glm::mat4 lightSpaceMatrix;
    glm::vec3 lightPos;
    bool isShadowPass;

    // VAO/VBO untuk shapes
    GLuint cubeVAO, sphereVAO, cylinderVAO, quadVAO;
    GLuint floorTexture;
    GLuint tankTexture;
    GLuint turretTexture;
    unsigned int cubeFaceCount, sphereFaceCount, cylinderFaceCount, quadFaceCount;

    void SetupShapes();
    void DrawCube(glm::mat4 model, glm::vec3 color);
    void DrawCubeTextured(glm::mat4 model, GLuint texture);
    void DrawCylinderTextured(glm::mat4 model, GLuint texture);
    void DrawSphere(glm::mat4 model, glm::vec3 color, float radius = 1.0f);
    void DrawCylinder(glm::mat4 model, glm::vec3 color);
    void DrawQuad(glm::mat4 model, glm::vec3 color);
    void DrawText2D(const std::string& text, float x, float y, float scale, glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f));

    GLuint CompileShader(const char* source, GLenum shaderType);
    GLuint CreateShaderProgram(const char* vertexSrc, const char* fragmentSrc);
};

#endif // RENDERER_H
