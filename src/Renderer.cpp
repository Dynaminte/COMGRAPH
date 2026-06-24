#include "Renderer.h"
#include "Player.h"
#include "Enemy_Bullet_Turret.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <fstream>
#include <sstream>

// Simple vertex shader source
const char* vertexShaderSource = R"(
#version 410 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;

void main()
{
    FragPos = vec3(model * vec4(position, 1.0));
    Normal = mat3(transpose(inverse(model))) * normal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

// Simple fragment shader source
const char* fragmentShaderSource = R"(
#version 410 core
in vec3 FragPos;
in vec3 Normal;

uniform vec3 objectColor;
uniform vec3 lightPos;
uniform vec3 viewPos;

out vec4 FragColor;

void main()
{
    // Ambient
    vec3 ambient = 0.3 * objectColor;
    
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * objectColor;
    
    // Specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = 0.5 * spec * vec3(1.0);
    
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}
)";

Renderer::Renderer(int width, int height)
    : screenWidth(width), screenHeight(height),
      shaderProgram(0), shadowShaderProgram(0),
      cubeVAO(0), sphereVAO(0), cylinderVAO(0), quadVAO(0),
      cubeFaceCount(0), sphereFaceCount(0), cylinderFaceCount(0), quadFaceCount(0) {}

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize() {
    // Compile shaders
    shaderProgram = CreateShaderProgram(vertexShaderSource, fragmentShaderSource);
    if (shaderProgram == 0) {
        std::cerr << "Failed to create shader program!" << std::endl;
        return false;
    }

    // Setup 3D shapes
    SetupShapes();

    return true;
}

GLuint Renderer::CompileShader(const char* source, GLenum shaderType) {
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    // Check compilation
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
        return 0;
    }

    return shader;
}

