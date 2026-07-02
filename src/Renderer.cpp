#include "Renderer.h"
#include "Player.h"
#include "Enemy_Bullet_Turret.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <fstream>
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Simple vertex shader source
const char* vertexShaderSource = R"(
#version 410 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec4 FragPosLightSpace;

void main()
{
    FragPos = vec3(model * vec4(position, 1.0));
    Normal = mat3(transpose(inverse(model))) * normal;
    TexCoords = texCoords;
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

// Depth shader
const char* depthVertexShaderSource = R"(
#version 410 core
layout(location = 0) in vec3 position;
uniform mat4 lightSpaceMatrix;
uniform mat4 model;
void main()
{
    gl_Position = lightSpaceMatrix * model * vec4(position, 1.0);
}
)";

const char* depthFragmentShaderSource = R"(
#version 410 core
void main()
{
    // gl_FragDepth = gl_FragCoord.z;
}
)";


// Simple fragment shader source
const char* fragmentShaderSource = R"(
#version 410 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 FragPosLightSpace;

uniform vec3 objectColor;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform sampler2D texture_diffuse;
uniform sampler2D shadowMap;
uniform int useTexture;   // 0=no texture, 1=ground (tiled), 2=object (no tiling)

out vec4 FragColor;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0) return 0.0;
    float currentDepth = projCoords.z;
    float bias = max(0.01 * (1.0 - dot(normal, lightDir)), 0.002);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    return shadow;
}

void main()
{
    vec3 color = objectColor;
    if (useTexture == 1) {
        // Ground texture - tiled
        color = clamp(texture(texture_diffuse, TexCoords * 20.0).rgb, 0.0, 1.0);
    } else if (useTexture == 2) {
        // Object texture (tank/turret) - no tiling
        vec4 texColor = texture(texture_diffuse, TexCoords);
        color = clamp(texColor.rgb, 0.0, 1.0);
    }
    // Ambient
    vec3 ambient = 0.45 * color;
    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * color;
    // Specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = 0.0 * spec * vec3(1.0);
    if (useTexture == 0) {
        specular = 0.4 * spec * vec3(1.0); // Specular only for solid-color objects
    }
    
    float shadow = ShadowCalculation(FragPosLightSpace, norm, lightDir);
    
    vec3 result = ambient + (1.0 - shadow) * (diffuse + specular);
    FragColor = vec4(clamp(result, 0.0, 1.0), 1.0);
}
)";


// Helper function to load texture
// forceChannels: 0=auto, 3=RGB, 4=RGBA
static GLuint loadTexture(const char* path, int forceChannels = 0) {
    GLuint textureID = 0;
    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, forceChannels);
    if (forceChannels != 0) nrComponents = forceChannels;
    if (data) {
        GLenum internalFormat = GL_RGB;
        GLenum dataFormat = GL_RGB;
        if (nrComponents == 1) {
            internalFormat = GL_RED;
            dataFormat = GL_RED;
        } else if (nrComponents == 3) {
            internalFormat = GL_RGB;
            dataFormat = GL_RGB;
        } else if (nrComponents == 4) {
            internalFormat = GL_RGBA;
            dataFormat = GL_RGBA;
        }

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        // Use GL_RGB internal format even for RGBA source to avoid alpha issues
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
        std::cout << "[TEXTURE OK] " << path << " (" << width << "x" << height << ", ch=" << nrComponents << ")" << std::endl;
    } else {
        std::cerr << "[TEXTURE FAIL] " << path << " - " << stbi_failure_reason() << std::endl;
    }
    return textureID;
}

