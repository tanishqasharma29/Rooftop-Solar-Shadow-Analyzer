
#include "Window.hpp"
#include <stdexcept>
#include <iostream>

Window::Window(
    int width,
    int height,
    const char* title
)
{
    if (!glfwInit())
    {
        throw std::runtime_error(
            "Failed to initialize GLFW"
        );
    }

    // Request OpenGL 3.3 Core Profile.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    glfwWindowHint(
        GLFW_OPENGL_PROFILE,
        GLFW_OPENGL_CORE_PROFILE
    );

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Enable depth buffering.
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    window = glfwCreateWindow(
        width,
        height,
        title,
        nullptr,
        nullptr
    );

    if (!window)
    {
        glfwTerminate();

        throw std::runtime_error(
            "Failed to create GLFW window"
        );
    }

    glfwMakeContextCurrent(window);

    // Load OpenGL functions through GLAD.
    int version = gladLoadGL(
        (GLADloadfunc)glfwGetProcAddress
    );

    if (version == 0)
    {
        glfwDestroyWindow(window);
        glfwTerminate();

        throw std::runtime_error(
            "Failed to initialize GLAD"
        );
    }

    std::cout << "OpenGL initialized successfully!\n";

    std::cout << "Loaded OpenGL "
              << GLAD_VERSION_MAJOR(version)
              << "."
              << GLAD_VERSION_MINOR(version)
              << "\n";

    // Enable V-Sync.
    glfwSwapInterval(1);

    // Enable depth testing so nearer objects hide farther objects.
    glEnable(GL_DEPTH_TEST);

    // Enable mouse capture for free-fly camera.
    glfwSetInputMode(
        window,
        GLFW_CURSOR,
        GLFW_CURSOR_NORMAL
    );

    glfwSetFramebufferSizeCallback(
        window,
        framebufferCallback
    );

    glfwSetCursorPosCallback(
        window,
        mouseCallback
    );

    glfwSetScrollCallback(
        window,
        scrollCallback
    );
}

Window::~Window()
{
    if (window)
        glfwDestroyWindow(window);

    glfwTerminate();
}

bool Window::shouldClose()
{
    return glfwWindowShouldClose(window);
}

void Window::swapBuffers()
{
    glfwSwapBuffers(window);
}

void Window::pollEvents()
{
    glfwPollEvents();
}

GLFWwindow* Window::getNativeWindow()
{
    return window;
}

void Window::framebufferCallback(
    GLFWwindow* window,
    int width,
    int height
)
{
    (void)window;

    // Update the viewport when the window is resized.
    glViewport(0, 0, width, height);
}

void Window::mouseCallback(
    GLFWwindow* window,
    double xpos,
    double ypos
)
{
    static bool firstMouse = true;

    static float lastX = 400.0f;
    static float lastY = 300.0f;

    float x = static_cast<float>(xpos);
    float y = static_cast<float>(ypos);

    // Always update the last position.
    // This prevents sudden jumps when camera mode is enabled.
    if (firstMouse)
    {
        lastX = x;
        lastY = y;
        firstMouse = false;
        return;
    }

    float xOffset = x - lastX;
    float yOffset = lastY - y;

    lastX = x;
    lastY = y;

    // Move camera only when mouse is captured.
    if (glfwGetInputMode(window, GLFW_CURSOR)
        != GLFW_CURSOR_DISABLED)
    {
        return;
    }

    Camera* camera = static_cast<Camera*>(
        glfwGetWindowUserPointer(window)
    );

    if (camera)
    {
        camera->ProcessMouseMovement(
            xOffset,
            yOffset
        );
    }
}


void Window::scrollCallback(
    GLFWwindow* window,
    double xoffset,
    double yoffset
)
{
    (void)xoffset;

    Camera* camera = static_cast<Camera*>(
        glfwGetWindowUserPointer(window)
    );

    if (camera)
    {
        camera->ProcessMouseScroll(
            static_cast<float>(yoffset)
        );
    }
}
