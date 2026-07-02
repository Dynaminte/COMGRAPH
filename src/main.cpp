#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "Game.h"
#include <iostream>

const int WINDOW_WIDTH = 1200;
const int WINDOW_HEIGHT = 800;

// Fullscreen state
static bool isFullscreen = false;
static int windowedX, windowedY, windowedW, windowedH;
static bool f11WasPressed = false;

void toggleFullscreen(GLFWwindow* window) {
    if (!isFullscreen) {
        // Simpan posisi & ukuran window sekarang
        glfwGetWindowPos(window, &windowedX, &windowedY);
        glfwGetWindowSize(window, &windowedW, &windowedH);

        // Masuk fullscreen di monitor utama
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window, monitor, 0, 0,
                             mode->width, mode->height, mode->refreshRate);
        isFullscreen = true;
    } else {
        // Kembali ke windowed
        glfwSetWindowMonitor(window, nullptr,
                             windowedX, windowedY,
                             windowedW, windowedH, 0);
        isFullscreen = false;
    }
}

int main() {
    // Inisialisasi GLFW
    if (!glfwInit()) {
        std::cerr << "GLFW initialization failed!" << std::endl;
        return -1;
    }

    // Setup OpenGL context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // Buat window
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
                                         "Tank Defender 3D", nullptr, nullptr);
    if (!window) {
        std::cerr << "Window creation failed!" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Simpan posisi awal windowed
    glfwGetWindowPos(window, &windowedX, &windowedY);
    windowedW = WINDOW_WIDTH;
    windowedH = WINDOW_HEIGHT;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // VSync

    // Inisialisasi GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "GLEW initialization failed!" << std::endl;
        return -1;
    }

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Inisialisasi game
    Game game(WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!game.Initialize()) {
        std::cerr << "Game initialization failed!" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Game loop
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        float deltaTime = (float)(currentTime - lastTime);
        lastTime = currentTime;

        // Toggle fullscreen dengan F11
        bool f11Now = (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS);
        if (f11Now && !f11WasPressed) {
            toggleFullscreen(window);
            // Update viewport sesuai ukuran window baru
            int w, h;
            glfwGetFramebufferSize(window, &w, &h);
            glViewport(0, 0, w, h);
        }
        f11WasPressed = f11Now;

        // Input
        game.ProcessInput(window);

        // Update
        game.Update(deltaTime);

        // Render
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        
        // Notify game of current size
        game.Resize(fbW, fbH);
        
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        game.Render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    game.Shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