Renderer::Renderer(int width, int height)
    : screenWidth(width), screenHeight(height),
      shaderProgram(0), shadowShaderProgram(0),
      cubeVAO(0), sphereVAO(0), cylinderVAO(0), quadVAO(0),
      cubeFaceCount(0), sphereFaceCount(0), cylinderFaceCount(0), quadFaceCount(0),
      floorTexture(0) {}

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize() {
    // Compile shaders
    shaderProgram = CreateShaderProgram(vertexShaderSource, fragmentShaderSource);
    shadowShaderProgram = CreateShaderProgram(depthVertexShaderSource, depthFragmentShaderSource);
    if (shaderProgram == 0 || shadowShaderProgram == 0) {
        std::cerr << "Failed to create shader programs!" << std::endl;
        return false;
    }

    // Configure depth map FBO
    glGenFramebuffers(1, &depthMapFBO);
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Load floor texture from asset folder - try multiple paths
    const char* sandPaths[] = {
        "asset/sand2.jpeg",
        "../asset/sand2.jpeg",
        "COMGRAPH/asset/sand2.jpeg"
    };
    for (const char* p : sandPaths) {
        floorTexture = loadTexture(p, 3); // Force RGB for JPEG
        if (floorTexture != 0) break;
    }
    if (floorTexture == 0) {
        std::cerr << "[WARNING] Sand texture not found - ground will use solid color" << std::endl;
    }

    // Load tank and tower textures - PNG files, try 3-channel (RGB) forced load
    const char* tankPaths[] = {
        "asset/tank_camo.png",
        "../asset/tank_camo.png",
        "COMGRAPH/asset/tank_camo.png"
    };
    for (const char* p : tankPaths) {
        tankTexture = loadTexture(p, 3); // Force RGB to avoid alpha channel issues
        if (tankTexture != 0) break;
    }
    if (tankTexture == 0) {
        std::cerr << "[WARNING] Tank texture not found - tank will use solid color" << std::endl;
    }

    const char* turretPaths[] = {
        "asset/tower_metal.png",
        "../asset/tower_metal.png",
        "COMGRAPH/asset/tower_metal.png"
    };
    for (const char* p : turretPaths) {
        turretTexture = loadTexture(p, 3); // Force RGB
        if (turretTexture != 0) break;
    }
    if (turretTexture == 0) {
        std::cerr << "[WARNING] Turret texture not found - turret will use solid color" << std::endl;
    }

    // Setup 3D shapes
    SetupShapes();

    isShadowPass = false;
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
    // 1. CUBE setup - Position(3) + Normal(3) + UV(2) = 8 floats per vertex
    float cubeVertices[] = {
        // Position              // Normals           // UV
        // Back face
        -0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
        // Front face
        -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
        // Left face
        -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        // Right face
         0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,   1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        // Bottom face
        -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
        // Top face
        -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,  0.0f, 1.0f
    };
    cubeFaceCount = 36;
    GLuint cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    // Normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    // UV
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
    glDeleteBuffers(1, &cubeVBO);

    // 2. QUAD setup (for ground and UI)
    float quadVertices[] = {
        // Position           // Normals          // UV
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   1.0f, 1.0f,
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   0.0f, 0.0f,
         0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,   0.0f, 1.0f
    };
    quadFaceCount = 6;
    GLuint quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    
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

    // Side wall — 8 floats per vertex: pos(3) + normal(3) + uv(2)
    for (int i = 0; i < cylSectors; ++i) {
        float angle0 = 2 * PI * (float)i / cylSectors;
        float angle1 = 2 * PI * (float)(i + 1) / cylSectors;

        float x0 = cos(angle0) * cylRadius, z0 = sin(angle0) * cylRadius;
        float x1 = cos(angle1) * cylRadius, z1 = sin(angle1) * cylRadius;

        float u0 = (float)i / cylSectors;
        float u1 = (float)(i + 1) / cylSectors;

        // Triangle 1: bottom-left, bottom-right, top-right
        cylinderVertices.push_back(x0); cylinderVertices.push_back(-cylHeight*0.5f); cylinderVertices.push_back(z0);
        cylinderVertices.push_back(cos(angle0)); cylinderVertices.push_back(0.0f); cylinderVertices.push_back(sin(angle0));
        cylinderVertices.push_back(u0); cylinderVertices.push_back(0.0f);

        cylinderVertices.push_back(x1); cylinderVertices.push_back(-cylHeight*0.5f); cylinderVertices.push_back(z1);
        cylinderVertices.push_back(cos(angle1)); cylinderVertices.push_back(0.0f); cylinderVertices.push_back(sin(angle1));
        cylinderVertices.push_back(u1); cylinderVertices.push_back(0.0f);

        cylinderVertices.push_back(x1); cylinderVertices.push_back(cylHeight*0.5f); cylinderVertices.push_back(z1);
        cylinderVertices.push_back(cos(angle1)); cylinderVertices.push_back(0.0f); cylinderVertices.push_back(sin(angle1));
        cylinderVertices.push_back(u1); cylinderVertices.push_back(1.0f);

        // Triangle 2: bottom-left, top-right, top-left
        cylinderVertices.push_back(x0); cylinderVertices.push_back(-cylHeight*0.5f); cylinderVertices.push_back(z0);
        cylinderVertices.push_back(cos(angle0)); cylinderVertices.push_back(0.0f); cylinderVertices.push_back(sin(angle0));
        cylinderVertices.push_back(u0); cylinderVertices.push_back(0.0f);

        cylinderVertices.push_back(x1); cylinderVertices.push_back(cylHeight*0.5f); cylinderVertices.push_back(z1);
        cylinderVertices.push_back(cos(angle1)); cylinderVertices.push_back(0.0f); cylinderVertices.push_back(sin(angle1));
        cylinderVertices.push_back(u1); cylinderVertices.push_back(1.0f);

        cylinderVertices.push_back(x0); cylinderVertices.push_back(cylHeight*0.5f); cylinderVertices.push_back(z0);
        cylinderVertices.push_back(cos(angle0)); cylinderVertices.push_back(0.0f); cylinderVertices.push_back(sin(angle0));
        cylinderVertices.push_back(u0); cylinderVertices.push_back(1.0f);
    }

    // Top cap
    for (int i = 0; i < cylSectors; ++i) {
        float angle0 = 2 * PI * (float)i / cylSectors;
        float angle1 = 2 * PI * (float)(i + 1) / cylSectors;

        float x0 = cos(angle0) * cylRadius, z0 = sin(angle0) * cylRadius;
        float x1 = cos(angle1) * cylRadius, z1 = sin(angle1) * cylRadius;

        // center
        cylinderVertices.push_back(0.0f); cylinderVertices.push_back(cylHeight*0.5f); cylinderVertices.push_back(0.0f);
        cylinderVertices.push_back(0.0f); cylinderVertices.push_back(1.0f); cylinderVertices.push_back(0.0f);
        cylinderVertices.push_back(0.5f); cylinderVertices.push_back(0.5f);

        cylinderVertices.push_back(x0); cylinderVertices.push_back(cylHeight*0.5f); cylinderVertices.push_back(z0);
        cylinderVertices.push_back(0.0f); cylinderVertices.push_back(1.0f); cylinderVertices.push_back(0.0f);
        cylinderVertices.push_back(0.5f + 0.5f*cos(angle0)); cylinderVertices.push_back(0.5f + 0.5f*sin(angle0));

        cylinderVertices.push_back(x1); cylinderVertices.push_back(cylHeight*0.5f); cylinderVertices.push_back(z1);
        cylinderVertices.push_back(0.0f); cylinderVertices.push_back(1.0f); cylinderVertices.push_back(0.0f);
        cylinderVertices.push_back(0.5f + 0.5f*cos(angle1)); cylinderVertices.push_back(0.5f + 0.5f*sin(angle1));
    }

    // Bottom cap
    for (int i = 0; i < cylSectors; ++i) {
        float angle0 = 2 * PI * (float)i / cylSectors;
        float angle1 = 2 * PI * (float)(i + 1) / cylSectors;

        float x0 = cos(angle0) * cylRadius, z0 = sin(angle0) * cylRadius;
        float x1 = cos(angle1) * cylRadius, z1 = sin(angle1) * cylRadius;

        cylinderVertices.push_back(0.0f); cylinderVertices.push_back(-cylHeight*0.5f); cylinderVertices.push_back(0.0f);
        cylinderVertices.push_back(0.0f); cylinderVertices.push_back(-1.0f); cylinderVertices.push_back(0.0f);
        cylinderVertices.push_back(0.5f); cylinderVertices.push_back(0.5f);

        cylinderVertices.push_back(x1); cylinderVertices.push_back(-cylHeight*0.5f); cylinderVertices.push_back(z1);
        cylinderVertices.push_back(0.0f); cylinderVertices.push_back(-1.0f); cylinderVertices.push_back(0.0f);
        cylinderVertices.push_back(0.5f + 0.5f*cos(angle1)); cylinderVertices.push_back(0.5f + 0.5f*sin(angle1));

        cylinderVertices.push_back(x0); cylinderVertices.push_back(-cylHeight*0.5f); cylinderVertices.push_back(z0);
        cylinderVertices.push_back(0.0f); cylinderVertices.push_back(-1.0f); cylinderVertices.push_back(0.0f);
        cylinderVertices.push_back(0.5f + 0.5f*cos(angle0)); cylinderVertices.push_back(0.5f + 0.5f*sin(angle0));
    }
    cylinderFaceCount = cylinderVertices.size() / 8; // now 8 floats per vertex
    GLuint cylinderVBO;
    glGenVertexArrays(1, &cylinderVAO);
    glGenBuffers(1, &cylinderVBO);
    glBindVertexArray(cylinderVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cylinderVBO);
    glBufferData(GL_ARRAY_BUFFER, cylinderVertices.size() * sizeof(float), cylinderVertices.data(), GL_STATIC_DRAW);
    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    // Normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    // UV
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
    glDeleteBuffers(1, &cylinderVBO);
}