GLuint Renderer::CreateShaderProgram(const char* vertexSrc, const char* fragmentSrc) {
    GLuint vertexShader = CompileShader(vertexSrc, GL_VERTEX_SHADER);
    GLuint fragmentShader = CompileShader(fragmentSrc, GL_FRAGMENT_SHADER);

    if (vertexShader == 0 || fragmentShader == 0) {
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Check linking
    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
        return 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

void Renderer::SetupShapes() {
    // 1. CUBE setup
    float cubeVertices[] = {
        // Position           // Normals
        // Back face
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,         
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        // Front face
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        // Left face
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        // Right face
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,         
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,     
        // Bottom face
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
        // Top face
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
    };
    cubeFaceCount = 36;
    GLuint cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
    glDeleteBuffers(1, &cubeVBO);

    // 2. QUAD setup (for ground and UI)
    float quadVertices[] = {
        // Position           // Normals
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,
         0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f
    };
    quadFaceCount = 6;
    GLuint quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
    glDeleteBuffers(1, &quadVBO);

    // 3. SPHERE setup
    std::vector<float> sphereVertices;
    const int sectors = 20;
    const int stacks = 20;
    const float PI = 3.14159265359f;

    for (int i = 0; i < stacks; ++i) {
        float lat0 = PI * (-0.5f + (float)i / stacks);
        float z0  = sin(lat0);
        float zr0 = cos(lat0);

        float lat1 = PI * (-0.5f + (float)(i + 1) / stacks);
        float z1  = sin(lat1);
        float zr1 = cos(lat1);

        for (int j = 0; j < sectors; ++j) {
            float lng0 = 2 * PI * (float)j / sectors;
            float x0 = cos(lng0);
            float y0 = sin(lng0);

            float lng1 = 2 * PI * (float)(j + 1) / sectors;
            float x1 = cos(lng1);
            float y1 = sin(lng1);

            // Triangle 1
            sphereVertices.push_back(x0 * zr0); sphereVertices.push_back(y0 * zr0); sphereVertices.push_back(z0);
            sphereVertices.push_back(x0 * zr0); sphereVertices.push_back(y0 * zr0); sphereVertices.push_back(z0);

            sphereVertices.push_back(x1 * zr0); sphereVertices.push_back(y1 * zr0); sphereVertices.push_back(z0);
            sphereVertices.push_back(x1 * zr0); sphereVertices.push_back(y1 * zr0); sphereVertices.push_back(z0);

            sphereVertices.push_back(x1 * zr1); sphereVertices.push_back(y1 * zr1); sphereVertices.push_back(z1);
            sphereVertices.push_back(x1 * zr1); sphereVertices.push_back(y1 * zr1); sphereVertices.push_back(z1);

            // Triangle 2
            sphereVertices.push_back(x0 * zr0); sphereVertices.push_back(y0 * zr0); sphereVertices.push_back(z0);
            sphereVertices.push_back(x0 * zr0); sphereVertices.push_back(y0 * zr0); sphereVertices.push_back(z0);

            sphereVertices.push_back(x1 * zr1); sphereVertices.push_back(y1 * zr1); sphereVertices.push_back(z1);
            sphereVertices.push_back(x1 * zr1); sphereVertices.push_back(y1 * zr1); sphereVertices.push_back(z1);

            sphereVertices.push_back(x0 * zr1); sphereVertices.push_back(y0 * zr1); sphereVertices.push_back(z1);
            sphereVertices.push_back(x0 * zr1); sphereVertices.push_back(y0 * zr1); sphereVertices.push_back(z1);
        }
    }
    sphereFaceCount = sphereVertices.size() / 6;
    GLuint sphereVBO;
    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &sphereVBO);
    glBindVertexArray(sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, sphereVertices.size() * sizeof(float), sphereVertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
    glDeleteBuffers(1, &sphereVBO);

    // 4. CYLINDER setup
    std::vector<float> cylinderVertices;
    const int cylSectors = 20;
    const float cylHeight = 1.0f;
    const float cylRadius = 0.5f;

    // Side wall
    for (int i = 0; i < cylSectors; ++i) {
        float angle0 = 2 * PI * (float)i / cylSectors;
        float angle1 = 2 * PI * (float)(i + 1) / cylSectors;

        float x0 = cos(angle0) * cylRadius;
        float z0 = sin(angle0) * cylRadius;
        float x1 = cos(angle1) * cylRadius;
        float z1 = sin(angle1) * cylRadius;

        // Triangle 1
        cylinderVertices.push_back(x0); cylinderVertices.push_back(-cylHeight*0.5f); cylinderVertices.push_back(z0);
        cylinderVertices.push_back(cos(angle0)); cylinderVertices.push_back(0.0f); cylinderVertices.push_back(sin(angle0));

        cylinderVertices.push_back(x1); cylinderVertices.push_back(-cylHeight*0.5f); cylinderVertices.push_back(z1);
        cylinderVertices.push_back(cos(angle1)); cylinderVertices.push_back(0.0f); cylinderVertices.push_back(sin(angle1));

        cylinderVertices.push_back(x1); cylinderVertices.push_back(cylHeight*0.5f); cylinderVertices.push_back(z1);
        cylinderVertices.push_back(cos(angle1)); cylinderVertices.push_back(0.0f); cylinderVertices.push_back(sin(angle1));

        // Triangle 2
        cylinderVertices.push_back(x0); cylinderVertices.push_back(-cylHeight*0.5f); cylinderVertices.push_back(z0);
        cylinderVertices.push_back(cos(angle0)); cylinderVertices.push_back(0.0f); cylinderVertices.push_back(sin(angle0));

        cylinderVertices.push_back(x1); cylinderVertices.push_back(cylHeight*0.5f); cylinderVertices.push_back(z1);
        cylinderVertices.push_back(cos(angle1)); cylinderVertices.push_back(0.0f); cylinderVertices.push_back(sin(angle1));

        cylinderVertices.push_back(x0); cylinderVertices.push_back(cylHeight*0.5f); cylinderVertices.push_back(z0);
        cylinderVertices.push_back(cos(angle0)); cylinderVertices.push_back(0.0f); cylinderVertices.push_back(sin(angle0));
    }

    // Top cap
    for (int i = 0; i < cylSectors; ++i) {
        float angle0 = 2 * PI * (float)i / cylSectors;
        float angle1 = 2 * PI * (float)(i + 1) / cylSectors;

        float x0 = cos(angle0) * cylRadius;
        float z0 = sin(angle0) * cylRadius;
        float x1 = cos(angle1) * cylRadius;
        float z1 = sin(angle1) * cylRadius;

        cylinderVertices.push_back(0.0f); cylinderVertices.push_back(cylHeight*0.5f); cylinderVertices.push_back(0.0f);
        cylinderVertices.push_back(0.0f); cylinderVertices.push_back(1.0f); cylinderVertices.push_back(0.0f);

        cylinderVertices.push_back(x0); cylinderVertices.push_back(cylHeight*0.5f); cylinderVertices.push_back(z0);
        cylinderVertices.push_back(0.0f); cylinderVertices.push_back(1.0f); cylinderVertices.push_back(0.0f);

        cylinderVertices.push_back(x1); cylinderVertices.push_back(cylHeight*0.5f); cylinderVertices.push_back(z1);
        cylinderVertices.push_back(0.0f); cylinderVertices.push_back(1.0f); cylinderVertices.push_back(0.0f);
    }

    // Bottom cap
    for (int i = 0; i < cylSectors; ++i) {
        float angle0 = 2 * PI * (float)i / cylSectors;
        float angle1 = 2 * PI * (float)(i + 1) / cylSectors;

        float x0 = cos(angle0) * cylRadius;
        float z0 = sin(angle0) * cylRadius;
        float x1 = cos(angle1) * cylRadius;
        float z1 = sin(angle1) * cylRadius;

        cylinderVertices.push_back(0.0f); cylinderVertices.push_back(-cylHeight*0.5f); cylinderVertices.push_back(0.0f);
        cylinderVertices.push_back(0.0f); cylinderVertices.push_back(-1.0f); cylinderVertices.push_back(0.0f);

        cylinderVertices.push_back(x1); cylinderVertices.push_back(-cylHeight*0.5f); cylinderVertices.push_back(z1);
        cylinderVertices.push_back(0.0f); cylinderVertices.push_back(-1.0f); cylinderVertices.push_back(0.0f);

        cylinderVertices.push_back(x0); cylinderVertices.push_back(-cylHeight*0.5f); cylinderVertices.push_back(z0);
        cylinderVertices.push_back(0.0f); cylinderVertices.push_back(-1.0f); cylinderVertices.push_back(0.0f);
    }
    cylinderFaceCount = cylinderVertices.size() / 6;
    GLuint cylinderVBO;
    glGenVertexArrays(1, &cylinderVAO);
    glGenBuffers(1, &cylinderVBO);
    glBindVertexArray(cylinderVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cylinderVBO);
    glBufferData(GL_ARRAY_BUFFER, cylinderVertices.size() * sizeof(float), cylinderVertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
    glDeleteBuffers(1, &cylinderVBO);
}

void Renderer::BeginFrame() {
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::EndFrame() {
    // Post-processing jika ada
}

// --- Local Helpers for 2D UI and Font Rendering ---
struct Point2D { float x, y; };
struct Line2D { Point2D p1, p2; };

static std::vector<Line2D> getCharStrokes(char c) {
    std::vector<Line2D> lines;
    c = toupper(c);
    
    Point2D tl = {0.0f, 0.0f};
    Point2D tc = {0.5f, 0.0f};
    Point2D tr = {1.0f, 0.0f};
    Point2D ml = {0.0f, 0.5f};
    Point2D mc = {0.5f, 0.5f};
    Point2D mr = {1.0f, 0.5f};
    Point2D bl = {0.0f, 1.0f};
    Point2D bc = {0.5f, 1.0f};
    Point2D br = {1.0f, 1.0f};

    switch(c) {
        case '0':
            lines.push_back({tl, tr}); lines.push_back({tr, br});
            lines.push_back({br, bl}); lines.push_back({bl, tl});
            lines.push_back({bl, tr});
            break;
        case '1':
            lines.push_back({tc, bc}); lines.push_back({tl, tc}); lines.push_back({bl, br});
            break;
        case '2':
            lines.push_back({tl, tr}); lines.push_back({tr, mr});
            lines.push_back({mr, ml}); lines.push_back({ml, bl});
            lines.push_back({bl, br});
            break;
        case '3':
            lines.push_back({tl, tr}); lines.push_back({tr, br});
            lines.push_back({br, bl}); lines.push_back({ml, mr});
            break;
        case '4':
            lines.push_back({tl, ml}); lines.push_back({ml, mr});
            lines.push_back({tr, br});
            break;
        case '5':
            lines.push_back({tr, tl}); lines.push_back({tl, ml});
            lines.push_back({ml, mr}); lines.push_back({mr, br});
            lines.push_back({br, bl});
            break;
        case '6':
            lines.push_back({tr, tl}); lines.push_back({tl, bl});
            lines.push_back({bl, br}); lines.push_back({br, mr});
            lines.push_back({mr, ml});
            break;
        case '7':
            lines.push_back({tl, tr}); lines.push_back({tr, br});
            break;
        case '8':
            lines.push_back({tl, tr}); lines.push_back({tr, br});
            lines.push_back({br, bl}); lines.push_back({bl, tl});
            lines.push_back({ml, mr});
            break;
        case '9':
            lines.push_back({ml, tl}); lines.push_back({tl, tr});
            lines.push_back({tr, br}); lines.push_back({br, bl});
            lines.push_back({mr, ml});
            break;
        case 'A':
            lines.push_back({bl, tl}); lines.push_back({tl, tr});
            lines.push_back({tr, br}); lines.push_back({ml, mr});
            break;
        case 'B':
            lines.push_back({bl, tl}); lines.push_back({tl, tc});
            lines.push_back({tc, mc}); lines.push_back({mc, ml});
            lines.push_back({mc, bc}); lines.push_back({bc, bl});
            break;
        case 'C':
            lines.push_back({tr, tl}); lines.push_back({tl, bl});
            lines.push_back({bl, br});
            break;
        case 'D':
            lines.push_back({bl, tl}); lines.push_back({tl, tc});
            lines.push_back({tc, bc}); lines.push_back({bc, bl});
            break;
        case 'E':
            lines.push_back({tr, tl}); lines.push_back({tl, bl});
            lines.push_back({bl, br}); lines.push_back({ml, mc});
            break;
        case 'F':
            lines.push_back({tr, tl}); lines.push_back({tl, bl});
            lines.push_back({ml, mc});
            break;
        case 'G':
            lines.push_back({tr, tl}); lines.push_back({tl, bl});
            lines.push_back({bl, br}); lines.push_back({br, mr});
            lines.push_back({mr, mc});
            break;
        case 'H':
            lines.push_back({tl, bl}); lines.push_back({tr, br});
            lines.push_back({ml, mr});
            break;
        case 'I':
            lines.push_back({tl, tr}); lines.push_back({tc, bc});
            lines.push_back({bl, br});
            break;
        case 'J':
            lines.push_back({tr, br}); lines.push_back({br, bl});
            lines.push_back({bl, ml});
            break;
        case 'K':
            lines.push_back({tl, bl}); lines.push_back({ml, tr});
            lines.push_back({ml, br});
            break;
        case 'L':
            lines.push_back({tl, bl}); lines.push_back({bl, br});
            break;
        case 'M':
            lines.push_back({bl, tl}); lines.push_back({tl, mc});
            lines.push_back({mc, tr}); lines.push_back({tr, br});
            break;
        case 'N':
            lines.push_back({bl, tl}); lines.push_back({tl, br});
            lines.push_back({br, tr});
            break;
        case 'O':
            lines.push_back({tl, tr}); lines.push_back({tr, br});
            lines.push_back({br, bl}); lines.push_back({bl, tl});
            break;
        case 'P':
            lines.push_back({bl, tl}); lines.push_back({tl, tr});
            lines.push_back({tr, mr}); lines.push_back({mr, ml});
            break;
        case 'Q':
            lines.push_back({tl, tr}); lines.push_back({tr, br});
            lines.push_back({br, bl}); lines.push_back({bl, tl});
            lines.push_back({mc, br});
            break;
        case 'R':
            lines.push_back({bl, tl}); lines.push_back({tl, tr});
            lines.push_back({tr, mr}); lines.push_back({mr, ml});
            lines.push_back({ml, br});
            break;
        case 'S':
            lines.push_back({tr, tl}); lines.push_back({tl, ml});
            lines.push_back({ml, mr}); lines.push_back({mr, br});
            lines.push_back({br, bl});
            break;
        case 'T':
            lines.push_back({tl, tr}); lines.push_back({tc, bc});
            break;
        case 'U':
            lines.push_back({tl, bl}); lines.push_back({bl, br});
            lines.push_back({br, tr});
            break;
        case 'V':
            lines.push_back({tl, bc}); lines.push_back({bc, tr});
            break;
        case 'W':
            lines.push_back({tl, bl}); lines.push_back({bl, mc});
            lines.push_back({mc, br}); lines.push_back({br, tr});
            break;
        case 'X':
            lines.push_back({tl, br}); lines.push_back({tr, bl});
            break;
        case 'Y':
            lines.push_back({tl, mc}); lines.push_back({tr, mc});
            lines.push_back({mc, bc});
            break;
        case 'Z':
            lines.push_back({tl, tr}); lines.push_back({tr, bl});
            lines.push_back({bl, br});
            break;
        case ':':
            lines.push_back({{0.5f, 0.25f}, {0.5f, 0.3f}});
            lines.push_back({{0.5f, 0.7f}, {0.5f, 0.75f}});
            break;
        case '-':
            lines.push_back({ml, mr});
            break;
        case ' ':
            break;
        default:
            lines.push_back({tl, tr}); lines.push_back({tr, mr});
            lines.push_back({mr, mc}); lines.push_back({mc, bc});
            break;
    }
    return lines;
}

static void drawLineList(GLuint shaderProgram, const std::vector<float>& vertices, glm::vec3 color) {
    GLuint tempVAO, tempVBO;
    glGenVertexArrays(1, &tempVAO);
    glGenBuffers(1, &tempVBO);
    
    glBindVertexArray(tempVAO);
    glBindBuffer(GL_ARRAY_BUFFER, tempVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    
    glUseProgram(shaderProgram);
    glm::mat4 identity = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(identity));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(identity));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(identity));
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), color.x, color.y, color.z);
    
    glLineWidth(2.5f);
    glDrawArrays(GL_LINES, 0, vertices.size() / 6);
    
    glBindVertexArray(0);
    glDeleteBuffers(1, &tempVBO);
    glDeleteVertexArrays(1, &tempVAO);
}

