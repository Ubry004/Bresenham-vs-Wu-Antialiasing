#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <sstream>

#include <vector>
#include <cmath>

// Vertex structure for Xiaolin Wu lines
struct Vertex {
    float x, y;
    float r, g, b;
    float alpha;
};

// Line structure for generating radial lines
struct Line {
    float x0, y0, x1, y1;
};

// Generate radial lines from center (x0, y0) with given radius and angle step
std::vector<Line> generateLines(int x0, int y0, int radius, int angleStep) {
    std::vector<Line> lines;
    for (int angle = 0; angle < 360; angle += angleStep) {
        double rad = angle * (3.14159) / 180.0;
        float x1 = x0 + static_cast<float>(radius * cos(rad));
        float y1 = y0 + static_cast<float>(radius * sin(rad));
        lines.push_back({ static_cast<float>(x0), static_cast<float>(y0), x1, y1 });
    }
    return lines;
}

// Bresenham lines
// Only uses integer arithmetic (other than NDC conversion)
std::vector<float> bresenhamLine(int x0, int y0, int x1, int y1, int width, int height) {
    std::vector<float> vertices;

    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (true) {
        float ndcX = (2.0f * (x0 + 0.5f)) / width - 1.0f;
        float ndcY = (2.0f * (y0 + 0.5f)) / height - 1.0f;
        vertices.push_back(ndcX);
        vertices.push_back(ndcY);

        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }

    return vertices;
}