void Renderer::BeginShadowPass(glm::vec3 lightPosition) {
    isShadowPass = true;
    lightPos = lightPosition;

    // 1. Render depth of scene to texture (from light's perspective)
    glm::mat4 lightProjection, lightView;
    float near_plane = 1.0f, far_plane = 75.0f;
    lightProjection = glm::ortho(-35.0f, 35.0f, -35.0f, 35.0f, near_plane, far_plane);
    lightView = glm::lookAt(lightPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    lightSpaceMatrix = lightProjection * lightView;

    glUseProgram(shadowShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shadowShaderProgram, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    // Draw functions will be called next...
}

void Renderer::BeginMainPass(glm::mat4 projection, glm::mat4 view, glm::vec3 cameraPos) {
    isShadowPass = false;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screenWidth, screenHeight);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniform3f(glGetUniformLocation(shaderProgram, "viewPos"), cameraPos.x, cameraPos.y, cameraPos.z);
    glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glUniform1i(glGetUniformLocation(shaderProgram, "shadowMap"), 1);
    // Draw functions will be called next...
}

void Renderer::EndFrame() {}

void Renderer::Resize(int width, int height) {
    screenWidth = width;
    screenHeight = height;
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
        case '*':
            lines.push_back({tc, bc}); // Vertikal
            lines.push_back({{0.15f, 0.35f}, {0.85f, 0.65f}}); // Diagonal turun
            lines.push_back({{0.15f, 0.65f}, {0.85f, 0.35f}}); // Diagonal naik
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
    glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0); // 0 = no texture
    
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
    glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0); // 0 = no texture
    
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, quadFaceCount);
    glBindVertexArray(0);
    
    glEnable(GL_DEPTH_TEST);
}

// A bar with a dark bezel/border, a track background, and a colored fill
// (proportional to pct, 0..1) drawn on top. Mimics the bordered HP bars
// seen in the reference HUD mockup.
static void drawBorderedBar2D(GLuint shaderProgram, GLuint quadVAO, unsigned int quadFaceCount,
                               float x, float y, float w, float h, float pct, glm::vec3 fillColor,
                               float screenWidth, float screenHeight) {
    pct = pct < 0.0f ? 0.0f : (pct > 1.0f ? 1.0f : pct);
    const float border = 2.0f;
    // Outer bezel (slightly larger, near-black)
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, x - border, y - border, w + border * 2.0f, h + border * 2.0f,
               glm::vec3(0.03f, 0.03f, 0.04f), screenWidth, screenHeight);
    // Track background
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, x, y, w, h, glm::vec3(0.16f, 0.16f, 0.19f), screenWidth, screenHeight);
    // Fill
    if (pct > 0.0f) {
        drawBar2D(shaderProgram, quadVAO, quadFaceCount, x, y, w * pct, h, fillColor, screenWidth, screenHeight);
    }
}

// Small square "pip" indicator (used for life pips and turret status pips),
// drawn with the same bordered look as drawBorderedBar2D but always fully filled.
static void drawPip2D(GLuint shaderProgram, GLuint quadVAO, unsigned int quadFaceCount,
                       float x, float y, float w, float h, glm::vec3 fillColor,
                       float screenWidth, float screenHeight) {
    const float border = 1.5f;
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, x - border, y - border, w + border * 2.0f, h + border * 2.0f,
               glm::vec3(0.03f, 0.03f, 0.04f), screenWidth, screenHeight);
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, x, y, w, h, fillColor, screenWidth, screenHeight);
}

// Health-based color ramp: green when healthy, amber mid-way, red when low.
static glm::vec3 healthColor(float pct) {
    if (pct > 0.6f) return glm::vec3(0.25f, 0.78f, 0.30f);
    if (pct > 0.3f) return glm::vec3(0.85f, 0.70f, 0.20f);
    return glm::vec3(0.82f, 0.25f, 0.22f);
}

void Renderer::DrawGround(glm::mat4 projection, glm::mat4 view) {
    GLuint currentProg = isShadowPass ? shadowShaderProgram : shaderProgram;
    glUseProgram(currentProg);
    
    // Rotate -90 degrees so normal (0,0,1) becomes (0,1,0) pointing UP.
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::scale(model, glm::vec3(200.0f, 200.0f, 1.0f));
    glUniformMatrix4fv(glGetUniformLocation(currentProg, "model"), 1, GL_FALSE, glm::value_ptr(model));

    if (!isShadowPass) {
        if (floorTexture != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, floorTexture);
            glUniform1i(glGetUniformLocation(shaderProgram, "texture_diffuse"), 0);
            glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 1); // 1 = ground tiled
        } else {
            // Fallback: sandy color for ground
            glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0);
            glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 0.76f, 0.60f, 0.38f);
        }
    } else {
        // Shadow pass - no texture needed
        glUniform1i(glGetUniformLocation(shadowShaderProgram, "useTexture"), 0);
    }

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, quadFaceCount);
    glBindVertexArray(0);
}