static void drawBar2D(GLuint shaderProgram, GLuint quadVAO, unsigned int quadFaceCount, float x, float y, float w, float h, glm::vec3 color, float screenWidth, float screenHeight) {
    glDisable(GL_DEPTH_TEST);
    
    float nw = w / screenWidth * 2.0f;
    float nh = h / screenHeight * 2.0f;
    float nx = (x + w * 0.5f) / screenWidth * 2.0f - 1.0f;
    float ny = 1.0f - (y + h * 0.5f) / screenHeight * 2.0f;
    
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(nx, ny, 0.0f));
    model = glm::scale(model, glm::vec3(nw, nh, 1.0f));
    
    glUseProgram(shaderProgram);
    glm::mat4 identity = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(identity));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(identity));
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), color.x, color.y, color.z);
    
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, quadFaceCount);
    glBindVertexArray(0);
    
    glEnable(GL_DEPTH_TEST);
}

void Renderer::DrawGround(glm::mat4 projection, glm::mat4 view) {
    glUseProgram(shaderProgram);
    
    glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), 10.0f, 20.0f, 15.0f);
    glUniform3f(glGetUniformLocation(shaderProgram, "viewPos"), 0.0f, 15.0f, 20.0f);
    
    // Horizontal ground is rotated X-Y plane flat onto X-Z
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::scale(model, glm::vec3(60.0f, 60.0f, 1.0f));
    
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    DrawQuad(model, glm::vec3(0.2f, 0.4f, 0.1f));
}