// Xiaolin Wu antialiasing
std::vector<Vertex> xiaolinWuLine(float x0, float y0, float x1, float y1,
    int width, int height,
    float r = 1.0f, float g = 0.0f, float b = 1.0f) {
    std::vector<Vertex> vertices;

	// Helper functions
	// fractional part (the bottom pixel coverage)
	// note the use of std::floor to ensure correct behavior for negative values
    auto fpart = [](float x) { return x - std::floor(x); };

	// reverse fractional part (the top pixel coverage)
	// this is a lambda to capture fpart
	// we do this to avoid recomputing 1.0 - fpart(x) multiple times
    auto rfpart = [&](float x) { return 1.0f - fpart(x); };

	// Step 1 : Handle steep lines
	// A line is steep if the absolute slope is greater than 1

    bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);

    float X0 = x0, Y0 = y0, X1 = x1, Y1 = y1;
    if (steep) {
        std::swap(X0, Y0);
        std::swap(X1, Y1);
    }
    if (X0 > X1) {
        std::swap(X0, X1);
        std::swap(Y0, Y1);
    }

	// Step 2 : Compute the line parameters
	// This means calculating the slope (gradient)

    float dx = X1 - X0;
    float dy = Y1 - Y0;
    float gradient = (dx == 0.0f) ? 1.0f : (dy / dx);

	// Step 3 : Handle the endpoints
	// We need to handle the first and last pixels separately

    // First endpoint
    float xend = std::floor(X0);
    float yend = Y0 + gradient * (xend - X0);
    float xgap = 1.0f - (X0 - xend);
    float xpxl1 = xend;
    float ypxl1 = std::floor(yend);

    auto pushVertex = [&](float ndcX, float ndcY, float alpha) {
        vertices.push_back({ ndcX, ndcY, r, g, b, alpha });
    };

    if (steep) {
        float ndcX1 = (2.0f * (ypxl1 + 0.5f)) / width - 1.0f;
        float ndcY1 = (2.0f * (xpxl1 + 0.5f)) / height - 1.0f;
        pushVertex(ndcX1, ndcY1, rfpart(yend) * xgap);

        float ndcX2 = (2.0f * (ypxl1 + 1 + 0.5f)) / width - 1.0f;
        float ndcY2 = (2.0f * (xpxl1 + 0.5f)) / height - 1.0f;
        pushVertex(ndcX2, ndcY2, fpart(yend) * xgap);
    }
    else {
        float ndcX1 = (2.0f * (xpxl1 + 0.5f)) / width - 1.0f;
        float ndcY1 = (2.0f * (ypxl1 + 0.5f)) / height - 1.0f;
        pushVertex(ndcX1, ndcY1, rfpart(yend) * xgap);

        float ndcX2 = (2.0f * (xpxl1 + 0.5f)) / width - 1.0f;
        float ndcY2 = (2.0f * (ypxl1 + 1 + 0.5f)) / height - 1.0f;
        pushVertex(ndcX2, ndcY2, fpart(yend) * xgap);
    }

	float intery = yend + gradient; // first y-intersection for the main loop, after the first endpoint

    // Second endpoint
    xend = std::ceil(X1);
    yend = Y1 + gradient * (xend - X1);
    xgap = 1.0f - (X1 - xend);
    float xpxl2 = xend;
    float ypxl2 = std::floor(yend);

    if (steep) {
        float ndcX1 = (2.0f * (ypxl2 + 0.5f)) / width - 1.0f;
        float ndcY1 = (2.0f * (xpxl2 + 0.5f)) / height - 1.0f;
        pushVertex(ndcX1, ndcY1, rfpart(yend) * xgap);

        float ndcX2 = (2.0f * (ypxl2 + 1 + 0.5f)) / width - 1.0f;
        float ndcY2 = (2.0f * (xpxl2 + 0.5f)) / height - 1.0f;
        pushVertex(ndcX2, ndcY2, fpart(yend) * xgap);
    }
    else {
        float ndcX1 = (2.0f * (xpxl2 + 0.5f)) / width - 1.0f;
        float ndcY1 = (2.0f * (ypxl2 + 0.5f)) / height - 1.0f;
        pushVertex(ndcX1, ndcY1, rfpart(yend) * xgap);

        float ndcX2 = (2.0f * (xpxl2 + 0.5f)) / width - 1.0f;
        float ndcY2 = (2.0f * (ypxl2 + 1 + 0.5f)) / height - 1.0f;
        pushVertex(ndcX2, ndcY2, fpart(yend) * xgap);
    }

    // Main loop
	// Step 4 : Draw the line
	// We must now draw the pixels between the two endpoints
    if (steep) {
        for (int x = int(xpxl1) + 1; x < int(xpxl2); ++x) {
            float y = intery;
			float ndcX1 = (2.0f * (std::floor(y) + 0.5f)) / width - 1.0f; // we swap x and y here (we are in steep mode)
            float ndcY1 = (2.0f * (x + 0.5f)) / height - 1.0f;
            pushVertex(ndcX1, ndcY1, rfpart(y));

            float ndcX2 = (2.0f * (std::floor(y) + 1 + 0.5f)) / width - 1.0f;
            float ndcY2 = (2.0f * (x + 0.5f)) / height - 1.0f;
            pushVertex(ndcX2, ndcY2, fpart(y));

            intery += gradient;
        }
    }
    else {
        for (int x = int(xpxl1) + 1; x < int(xpxl2); ++x) {
            float y = intery;
            float ndcX1 = (2.0f * (x + 0.5f)) / width - 1.0f;
            float ndcY1 = (2.0f * (std::floor(y) + 0.5f)) / height - 1.0f;
            pushVertex(ndcX1, ndcY1, rfpart(y));

            float ndcX2 = (2.0f * (x + 0.5f)) / width - 1.0f;
            float ndcY2 = (2.0f * (std::floor(y) + 1 + 0.5f)) / height - 1.0f;
            pushVertex(ndcX2, ndcY2, fpart(y));

            intery += gradient;
        }
    }

    return vertices;
}