void Renderer::DrawPlayer(const Player& player, glm::mat4 projection, glm::mat4 view) {
    GLuint currentProg = isShadowPass ? shadowShaderProgram : shaderProgram;
    glUseProgram(currentProg);

    glm::vec3 pos = player.GetPosition();
    glm::vec3 rot = player.GetRotation();
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
    model = glm::rotate(model, rot.y, glm::vec3(0.0f, 1.0f, 0.0f));

    // Warna berdasarkan player
    glm::vec3 bodyColor  = player.IsPlayer1() ? glm::vec3(0.18f, 0.72f, 0.22f) : glm::vec3(0.18f, 0.35f, 0.80f);
    glm::vec3 domeColor  = player.IsPlayer1() ? glm::vec3(0.12f, 0.55f, 0.15f) : glm::vec3(0.12f, 0.25f, 0.65f);
    glm::vec3 barrelColor= glm::vec3(0.18f, 0.18f, 0.20f); // Dark metal

    // --- Body (Textured Cube) ---
    glm::mat4 bodyModel = glm::scale(model, glm::vec3(1.5f, 0.6f, 2.0f));
    if (isShadowPass) {
        glUniformMatrix4fv(glGetUniformLocation(currentProg, "model"), 1, GL_FALSE, glm::value_ptr(bodyModel));
        glBindVertexArray(cubeVAO); glDrawArrays(GL_TRIANGLES, 0, cubeFaceCount); glBindVertexArray(0);
    } else if (tankTexture != 0) {
        DrawCubeTextured(bodyModel, tankTexture);
    } else {
        DrawCube(bodyModel, bodyColor);
    }

    // --- Turret Dome (Textured Cylinder) ---
    glm::mat4 turretDomeModel = glm::translate(model, glm::vec3(0.0f, 0.5f, 0.0f));
    turretDomeModel = glm::scale(turretDomeModel, glm::vec3(0.8f, 0.5f, 0.8f));
    if (isShadowPass) {
        glUniformMatrix4fv(glGetUniformLocation(currentProg, "model"), 1, GL_FALSE, glm::value_ptr(turretDomeModel));
        glBindVertexArray(cylinderVAO); glDrawArrays(GL_TRIANGLES, 0, cylinderFaceCount); glBindVertexArray(0);
    } else if (tankTexture != 0) {
        DrawCylinderTextured(turretDomeModel, tankTexture);
    } else {
        DrawCylinder(turretDomeModel, domeColor);
    }

    // --- Barrel (Dark metal cylinder - textured with turret metal texture or dark color) ---
    glm::mat4 barrelModel = glm::translate(model, glm::vec3(0.0f, 0.5f, -0.8f));
    barrelModel = glm::rotate(barrelModel, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    barrelModel = glm::scale(barrelModel, glm::vec3(0.2f, 1.2f, 0.2f));
    if (isShadowPass) {
        glUniformMatrix4fv(glGetUniformLocation(currentProg, "model"), 1, GL_FALSE, glm::value_ptr(barrelModel));
        glBindVertexArray(cylinderVAO); glDrawArrays(GL_TRIANGLES, 0, cylinderFaceCount); glBindVertexArray(0);
    } else if (turretTexture != 0) {
        // Barrel pakai turretTexture (metal plate) agar terlihat logam
        DrawCylinderTextured(barrelModel, turretTexture);
    } else {
        DrawCylinder(barrelModel, barrelColor);
    }

    // --- World-space HP bar above tank (only in main pass) ---
    if (!isShadowPass && player.GetHealth() > 0) {
        glUseProgram(shaderProgram);
        float hpPct = player.GetHealth() / 100.0f;

        // Background (dark)
        glm::mat4 hpBg = glm::translate(glm::mat4(1.0f), pos + glm::vec3(0.0f, 1.5f, 0.0f));
        hpBg = glm::scale(hpBg, glm::vec3(2.0f, 0.15f, 0.15f));
        DrawCube(hpBg, glm::vec3(0.1f, 0.1f, 0.1f));

        glm::vec3 hpColor = hpPct > 0.6f ? glm::vec3(0.2f, 0.85f, 0.2f) :
                            hpPct > 0.3f ? glm::vec3(0.9f, 0.75f, 0.1f) :
                                           glm::vec3(0.9f, 0.15f, 0.15f);
        glm::mat4 hpFg = glm::translate(glm::mat4(1.0f), pos + glm::vec3(-1.0f * (1.0f - hpPct), 1.5f, 0.01f));
        hpFg = glm::scale(hpFg, glm::vec3(2.0f * hpPct, 0.15f, 0.15f));
        DrawCube(hpFg, hpColor);
    }
    glBindVertexArray(0);
}


void Renderer::DrawEnemy(const Enemy& enemy, glm::mat4 projection, glm::mat4 view) {
    // Gunakan shadow atau main shader sesuai pass
    GLuint currentProg = isShadowPass ? shadowShaderProgram : shaderProgram;
    glUseProgram(currentProg);

    glm::vec3 pos = enemy.GetPosition();
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
    model = glm::rotate(model, enemy.GetRotation(), glm::vec3(0.0f, 1.0f, 0.0f));

    // Colors (hanya main pass)
    glm::vec3 planeColor(0.85f, 0.15f, 0.15f);
    glm::vec3 wingColor(0.88f, 0.88f, 0.88f);
    glm::vec3 propColor(0.9f, 0.7f, 0.1f);

    auto shadowCylinder = [&](glm::mat4 m) {
        glUniformMatrix4fv(glGetUniformLocation(currentProg, "model"), 1, GL_FALSE, glm::value_ptr(m));
        glBindVertexArray(cylinderVAO); glDrawArrays(GL_TRIANGLES, 0, cylinderFaceCount); glBindVertexArray(0);
    };
    auto shadowCube = [&](glm::mat4 m) {
        glUniformMatrix4fv(glGetUniformLocation(currentProg, "model"), 1, GL_FALSE, glm::value_ptr(m));
        glBindVertexArray(cubeVAO); glDrawArrays(GL_TRIANGLES, 0, cubeFaceCount); glBindVertexArray(0);
    };

    // 1. Fuselage
    glm::mat4 fuselage = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    fuselage = glm::scale(fuselage, glm::vec3(0.35f, 1.6f, 0.35f));
    isShadowPass ? shadowCylinder(fuselage) : DrawCylinder(fuselage, planeColor);

    // 2. Main Wings
    glm::mat4 wings = glm::translate(model, glm::vec3(0.0f, 0.0f, -0.2f));
    wings = glm::scale(wings, glm::vec3(2.6f, 0.06f, 0.5f));
    isShadowPass ? shadowCube(wings) : DrawCube(wings, wingColor);

    // 3. Tail Wings
    glm::mat4 tailWings = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.7f));
    tailWings = glm::scale(tailWings, glm::vec3(1.0f, 0.05f, 0.25f));
    isShadowPass ? shadowCube(tailWings) : DrawCube(tailWings, wingColor);

    // 4. Vertical Stabilizer
    glm::mat4 tailFin = glm::translate(model, glm::vec3(0.0f, 0.25f, 0.7f));
    tailFin = glm::scale(tailFin, glm::vec3(0.05f, 0.4f, 0.2f));
    isShadowPass ? shadowCube(tailFin) : DrawCube(tailFin, planeColor);

    // 5. Spinner (moncong)
    glm::mat4 spinner = glm::translate(model, glm::vec3(0.0f, 0.0f, -0.82f));
    spinner = glm::rotate(spinner, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    spinner = glm::scale(spinner, glm::vec3(0.2f, 0.15f, 0.2f));
    isShadowPass ? shadowCylinder(spinner) : DrawCylinder(spinner, propColor);

    // 6. Propeller blades (hanya main pass agar shadow tidak aneh berputar)
    if (!isShadowPass) {
        float propAngle = (float)glfwGetTime() * 25.0f;
        glm::mat4 propRoot = glm::translate(model, glm::vec3(0.0f, 0.0f, -0.85f));
        propRoot = glm::rotate(propRoot, propAngle, glm::vec3(0.0f, 0.0f, 1.0f));
        DrawCube(glm::scale(propRoot, glm::vec3(0.06f, 0.8f, 0.02f)), glm::vec3(0.2f, 0.2f, 0.2f));
        DrawCube(glm::scale(propRoot, glm::vec3(0.8f, 0.06f, 0.02f)), glm::vec3(0.2f, 0.2f, 0.2f));
    }

    // 7. Health Bar (main pass only)
    if (!isShadowPass && enemy.GetHealth() > 0) {
        float hpPct = (float)enemy.GetHealth() / 3.0f;
        glm::mat4 hpBg = glm::translate(model, glm::vec3(0.0f, 0.9f, 0.0f));
        hpBg = glm::scale(hpBg, glm::vec3(1.0f, 0.08f, 0.08f));
        DrawCube(hpBg, glm::vec3(0.1f, 0.1f, 0.1f));
        glm::mat4 hpFg = glm::translate(model, glm::vec3(-0.5f * (1.0f - hpPct), 0.9f, 0.01f));
        hpFg = glm::scale(hpFg, glm::vec3(1.0f * hpPct, 0.08f, 0.08f));
        DrawCube(hpFg, glm::vec3(0.2f, 0.8f, 0.2f));
    }
}