void Renderer::DrawPlayer(const Player& player, glm::mat4 projection, glm::mat4 view) {
    glUseProgram(shaderProgram);

    glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), 10.0f, 20.0f, 15.0f);
    glUniform3f(glGetUniformLocation(shaderProgram, "viewPos"), 0.0f, 15.0f, 20.0f);

    glm::vec3 pos = player.GetPosition();
    glm::vec3 rot = player.GetRotation();

    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
    model = glm::rotate(model, rot.y, glm::vec3(0.0f, 1.0f, 0.0f));

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glm::vec3 color = player.IsPlayer1() ? glm::vec3(0.2f, 0.8f, 0.2f) 
                                         : glm::vec3(0.2f, 0.2f, 0.8f);

    // Draw tank body (cube)
    glm::mat4 bodyModel = glm::scale(model, glm::vec3(1.5f, 0.6f, 2.0f));
    DrawCube(bodyModel, color);

    // Draw turret (cylinder)
    glm::mat4 turretModel = glm::translate(model, glm::vec3(0.0f, 0.5f, 0.0f));
    turretModel = glm::scale(turretModel, glm::vec3(0.8f, 0.5f, 0.8f));
    DrawCylinder(turretModel, color * 0.8f);

    // Draw barrel (cylinder extended forward)
    glm::mat4 barrelModel = glm::translate(model, glm::vec3(0.0f, 0.5f, -0.8f));
    barrelModel = glm::rotate(barrelModel, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)); // point forward
    barrelModel = glm::scale(barrelModel, glm::vec3(0.2f, 1.2f, 0.2f));
    DrawCylinder(barrelModel, glm::vec3(0.3f, 0.3f, 0.3f));
}