// Shader loader
std::string loadShaderSource(const char* filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Could not open shader file: " << filePath << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// GLFW callbacks
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

// settings
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Curve switch
int CURVE = 0; // Begins with sine wave

int main()
{
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Bresenham Lines", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR);

    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

	// **** IMPORTANT: ****
    // Depth Test is disabled to ensure proper blending of Wu lines. This will make it look ugly (try it)
	// In a real application you would probably want to sort
	// the lines by depth before drawing to avoid this issue.

	// Characteristics of the lines when Depth Test is enabled:
	// Flickering lines when they overlap
	// Blending will not work correctly, leading to other visual artifacts

    // configure global opengl state
    //glEnable(GL_DEPTH_TEST);

    // Shaders
    std::string vertexCode = loadShaderSource("vertex_shader.glsl");
    std::string fragmentCode = loadShaderSource("fragment_shader.glsl");

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vShaderCode, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fShaderCode, NULL);
    glCompileShader(fragmentShader);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Generate initial vertices
    std::vector<float> verticesBresenham = bresenhamLine(50, 50, 750, 550, SCR_WIDTH, SCR_HEIGHT);
    std::vector<Vertex> verticesWu = xiaolinWuLine(50.0f, 50.0f, 750.0f, 550.0f, SCR_WIDTH, SCR_HEIGHT);

    // Create VAO & VBO for Bresenham
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, verticesBresenham.size() * sizeof(float), verticesBresenham.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Create VAO & VBO for Xiaolin Wu
    GLuint VAOWu, VBOWu;
    glGenVertexArrays(1, &VAOWu);
    glGenBuffers(1, &VBOWu);

    glBindVertexArray(VAOWu);
    glBindBuffer(GL_ARRAY_BUFFER, VBOWu);
    glBufferData(GL_ARRAY_BUFFER, verticesWu.size() * sizeof(Vertex), verticesWu.data(), GL_DYNAMIC_DRAW);

    // Position attribute (x,y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    // Color attribute (r,g,b,a)
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Time uniform
    GLint timeLoc = glGetUniformLocation(shaderProgram, "time");

    // Just to make it bigger
    glPointSize(1.0f);

    // render loop
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);
        glUseProgram(shaderProgram);

        // ---------- Animate sine wave ----------
        int x_start = 50;
        int x_end = SCR_WIDTH - 50;
        float amplitude = 200.0f;  // pixels
        float frequency = 0.01f;   // controls wavelength
        float phase = currentFrame; // animate

		// ---------- Radial lines ----------
        int radius = 800;
        int angleStep = 15; // every 15 degrees

        // ---------- Generate vertices depending on CURVE ----------

        std::vector<Vertex> verticesWu;
        std::vector<float> verticesBresenham;

        if (CURVE == 0) {
            // Sine wave
            // Note that we sample at every pixel column so Wu can blend adjacent pixels
            int num_points = x_end - x_start + 1; // one sample per pixel
            for (int i = 0; i < num_points; ++i) {
                float x = x_start + i; 
                float y = SCR_HEIGHT / 2 + amplitude * sin(frequency * x + phase);

                // Bresenham: just pixel centers (one vertex per column)
                float ndcX = (2.0f * (x + 0.5f)) / SCR_WIDTH - 1.0f;
                float ndcY = (2.0f * (y + 0.5f)) / SCR_HEIGHT - 1.0f;
                verticesBresenham.push_back(ndcX);
                verticesBresenham.push_back(ndcY);

                // Wu: use floor + fractional part (dont round)
                float y_floor = std::floor(y);
                float frac = y - y_floor; // 0..1
                float ndcXw = (2.0f * (x + 0.5f)) / SCR_WIDTH - 1.0f;

                float ndcY1 = (2.0f * (y_floor + 0.5f)) / SCR_HEIGHT - 1.0f;        // lower (floor) pixel
                verticesWu.push_back({ ndcXw, ndcY1, 1.0f, 0.0f, 1.0f, (1.0f - frac) });

                float ndcY2 = (2.0f * (y_floor + 1 + 0.5f)) / SCR_HEIGHT - 1.0f;    // upper (ceil) pixel
                verticesWu.push_back({ ndcXw, ndcY2, 1.0f, 0.0f, 1.0f, frac });
            }
        }
        else {
            auto lines = generateLines(SCR_WIDTH / 2, SCR_HEIGHT / 2, radius, angleStep);

            for (const auto& line : lines) {
                // Bresenham needs integer endpoints; round subpixel endpoints for it
                auto bresenhamVerts = bresenhamLine(
                    static_cast<int>(std::round(line.x0)),
                    static_cast<int>(std::round(line.y0)),
                    static_cast<int>(std::round(line.x1)),
                    static_cast<int>(std::round(line.y1)),
                    SCR_WIDTH, SCR_HEIGHT);
                verticesBresenham.insert(verticesBresenham.end(), bresenhamVerts.begin(), bresenhamVerts.end());

                // Wu: pass float endpoints so algorithm computes correct fractional coverage
                auto wuVerts = xiaolinWuLine(line.x0, line.y0, line.x1, line.y1, SCR_WIDTH, SCR_HEIGHT, 1.0f, 0.0f, 1.0f);
                verticesWu.insert(verticesWu.end(), wuVerts.begin(), wuVerts.end());
            }
        }

        // ---------- Cell 1: Top-left (Bresenham, white background) ----------
        glViewport(0, SCR_HEIGHT / 2, SCR_WIDTH / 2, SCR_HEIGHT / 2);
        glEnable(GL_SCISSOR_TEST);

        // For Bresenham draws we don't have per-vertex color attribute data,
        // so we use the generic vertex attribute value at location 1 to supply a constant color.
        glBindVertexArray(VAO);
        glDisableVertexAttribArray(1); // ensure attribute array 1 is disabled for this VAO
        glVertexAttrib4f(1, 0.0f, 0.0f, 0.0f, 1.0f); // black

        glScissor(0, SCR_HEIGHT / 2, SCR_WIDTH / 2, SCR_HEIGHT / 2);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, verticesBresenham.size() * sizeof(float),
            verticesBresenham.data(), GL_DYNAMIC_DRAW);
        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, verticesBresenham.size() / 2);

        // ---------- Cell 2: Bottom-left (Bresenham, black background) ----------
        glViewport(0, 0, SCR_WIDTH / 2, SCR_HEIGHT / 2);
        glEnable(GL_SCISSOR_TEST);

        // Set constant yellow for this draw
        glBindVertexArray(VAO);
        glDisableVertexAttribArray(1);
        glVertexAttrib4f(1, 1.0f, 1.0f, 0.0f, 1.0f); // yellow

        glScissor(0, 0, SCR_WIDTH / 2, SCR_HEIGHT / 2);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, verticesBresenham.size() * sizeof(float),
            verticesBresenham.data(), GL_DYNAMIC_DRAW);
        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, verticesBresenham.size() / 2);


        // ---------- Wu rendering ----------
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Cell 3: Top-right (Wu, white background)
        glViewport(SCR_WIDTH / 2, SCR_HEIGHT / 2, SCR_WIDTH / 2, SCR_HEIGHT / 2);
        glEnable(GL_SCISSOR_TEST);
        glScissor(SCR_WIDTH / 2, SCR_HEIGHT / 2, SCR_WIDTH / 2, SCR_HEIGHT / 2);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);

        glBindBuffer(GL_ARRAY_BUFFER, VBOWu);
        glBufferData(GL_ARRAY_BUFFER, verticesWu.size() * sizeof(Vertex),
            verticesWu.data(), GL_DYNAMIC_DRAW);
        glBindVertexArray(VAOWu);
        glDrawArrays(GL_POINTS, 0, verticesWu.size());

        // Cell 4: Bottom-right (Wu, black background)
        glViewport(SCR_WIDTH / 2, 0, SCR_WIDTH / 2, SCR_HEIGHT / 2);
        glEnable(GL_SCISSOR_TEST);
        glScissor(SCR_WIDTH / 2, 0, SCR_WIDTH / 2, SCR_HEIGHT / 2);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);

        glBindBuffer(GL_ARRAY_BUFFER, VBOWu);
        glBufferData(GL_ARRAY_BUFFER, verticesWu.size() * sizeof(Vertex),
            verticesWu.data(), GL_DYNAMIC_DRAW);
        glBindVertexArray(VAOWu);
        glDrawArrays(GL_POINTS, 0, verticesWu.size());

        glDisable(GL_BLEND);

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Clean up
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAOWu);
    glDeleteBuffers(1, &VBOWu);

    glfwTerminate();
    return 0;
}

// process all input
void processInput(GLFWwindow* window)
{
    static bool cWasPressed = false;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    int cState = glfwGetKey(window, GLFW_KEY_C);
    if (cState == GLFW_PRESS && !cWasPressed) {
        CURVE = (CURVE + 1) % 2; // Toggle between 0 and 1
        std::cout << "CURVE switched to " << CURVE << std::endl;
        cWasPressed = true;
    }
    if (cState == GLFW_RELEASE) {
        cWasPressed = false;
    }
}

// framebuffer resize callback
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// mouse callback
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {}

// scroll callback
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {}