void Renderer::DrawBullet(const Bullet& bullet, glm::mat4 projection, glm::mat4 view) {
    GLuint currentProg = isShadowPass ? shadowShaderProgram : shaderProgram;
    glUseProgram(currentProg);

    glm::vec3 pos = bullet.GetPosition();
    glm::vec3 dir = bullet.GetDirection();
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);
    
    // Rotate the bullet to face its direction
    float rotY = atan2(dir.x, dir.z);
    model = glm::rotate(model, rotY, glm::vec3(0.0f, 1.0f, 0.0f));
    
    // Scale it to be oval/elongated along its local Z axis
    model = glm::scale(model, glm::vec3(0.15f, 0.15f, 0.4f));

    glUniformMatrix4fv(glGetUniformLocation(currentProg, "model"), 1, GL_FALSE, glm::value_ptr(model));
    if(!isShadowPass) {
        glm::vec3 color = bullet.IsEnemyBullet() ? glm::vec3(1.0f, 0.15f, 0.15f) : glm::vec3(1.0f, 1.0f, 0.0f);
        glUniform3f(glGetUniformLocation(currentProg, "objectColor"), color.x, color.y, color.z);
        glUniform1i(glGetUniformLocation(currentProg, "useTexture"), 0);
    }
    glBindVertexArray(sphereVAO);
    glDrawArrays(GL_TRIANGLES, 0, sphereFaceCount);
    glBindVertexArray(0);
}