void Renderer::DrawEnemy(const Enemy& enemy, glm::mat4 projection, glm::mat4 view) {
    glUseProgram(shaderProgram);

    glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), 10.0f, 20.0f, 15.0f);
    glUniform3f(glGetUniformLocation(shaderProgram, "viewPos"), 0.0f, 15.0f, 20.0f);

    glm::vec3 pos = enemy.GetPosition();
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
    model = glm::rotate(model, enemy.GetRotation(), glm::vec3(0.0f, 1.0f, 0.0f));

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    // Colors
    glm::vec3 planeColor(0.85f, 0.15f, 0.15f); // Merah untuk bodi pesawat
    glm::vec3 wingColor(0.9f, 0.9f, 0.9f);     // Putih untuk sayap
    glm::vec3 propellerColor(0.9f, 0.7f, 0.1f); // Kuning untuk moncong

    // 1. Fuselage (Badan Pesawat) - Rotated 90 degrees around X to lie flat horizontally
    glm::mat4 fuselageModel = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    fuselageModel = glm::scale(fuselageModel, glm::vec3(0.35f, 1.6f, 0.35f));
    DrawCylinder(fuselageModel, planeColor);

    // 2. Main Wings (Sayap Utama)
    glm::mat4 wingsModel = glm::translate(model, glm::vec3(0.0f, 0.0f, -0.2f));
    wingsModel = glm::scale(wingsModel, glm::vec3(2.6f, 0.06f, 0.5f));
    DrawCube(wingsModel, wingColor);

    // 3. Tail Wings (Sayap Ekor Horizontal)
    glm::mat4 tailWingsModel = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.7f));
    tailWingsModel = glm::scale(tailWingsModel, glm::vec3(1.0f, 0.05f, 0.25f));
    DrawCube(tailWingsModel, wingColor);

    // 4. Vertical Stabilizer (Sayap Ekor Vertikal)
    glm::mat4 tailFinModel = glm::translate(model, glm::vec3(0.0f, 0.25f, 0.7f));
    tailFinModel = glm::scale(tailFinModel, glm::vec3(0.05f, 0.4f, 0.2f));
    DrawCube(tailFinModel, planeColor);

    // 5. Spinner & Propeller (Moncong Pesawat yang berputar)
    glm::mat4 spinnerModel = glm::translate(model, glm::vec3(0.0f, 0.0f, -0.82f));
    spinnerModel = glm::rotate(spinnerModel, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    spinnerModel = glm::scale(spinnerModel, glm::vec3(0.2f, 0.15f, 0.2f));
    DrawCylinder(spinnerModel, propellerColor);

    // Propeller blades (spinning animation using time)
    float propAngle = (float)glfwGetTime() * 25.0f;
    glm::mat4 propModel = glm::translate(model, glm::vec3(0.0f, 0.0f, -0.85f));
    propModel = glm::rotate(propModel, propAngle, glm::vec3(0.0f, 0.0f, 1.0f));

    // Blade 1
    glm::mat4 blade1Model = glm::scale(propModel, glm::vec3(0.06f, 0.8f, 0.02f));
    DrawCube(blade1Model, glm::vec3(0.2f, 0.2f, 0.2f));

    // Blade 2
    glm::mat4 blade2Model = glm::scale(propModel, glm::vec3(0.8f, 0.06f, 0.02f));
    DrawCube(blade2Model, glm::vec3(0.2f, 0.2f, 0.2f));

    // 6. Draw Health Bar diatas pesawat
    if (enemy.GetHealth() > 0) {
        float hpPercent = (float)enemy.GetHealth() / 3.0f; // max HP = 3
        
        // Background bar (hitam)
        glm::mat4 hpBg = glm::translate(model, glm::vec3(0.0f, 0.9f, 0.0f));
        hpBg = glm::scale(hpBg, glm::vec3(1.0f, 0.08f, 0.08f));
        DrawCube(hpBg, glm::vec3(0.1f, 0.1f, 0.1f));

        // Foreground bar (hijau)
        glm::mat4 hpFg = glm::translate(model, glm::vec3(-0.5f * (1.0f - hpPercent), 0.9f, 0.01f));
        hpFg = glm::scale(hpFg, glm::vec3(1.0f * hpPercent, 0.08f, 0.08f));
        DrawCube(hpFg, glm::vec3(0.2f, 0.8f, 0.2f));
    }
}

void Renderer::DrawBullet(const Bullet& bullet, glm::mat4 projection, glm::mat4 view) {
    glUseProgram(shaderProgram);

    glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), 10.0f, 20.0f, 15.0f);
    glUniform3f(glGetUniformLocation(shaderProgram, "viewPos"), 0.0f, 15.0f, 20.0f);

    glm::vec3 pos = bullet.GetPosition();
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
    model = glm::scale(model, glm::vec3(0.3f, 0.3f, 0.3f));

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glm::vec3 color = bullet.IsEnemyBullet() ? glm::vec3(1.0f, 0.15f, 0.15f) : glm::vec3(1.0f, 1.0f, 0.0f);
    DrawSphere(model, color, 0.3f);
}

