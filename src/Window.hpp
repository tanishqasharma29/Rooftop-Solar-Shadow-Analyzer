#pragma once

#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Camera.hpp"

class Window
{
public:

    Window(int width, int height, const char* title);

    ~Window();

    bool shouldClose();

    void swapBuffers();

    void pollEvents();

    GLFWwindow* getNativeWindow();

private:

    GLFWwindow* window;

    static void framebufferCallback(
        GLFWwindow* window,
        int width,
        int height
    );

    static void mouseCallback(
        GLFWwindow* window,
        double xpos,
        double ypos
    );

    static void scrollCallback(
        GLFWwindow* window,
        double xoffset,
        double yoffset
    );
};