void Renderer::DrawTurret(const Turret& turret, glm::mat4 projection, glm::mat4 view) {
    // Gunakan shadow atau main shader sesuai pass
    GLuint currentProg = isShadowPass ? shadowShaderProgram : shaderProgram;
    glUseProgram(currentProg);

    glm::vec3 pos = turret.GetPosition();
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);

    // Warna turret: aktif=gold, hancur=abu
    glm::vec3 goldColor(0.80f, 0.68f, 0.18f);
    glm::vec3 deadColor(0.28f, 0.28f, 0.28f);
    glm::vec3 metalDark(0.20f, 0.20f, 0.22f);  // Warna tiang/barrel
    glm::vec3 metalMid(0.38f, 0.38f, 0.40f);   // Warna shield armor

    // Helper lambdas untuk shadow pass
    auto sCube = [&](glm::mat4 m) {
        glUniformMatrix4fv(glGetUniformLocation(currentProg, "model"), 1, GL_FALSE, glm::value_ptr(m));
        glBindVertexArray(cubeVAO); glDrawArrays(GL_TRIANGLES, 0, cubeFaceCount); glBindVertexArray(0);
    };
    auto sCyl = [&](glm::mat4 m) {
        glUniformMatrix4fv(glGetUniformLocation(currentProg, "model"), 1, GL_FALSE, glm::value_ptr(m));
        glBindVertexArray(cylinderVAO); glDrawArrays(GL_TRIANGLES, 0, cylinderFaceCount); glBindVertexArray(0);
    };
    auto sQuad = [&](glm::mat4 m) {
        glUniformMatrix4fv(glGetUniformLocation(currentProg, "model"), 1, GL_FALSE, glm::value_ptr(m));
        glBindVertexArray(quadVAO); glDrawArrays(GL_TRIANGLES, 0, quadFaceCount); glBindVertexArray(0);
    };

    // 1. Base ring (dekoratif, hanya main pass karena shadow-nya tidak penting)
    if (!isShadowPass) {
        glm::mat4 ringModel = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, -0.24f, pos.z));
        ringModel = glm::rotate(ringModel, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        ringModel = glm::scale(ringModel, glm::vec3(3.2f, 3.2f, 1.0f));
        DrawQuad(ringModel, turret.IsAlive() ? glm::vec3(0.9f, 0.8f, 0.1f) : glm::vec3(0.4f, 0.15f, 0.15f));
    }

    // 2. Base plate - texture metal untuk turret aktif
    glm::mat4 baseModel = glm::scale(model, glm::vec3(2.5f, 0.6f, 2.5f));
    if (isShadowPass) { sCube(baseModel); }
    else if (turretTexture != 0 && turret.IsAlive()) { DrawCubeTextured(baseModel, turretTexture); }
    else { DrawCube(baseModel, turret.IsAlive() ? goldColor : deadColor); }

    // 3. Four Corner Pillars - warna gelap (metalDark), bukan tekstur
    if (turret.IsAlive()) {
        float off = 1.05f;
        glm::vec3 pillarOff[4] = {{-off,0.4f,-off},{off,0.4f,-off},{-off,0.4f,off},{off,0.4f,off}};
        for (auto& o : pillarOff) {
            glm::mat4 pm = glm::scale(glm::translate(model, o), glm::vec3(0.4f, 0.8f, 0.4f));
            isShadowPass ? sCube(pm) : DrawCube(pm, metalDark);
        }
    }

    // 4. Sub-base plate - tekstur juga
    glm::mat4 subBase = glm::scale(glm::translate(model, glm::vec3(0.0f, 0.35f, 0.0f)), glm::vec3(1.8f, 0.3f, 1.8f));
    if (isShadowPass) { sCube(subBase); }
    else if (turretTexture != 0 && turret.IsAlive()) { DrawCubeTextured(subBase, turretTexture); }
    else { DrawCube(subBase, turret.IsAlive() ? goldColor * 0.85f : deadColor * 0.85f); }

    // 5. Dome body (Cylinder) - tekstur utama turret
    glm::mat4 domeModel = glm::scale(glm::translate(model, glm::vec3(0.0f, 0.8f, 0.0f)), glm::vec3(1.4f, 0.8f, 1.4f));
    if (isShadowPass) { sCyl(domeModel); }
    else if (turretTexture != 0 && turret.IsAlive()) { DrawCylinderTextured(domeModel, turretTexture); }
    else { DrawCylinder(domeModel, turret.IsAlive() ? goldColor * 0.95f : deadColor); }

    // 6. Side Shield Armor - warna metalMid untuk kontras
    if (turret.IsAlive()) {
        glm::mat4 lShield = glm::scale(glm::translate(model, glm::vec3(-0.85f, 0.9f, 0.0f)), glm::vec3(0.2f, 0.8f, 1.3f));
        glm::mat4 rShield = glm::scale(glm::translate(model, glm::vec3( 0.85f, 0.9f, 0.0f)), glm::vec3(0.2f, 0.8f, 1.3f));
        if (isShadowPass) { sCube(lShield); sCube(rShield); }
        else {
            // Shield pakai turretTexture tapi lebih gelap (tinted)
            if (turretTexture != 0) {
                DrawCubeTextured(lShield, turretTexture);
                DrawCubeTextured(rShield, turretTexture);
            } else {
                DrawCube(lShield, metalMid);
                DrawCube(rShield, metalMid);
            }
        }
    }

    // 7. Radar stand + dish (hanya main pass — shadow terlalu kecil untuk signifikan)
    if (turret.IsAlive() && !isShadowPass) {
        glm::mat4 radarStand = glm::scale(glm::translate(model, glm::vec3(0.0f, 1.35f, 0.4f)), glm::vec3(0.12f, 0.4f, 0.12f));
        DrawCylinder(radarStand, metalDark);

        float radarAngle = (float)glfwGetTime() * 4.0f;
        glm::mat4 radarDish = glm::rotate(glm::translate(model, glm::vec3(0.0f, 1.6f, 0.4f)), radarAngle, glm::vec3(0.0f, 1.0f, 0.0f));
        radarDish = glm::scale(radarDish, glm::vec3(0.9f, 0.15f, 0.3f));
        DrawCube(radarDish, glm::vec3(0.65f, 0.65f, 0.68f));
    }

    // 8. Gun Barrels - teksturkan dengan turretTexture agar terlihat logam
    if (turret.IsAlive()) {
        glm::vec3 barrelOffsets[2] = {{-0.4f, 0.9f, -0.6f}, {0.4f, 0.9f, -0.6f}};
        for (auto& bo : barrelOffsets) {
            glm::mat4 bm = glm::translate(model, bo);
            bm = glm::rotate(bm, glm::radians(55.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            bm = glm::scale(bm, glm::vec3(0.16f, 2.0f, 0.16f));
            if (isShadowPass) { sCyl(bm); }
            else if (turretTexture != 0) { DrawCylinderTextured(bm, turretTexture); }
            else { DrawCylinder(bm, metalDark); }
        }
    }

    // 9. HP bar (main pass only)
    if (!isShadowPass && turret.IsAlive() && turret.GetHealth() > 0) {
        float hpPct = (float)turret.GetHealth() / 100.0f;
        glm::mat4 hpBg = glm::scale(glm::translate(model, glm::vec3(0.0f, 2.5f, 0.0f)), glm::vec3(1.8f, 0.12f, 0.12f));
        DrawCube(hpBg, glm::vec3(0.1f, 0.1f, 0.1f));
        glm::mat4 hpFg = glm::scale(glm::translate(model, glm::vec3(-0.9f*(1.0f-hpPct), 2.5f, 0.01f)), glm::vec3(1.8f*hpPct, 0.12f, 0.12f));
        DrawCube(hpFg, glm::vec3(0.85f, 0.75f, 0.2f));
    }
}

void Renderer::DrawCube(glm::mat4 model, glm::vec3 color) {
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), color.x, color.y, color.z);
    glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0);
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, cubeFaceCount);
    glBindVertexArray(0);
}

void Renderer::DrawCubeTextured(glm::mat4 model, GLuint texture) {
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 2); // 2 = object texture (no tiling)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture_diffuse"), 0);
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, cubeFaceCount);
    glBindVertexArray(0);
    glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0);
}

void Renderer::DrawCylinderTextured(glm::mat4 model, GLuint texture) {
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 2); // 2 = object texture (no tiling)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture_diffuse"), 0);
    glBindVertexArray(cylinderVAO);
    glDrawArrays(GL_TRIANGLES, 0, cylinderFaceCount);
    glBindVertexArray(0);
    glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0);
}

void Renderer::DrawSphere(glm::mat4 model, glm::vec3 color, float radius) {
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), color.x, color.y, color.z);
    glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0);
    glBindVertexArray(sphereVAO);
    glDrawArrays(GL_TRIANGLES, 0, sphereFaceCount);
    glBindVertexArray(0);
}