void Renderer::DrawTurret(const Turret& turret, glm::mat4 projection, glm::mat4 view) {
    glUseProgram(shaderProgram);

    glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), 10.0f, 20.0f, 15.0f);
    glUniform3f(glGetUniformLocation(shaderProgram, "viewPos"), 0.0f, 15.0f, 20.0f);

    glm::vec3 pos = turret.GetPosition();
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glm::vec3 color = turret.IsAlive() ? glm::vec3(0.85f, 0.75f, 0.2f) 
                                       : glm::vec3(0.3f, 0.3f, 0.3f);
    glm::vec3 metalColor(0.25f, 0.25f, 0.25f);

    // 1. Sci-fi Base Border Ring (flat on ground at y = -0.24f) - scaled larger
    glm::mat4 ringModel = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, -0.24f, pos.z));
    ringModel = glm::rotate(ringModel, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    ringModel = glm::scale(ringModel, glm::vec3(3.2f, 3.2f, 1.0f));
    if (turret.IsAlive()) {
        DrawQuad(ringModel, glm::vec3(0.9f, 0.8f, 0.1f)); // Bright yellow-gold for active
    } else {
        DrawQuad(ringModel, glm::vec3(0.4f, 0.15f, 0.15f)); // Dark red for destroyed
    }

    // 2. Base plate (Cube) - scaled larger (2.5f, 0.6f, 2.5f)
    glm::mat4 baseModel = glm::scale(model, glm::vec3(2.5f, 0.6f, 2.5f));
    DrawCube(baseModel, color);

    // 3. Four Corner Pillars/Pads for structural reinforcement (Cubes)
    if (turret.IsAlive()) {
        float offset = 1.05f;
        glm::mat4 p1 = glm::translate(model, glm::vec3(-offset, 0.4f, -offset));
        DrawCube(glm::scale(p1, glm::vec3(0.4f, 0.8f, 0.4f)), metalColor);

        glm::mat4 p2 = glm::translate(model, glm::vec3(offset, 0.4f, -offset));
        DrawCube(glm::scale(p2, glm::vec3(0.4f, 0.8f, 0.4f)), metalColor);

        glm::mat4 p3 = glm::translate(model, glm::vec3(-offset, 0.4f, offset));
        DrawCube(glm::scale(p3, glm::vec3(0.4f, 0.8f, 0.4f)), metalColor);

        glm::mat4 p4 = glm::translate(model, glm::vec3(offset, 0.4f, offset));
        DrawCube(glm::scale(p4, glm::vec3(0.4f, 0.8f, 0.4f)), metalColor);
    }

    // 4. Sub-base plate (Cube) - scaled larger
    glm::mat4 subBaseModel = glm::translate(model, glm::vec3(0.0f, 0.35f, 0.0f));
    subBaseModel = glm::scale(subBaseModel, glm::vec3(1.8f, 0.3f, 1.8f));
    DrawCube(subBaseModel, color * 0.8f);

    // 5. Dome body (Cylinder) - scaled larger
    glm::mat4 bodyModel = glm::translate(model, glm::vec3(0.0f, 0.8f, 0.0f));
    bodyModel = glm::scale(bodyModel, glm::vec3(1.4f, 0.8f, 1.4f));
    DrawCylinder(bodyModel, color * 0.95f);

    // 6. Dome Side shields/armor plates (Cubes)
    if (turret.IsAlive()) {
        glm::mat4 leftShield = glm::translate(model, glm::vec3(-0.85f, 0.9f, 0.0f));
        leftShield = glm::scale(leftShield, glm::vec3(0.2f, 0.8f, 1.3f));
        DrawCube(leftShield, color * 0.7f);

        glm::mat4 rightShield = glm::translate(model, glm::vec3(0.85f, 0.9f, 0.0f));
        rightShield = glm::scale(rightShield, glm::vec3(0.2f, 0.8f, 1.3f));
        DrawCube(rightShield, color * 0.7f);
    }

    // 7. Spinning radar array on top
    if (turret.IsAlive()) {
        // Radar Stand
        glm::mat4 radarStand = glm::translate(model, glm::vec3(0.0f, 1.35f, 0.4f));
        radarStand = glm::scale(radarStand, glm::vec3(0.12f, 0.4f, 0.12f));
        DrawCylinder(radarStand, metalColor);

        // Radar Dish (spins around Y-axis)
        float radarAngle = (float)glfwGetTime() * 4.0f;
        glm::mat4 radarDish = glm::translate(model, glm::vec3(0.0f, 1.6f, 0.4f));
        radarDish = glm::rotate(radarDish, radarAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        radarDish = glm::scale(radarDish, glm::vec3(0.9f, 0.15f, 0.3f));
        DrawCube(radarDish, glm::vec3(0.7f, 0.7f, 0.7f));
    }

    // 8. Heavy Dual Anti-Aircraft Gun Barrels
    if (turret.IsAlive()) {
        // Left Heavy Barrel
        glm::mat4 barrel1 = glm::translate(model, glm::vec3(-0.4f, 0.9f, -0.6f));
        barrel1 = glm::rotate(barrel1, glm::radians(55.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        barrel1 = glm::scale(barrel1, glm::vec3(0.16f, 2.0f, 0.16f));
        DrawCylinder(barrel1, metalColor);

        // Right Heavy Barrel
        glm::mat4 barrel2 = glm::translate(model, glm::vec3(0.4f, 0.9f, -0.6f));
        barrel2 = glm::rotate(barrel2, glm::radians(55.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        barrel2 = glm::scale(barrel2, glm::vec3(0.16f, 2.0f, 0.16f));
        DrawCylinder(barrel2, metalColor);
    }

    // 9. Floating health bar above the massive turret (raised Y coordinate to 2.5f)
    if (turret.IsAlive() && turret.GetHealth() > 0) {
        float hpPercent = (float)turret.GetHealth() / 100.0f;
        
        // Background bar (black)
        glm::mat4 hpBg = glm::translate(model, glm::vec3(0.0f, 2.5f, 0.0f));
        hpBg = glm::scale(hpBg, glm::vec3(1.8f, 0.12f, 0.12f));
        DrawCube(hpBg, glm::vec3(0.1f, 0.1f, 0.1f));

        // Foreground bar (yellow/gold)
        glm::mat4 hpFg = glm::translate(model, glm::vec3(-0.9f * (1.0f - hpPercent), 2.5f, 0.01f));
        hpFg = glm::scale(hpFg, glm::vec3(1.8f * hpPercent, 0.12f, 0.12f));
        DrawCube(hpFg, glm::vec3(0.85f, 0.75f, 0.2f));
    }
}

void Renderer::DrawCube(glm::mat4 model, glm::vec3 color) {
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), color.x, color.y, color.z);
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, cubeFaceCount);
    glBindVertexArray(0);
}

void Renderer::DrawSphere(glm::mat4 model, glm::vec3 color, float radius) {
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), color.x, color.y, color.z);
    glBindVertexArray(sphereVAO);
    glDrawArrays(GL_TRIANGLES, 0, sphereFaceCount);
    glBindVertexArray(0);
}

void Renderer::DrawCylinder(glm::mat4 model, glm::vec3 color) {
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), color.x, color.y, color.z);
    glBindVertexArray(cylinderVAO);
    glDrawArrays(GL_TRIANGLES, 0, cylinderFaceCount);
    glBindVertexArray(0);
}

void Renderer::DrawQuad(glm::mat4 model, glm::vec3 color) {
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), color.x, color.y, color.z);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, quadFaceCount);
    glBindVertexArray(0);
}

void Renderer::DrawShadows(const Player& p1, const Player& p2,
                          const std::vector<Enemy>& enemies,
                          const std::vector<Turret>& turrets,
                          glm::mat4 projection, glm::mat4 view) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    
    glm::vec3 shadowColor(0.02f, 0.02f, 0.02f);
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), shadowColor.x, shadowColor.y, shadowColor.z);

    auto drawObjectShadow = [&](glm::vec3 pos, glm::vec3 scale, float rotY, bool isCube) {
        // Shadow is projected flat on ground at y = 0.01f
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, 0.01f, pos.z));
        // Rotate flat onto ground
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, rotY, glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(scale.x * 1.2f, scale.z * 1.2f, 1.0f));
        
        DrawQuad(model, shadowColor);
    };

    // Draw Player 1 shadow
    if (p1.GetHealth() > 0) {
        drawObjectShadow(p1.GetPosition(), glm::vec3(1.5f, 0.6f, 2.0f), p1.GetRotation().y, true);
    }
    // Draw Player 2 shadow
    if (p2.GetHealth() > 0) {
        drawObjectShadow(p2.GetPosition(), glm::vec3(1.5f, 0.6f, 2.0f), p2.GetRotation().y, true);
    }
    // Draw Enemy shadows
    for (const auto& enemy : enemies) {
        drawObjectShadow(enemy.GetPosition(), glm::vec3(1.0f, 1.0f, 1.0f), enemy.GetRotation(), true);
    }
    // Draw Turret shadows
    for (const auto& turret : turrets) {
        if (turret.IsAlive()) {
            drawObjectShadow(turret.GetPosition(), glm::vec3(1.5f, 1.5f, 1.5f), 0.0f, true);
        }
    }
}

void Renderer::DrawHUD(const Player& p1, const Player& p2, float gameTimer,
                      float waveDuration, int* turretHealth, int score, int wave) {
    // 1. HUD Background / Top panel
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, 0.0f, 0.0f, (float)screenWidth, 60.0f, glm::vec3(0.08f, 0.08f, 0.12f), (float)screenWidth, (float)screenHeight);

    // 2. Player 1 HP Bar (Green panel left)
    DrawText2D("P1", 20.0f, 10.0f, 1.0f);
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, 50.0f, 15.0f, 150.0f, 15.0f, glm::vec3(0.2f, 0.2f, 0.2f), (float)screenWidth, (float)screenHeight);
    if (p1.GetHealth() > 0) {
        float hpPercent = (float)p1.GetHealth() / 100.0f;
        drawBar2D(shaderProgram, quadVAO, quadFaceCount, 50.0f, 15.0f, 150.0f * hpPercent, 15.0f, glm::vec3(0.2f, 0.8f, 0.2f), (float)screenWidth, (float)screenHeight);
    }
    // Lives count (Hati)
    std::string p1LivesText = "LIVES:" + std::to_string(p1.GetLives());
    DrawText2D(p1LivesText, 210.0f, 12.0f, 0.8f);

    // 3. Player 2 HP Bar (Blue panel right)
    DrawText2D("P2", (float)screenWidth - 410.0f, 10.0f, 1.0f);
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, (float)screenWidth - 380.0f, 15.0f, 150.0f, 15.0f, glm::vec3(0.2f, 0.2f, 0.2f), (float)screenWidth, (float)screenHeight);
    if (p2.GetHealth() > 0) {
        float hpPercent = (float)p2.GetHealth() / 100.0f;
        drawBar2D(shaderProgram, quadVAO, quadFaceCount, (float)screenWidth - 380.0f, 15.0f, 150.0f * hpPercent, 15.0f, glm::vec3(0.2f, 0.2f, 0.8f), (float)screenWidth, (float)screenHeight);
    }
    std::string p2LivesText = "LIVES:" + std::to_string(p2.GetLives());
    DrawText2D(p2LivesText, (float)screenWidth - 220.0f, 12.0f, 0.8f);

    // 4. Center Info: Score, Wave, Timer
    std::string scoreText = "SCORE:" + std::to_string(score);
    DrawText2D(scoreText, (float)screenWidth / 2.0f - 180.0f, 10.0f, 1.0f);

    std::string waveText = "WAVE:" + std::to_string(wave);
    DrawText2D(waveText, (float)screenWidth / 2.0f - 30.0f, 10.0f, 1.0f);

    int seconds = (int)gameTimer;
    std::string timerText = "TIME:" + std::to_string(seconds) + "S";
    DrawText2D(timerText, (float)screenWidth / 2.0f + 70.0f, 10.0f, 1.0f);

    // 5. Turret Health Bars at the bottom
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, 0.0f, (float)screenHeight - 40.0f, (float)screenWidth, 40.0f, glm::vec3(0.08f, 0.08f, 0.12f), (float)screenWidth, (float)screenHeight);
    
    DrawText2D("TURRETS:", 20.0f, (float)screenHeight - 30.0f, 0.9f);
    
    float barWidth = 120.0f;
    float startX = 120.0f;
    for (int i = 0; i < 3; i++) {
        float tx = startX + i * (barWidth + 40.0f);
        float ty = (float)screenHeight - 28.0f;
        
        std::string labelText = "T" + std::to_string(i + 1) + ":";
        DrawText2D(labelText, tx, ty - 2.0f, 0.8f);
        
        drawBar2D(shaderProgram, quadVAO, quadFaceCount, tx + 30.0f, ty, barWidth, 12.0f, glm::vec3(0.2f, 0.2f, 0.2f), (float)screenWidth, (float)screenHeight);
        if (turretHealth[i] > 0) {
            float thPercent = (float)turretHealth[i] / 100.0f;
            drawBar2D(shaderProgram, quadVAO, quadFaceCount, tx + 30.0f, ty, barWidth * thPercent, 12.0f, glm::vec3(0.8f, 0.8f, 0.2f), (float)screenWidth, (float)screenHeight);
        } else {
            DrawText2D("DESTROYED", tx + 30.0f, ty - 2.0f, 0.6f);
        }
    }
}