void Renderer::DrawCylinder(glm::mat4 model, glm::vec3 color) {
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), color.x, color.y, color.z);
    glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0);
    glBindVertexArray(cylinderVAO);
    glDrawArrays(GL_TRIANGLES, 0, cylinderFaceCount);
    glBindVertexArray(0);
}

void Renderer::DrawQuad(glm::mat4 model, glm::vec3 color) {
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), color.x, color.y, color.z);
    glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, quadFaceCount);
    glBindVertexArray(0);
}



void Renderer::DrawHUD(const Player& p1, const Player& p2, float gameTimer,
                      float waveDuration, int* turretHealth, int score, int wave) {
    auto textWidth = [](const std::string& s, float scale) {
        return (float)s.size() * 16.0f * scale;
    };

    glm::vec3 panelColor(0.05f, 0.05f, 0.08f);
    glm::vec3 white(0.95f, 0.95f, 0.95f);
    glm::vec3 amber(0.95f, 0.78f, 0.25f);
    glm::vec3 slotColors[3] = {
        glm::vec3(0.30f, 0.80f, 0.32f), // green
        glm::vec3(0.92f, 0.78f, 0.20f), // amber
        glm::vec3(0.86f, 0.30f, 0.26f)  // red
    };
    glm::vec3 lostColor(0.20f, 0.20f, 0.23f);
    glm::vec3 pipLabelColor(0.65f, 0.65f, 0.68f);

    float sw = (float)screenWidth;
    float sh = (float)screenHeight;

    // ===================== TOP BAR =====================
    const float TOP_H = 64.0f;
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, 0.0f, 0.0f, sw, TOP_H, panelColor, sw, sh);

    // --- Player 1 (Left) ---
    DrawText2D("P1", 18.0f, 14.0f, 1.0f, glm::vec3(0.35f, 0.85f, 0.4f));
    float p1BarX = 56.0f, p1BarY = 18.0f, p1BarW = 150.0f, p1BarH = 14.0f;
    float p1Pct = p1.GetHealth() / 100.0f;
    drawBorderedBar2D(shaderProgram, quadVAO, quadFaceCount, p1BarX, p1BarY, p1BarW, p1BarH, p1Pct, healthColor(p1Pct), sw, sh);
    
    // P1 Lives - draw as stars below HP bar
    std::string p1LivesText = "LIVES: ";
    for (int i=0; i<p1.GetLives(); i++) p1LivesText += "* ";
    DrawText2D(p1LivesText, 56.0f, 38.0f, 0.75f, amber);

    // --- CENTER: SCORE / WAVE / TIME (clearly visible) ---
    std::string scoreText = "SCORE:" + std::to_string(score);
    std::string waveText  = "WAVE:" + std::to_string(wave);
    
    const float TOTAL_TIME = 60.0f;
    float remaining = std::max(0.0f, TOTAL_TIME - gameTimer);
    int mins = (int)(remaining / 60.0f);
    int secs = (int)(remaining) % 60;
    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%01d:%02d", mins, secs);
    std::string timerText = std::string("TIME:") + timeBuf;

    float gap = 36.0f, scale = 0.9f;
    float totalW = textWidth(scoreText, scale) + gap + textWidth(waveText, scale) + gap + textWidth(timerText, scale);
    float startX = sw / 2.0f - totalW / 2.0f;

    DrawText2D(scoreText, startX, 22.0f, scale, white);
    startX += textWidth(scoreText, scale) + gap;
    DrawText2D(waveText,  startX, 22.0f, scale, glm::vec3(0.3f, 0.9f, 1.0f));
    startX += textWidth(waveText, scale) + gap;
    DrawText2D(timerText, startX, 22.0f, scale, glm::vec3(1.0f, 0.85f, 0.3f));

    // --- Player 2 (Right) ---
    float p2BarW = 150.0f, p2BarH = 14.0f, p2BarY = 18.0f;
    float p2BarX = sw - 56.0f - p2BarW;
    float p2Pct = p2.GetHealth() / 100.0f;
    
    std::string p2LivesText = "LIVES: ";
    for (int i=0; i<p2.GetLives(); i++) p2LivesText += "* ";
    float p2LivesW = textWidth(p2LivesText, 0.75f);
    DrawText2D(p2LivesText, sw - 56.0f - p2LivesW, 38.0f, 0.75f, amber);
    
    drawBorderedBar2D(shaderProgram, quadVAO, quadFaceCount, p2BarX, p2BarY, p2BarW, p2BarH, p2Pct, healthColor(p2Pct), sw, sh);
    DrawText2D("P2", p2BarX + p2BarW + 14.0f, 14.0f, 1.0f, glm::vec3(0.35f, 0.6f, 0.95f));

    // ===================== BOTTOM BAR =====================
    const float BOT_H = 52.0f;
    float botY = sh - BOT_H;
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, 0.0f, botY, sw, BOT_H, panelColor, sw, sh);

    // --- Left: WAVE and TIME (redundant display for quick glance during gameplay) ---
    DrawText2D(waveText,  18.0f, botY + 8.0f,  0.85f, glm::vec3(0.3f, 0.9f, 1.0f));
    DrawText2D(timerText, 18.0f, botY + 28.0f, 0.85f, glm::vec3(1.0f, 0.85f, 0.3f));

    // --- Center: TURRET STATUS ---
    const float PIP_W = 60.0f, PIP_H = 18.0f, PIP_GAP = 20.0f;
    float turretsPipsTotalW = 3.0f * PIP_W + 2.0f * PIP_GAP;
    float turretsPipsStartX = sw / 2.0f - turretsPipsTotalW / 2.0f;
    
    DrawText2D("TURRETS:", turretsPipsStartX - 160.0f, botY + 18.0f, 0.85f, white);
    
    for (int i = 0; i < 3; i++) {
        float px = turretsPipsStartX + i * (PIP_W + PIP_GAP);
        glm::vec3 pipColor;
        if (turretHealth[i] <= 0) pipColor = lostColor;
        else if (turretHealth[i] > 66) pipColor = slotColors[0];
        else if (turretHealth[i] > 33) pipColor = slotColors[1];
        else pipColor = slotColors[2];
        
        drawPip2D(shaderProgram, quadVAO, quadFaceCount, px, botY + 17.0f, PIP_W, PIP_H, pipColor, sw, sh);
        
        std::string label = "T" + std::to_string(i + 1);
        float lw = textWidth(label, 0.65f);
        DrawText2D(label, px + PIP_W * 0.5f - lw * 0.5f, botY + 19.0f, 0.65f, pipLabelColor);
    }
}