void Renderer::DrawGameOverScreen(int score, int stars, int wave) {
    // Semitransparent black background
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, 0.0f, 0.0f, (float)screenWidth, (float)screenHeight, glm::vec3(0.05f, 0.05f, 0.08f), (float)screenWidth, (float)screenHeight);

    float centerX = (float)screenWidth / 2.0f;
    float centerY = (float)screenHeight / 2.0f;

    DrawText2D("GAME OVER", centerX - 140.0f, centerY - 120.0f, 2.5f);
    
    std::string waveText = "FINAL WAVE: " + std::to_string(wave);
    DrawText2D(waveText, centerX - 110.0f, centerY - 40.0f, 1.2f);
    
    std::string scoreText = "FINAL SCORE: " + std::to_string(score);
    DrawText2D(scoreText, centerX - 120.0f, centerY, 1.2f);
    
    std::string starsText = "STARS: ";
    for (int i = 0; i < stars; i++) starsText += "* ";
    if (stars == 0) starsText += "NONE";
    DrawText2D(starsText, centerX - 80.0f, centerY + 40.0f, 1.2f);
    
    DrawText2D("PRESS SPACE TO RETURN TO MENU", centerX - 250.0f, centerY + 120.0f, 1.0f);
}

void Renderer::DrawText2D(const std::string& text, float x, float y, float scale) {
    glDisable(GL_DEPTH_TEST);
    
    std::vector<float> lineVertices;
    float currentX = x;
    float charWidth = 12.0f * scale;
    float charHeight = 20.0f * scale;
    float spacing = 4.0f * scale;
    
    for (char c : text) {
        if (c == ' ') {
            currentX += charWidth + spacing;
            continue;
        }
        
        auto strokes = getCharStrokes(c);
        for (const auto& line : strokes) {
            float px1 = currentX + line.p1.x * charWidth;
            float py1 = y + line.p1.y * charHeight;
            float px2 = currentX + line.p2.x * charWidth;
            float py2 = y + line.p2.y * charHeight;
            
            float ndcX1 = (px1 / screenWidth) * 2.0f - 1.0f;
            float ndcY1 = 1.0f - (py1 / screenHeight) * 2.0f;
            float ndcX2 = (px2 / screenWidth) * 2.0f - 1.0f;
            float ndcY2 = 1.0f - (py2 / screenHeight) * 2.0f;
            
            lineVertices.push_back(ndcX1); lineVertices.push_back(ndcY1); lineVertices.push_back(0.0f);
            lineVertices.push_back(0.0f); lineVertices.push_back(0.0f); lineVertices.push_back(1.0f);
            
            lineVertices.push_back(ndcX2); lineVertices.push_back(ndcY2); lineVertices.push_back(0.0f);
            lineVertices.push_back(0.0f); lineVertices.push_back(0.0f); lineVertices.push_back(1.0f);
        }
        currentX += charWidth + spacing;
    }
    
    if (!lineVertices.empty()) {
        drawLineList(shaderProgram, lineVertices, glm::vec3(1.0f, 1.0f, 1.0f));
    }
    
    glEnable(GL_DEPTH_TEST);
}

void Renderer::Shutdown() {
    if (shaderProgram != 0) {
        glDeleteProgram(shaderProgram);
    }
    if (shadowShaderProgram != 0) {
        glDeleteProgram(shadowShaderProgram);
    }
    // Delete VAOs
    if (cubeVAO != 0) glDeleteVertexArrays(1, &cubeVAO);
    if (sphereVAO != 0) glDeleteVertexArrays(1, &sphereVAO);
    if (cylinderVAO != 0) glDeleteVertexArrays(1, &cylinderVAO);
    if (quadVAO != 0) glDeleteVertexArrays(1, &quadVAO);
}