void Renderer::DrawGameOverScreen(bool isWin, int score, int stars, int wave) {
    // Semitransparent black background
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, 0.0f, 0.0f, (float)screenWidth, (float)screenHeight, glm::vec3(0.05f, 0.05f, 0.08f), (float)screenWidth, (float)screenHeight);

    float centerX = (float)screenWidth / 2.0f;
    float centerY = (float)screenHeight / 2.0f;

    if (isWin) {
        DrawText2D("VICTORY!", centerX - 120.0f, centerY - 120.0f, 2.5f, glm::vec3(0.2f, 0.8f, 0.2f));
    } else {
        DrawText2D("GAME OVER", centerX - 140.0f, centerY - 120.0f, 2.5f, glm::vec3(0.86f, 0.30f, 0.26f));
    }
    
    std::string waveText = "FINAL WAVE: " + std::to_string(wave);
    DrawText2D(waveText, centerX - 110.0f, centerY - 40.0f, 1.2f);
    
    std::string scoreText = "FINAL SCORE: " + std::to_string(score);
    DrawText2D(scoreText, centerX - 120.0f, centerY, 1.2f);
    
    std::string starsText = "STARS: ";
    for (int i = 0; i < stars; i++) starsText += "* ";
    if (stars == 0) starsText += "NONE";
    DrawText2D(starsText, centerX - 80.0f, centerY + 40.0f, 1.2f, glm::vec3(0.95f, 0.78f, 0.25f));
    
    DrawText2D("PRESS SPACE TO RETURN TO MENU", centerX - 250.0f, centerY + 120.0f, 1.0f);
    DrawText2D("PRESS R TO RESTART", centerX - 150.0f, centerY + 160.0f, 1.0f);
}

void Renderer::DrawText2D(const std::string& text, float x, float y, float scale, glm::vec3 color) {
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
        drawLineList(shaderProgram, lineVertices, color);
    }
    
    glEnable(GL_DEPTH_TEST);
}

void Renderer::DrawMenuScreen() {
    float cx = screenWidth / 2.0f;
    float cy = screenHeight / 2.0f;
    
    DrawText2D("TANK DEFENDER 3D", cx - 180.0f, cy - 100.0f, 1.5f, glm::vec3(0.9f, 0.9f, 0.1f));
    
    // Play Button
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, cx - 60.0f, cy + 90.0f, 120.0f, 40.0f, glm::vec3(0.2f, 0.6f, 0.2f), screenWidth, screenHeight);
    DrawText2D("PLAY", cx - 35.0f, cy + 105.0f, 1.2f, glm::vec3(1.0f, 1.0f, 1.0f));
    
    // How To Play Button
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, cx - 120.0f, cy + 150.0f, 240.0f, 40.0f, glm::vec3(0.2f, 0.4f, 0.6f), screenWidth, screenHeight);
    DrawText2D("HOW TO PLAY", cx - 100.0f, cy + 165.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));
    
    // Quit Button
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, cx - 60.0f, cy + 210.0f, 120.0f, 40.0f, glm::vec3(0.6f, 0.2f, 0.2f), screenWidth, screenHeight);
    DrawText2D("QUIT", cx - 35.0f, cy + 225.0f, 1.2f, glm::vec3(1.0f, 1.0f, 1.0f));
}

void Renderer::DrawHowToPlayScreen() {
    float cx = screenWidth / 2.0f;
    float cy = screenHeight / 2.0f;
    
    // Draw background
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, 0.0f, 0.0f, (float)screenWidth, (float)screenHeight, glm::vec3(0.0f, 0.0f, 0.0f), screenWidth, screenHeight);

    DrawText2D("HOW TO PLAY", cx - 120.0f, cy - 100.0f, 1.2f, glm::vec3(0.9f, 0.9f, 0.1f));
    
    DrawText2D("P1 (GREEN): WASD TO MOVE, F TO SHOOT", cx - 250.0f, cy - 30.0f, 0.8f, glm::vec3(0.3f, 0.8f, 0.3f));
    DrawText2D("P2 (BLUE) : IJKL TO MOVE, ENTER/N TO SHOOT", cx - 250.0f, cy + 10.0f, 0.8f, glm::vec3(0.3f, 0.6f, 0.9f));
    DrawText2D("DEFEND THE 3 TURRETS FROM ENEMIES!", cx - 230.0f, cy + 50.0f, 0.8f, glm::vec3(0.9f, 0.8f, 0.2f));
    DrawText2D("IF ALL TURRETS ARE DESTROYED, YOU LOSE.", cx - 250.0f, cy + 80.0f, 0.8f, glm::vec3(0.8f, 0.3f, 0.3f));
    
    // Back Button
    drawBar2D(shaderProgram, quadVAO, quadFaceCount, cx - 60.0f, cy + 150.0f, 120.0f, 40.0f, glm::vec3(0.4f, 0.4f, 0.4f), screenWidth, screenHeight);
    DrawText2D("BACK", cx - 35.0f, cy + 165.0f, 1.2f, glm::vec3(1.0f, 1.0f, 1.0f));
}

void Renderer::Shutdown() {
    if (floorTexture != 0)  { glDeleteTextures(1, &floorTexture);  floorTexture  = 0; }
    if (tankTexture != 0)   { glDeleteTextures(1, &tankTexture);   tankTexture   = 0; }
    if (turretTexture != 0) { glDeleteTextures(1, &turretTexture); turretTexture = 0; }
    if (depthMap != 0)      { glDeleteTextures(1, &depthMap);      depthMap      = 0; }
    if (depthMapFBO != 0)   { glDeleteFramebuffers(1, &depthMapFBO); depthMapFBO = 0; }
    if (shaderProgram != 0)       { glDeleteProgram(shaderProgram);       shaderProgram       = 0; }
    if (shadowShaderProgram != 0) { glDeleteProgram(shadowShaderProgram); shadowShaderProgram = 0; }
    // Delete VAOs
    if (cubeVAO != 0)     glDeleteVertexArrays(1, &cubeVAO);
    if (sphereVAO != 0)   glDeleteVertexArrays(1, &sphereVAO);
    if (cylinderVAO != 0) glDeleteVertexArrays(1, &cylinderVAO);
    if (quadVAO != 0)     glDeleteVertexArrays(1, &quadVAO);
}
