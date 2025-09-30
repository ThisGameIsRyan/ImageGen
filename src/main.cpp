// main.cpp - fixed + fullscreen display of outputTex
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <random>
#include <chrono>

std::string loadFile(const std::string& path) {
    std::ifstream file(path);
    std::stringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

GLuint makeShader(const std::string& src, GLenum type) {
    GLuint shader = glCreateShader(type);
    const char* code = src.c_str();
    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, log);
        std::cerr << "Shader compile error:\n" << log << std::endl;
    }
    return shader;
}

GLuint makeProgram(std::initializer_list<GLuint> shaders) {
    GLuint prog = glCreateProgram();
    for (auto s : shaders) glAttachShader(prog, s);
    glLinkProgram(prog);

    int success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetProgramInfoLog(prog, 1024, nullptr, log);
        std::cerr << "Program link error:\n" << log << std::endl;
    }
    return prog;
}

struct IVec2 { int x, y; };
struct Vec4 { float r, g, b, a; };

struct Triangle {
    IVec2 p1;
    IVec2 p2;
    IVec2 p3;
    int _pad0;
    int _pad1;
    alignas(16) Vec4 color;
};

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Triangle Evolution :3", nullptr, nullptr);
    if (!window) { std::cerr << "Failed create window\n"; return -1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to init GL loader\n";
        return -1;
    }

    std::mt19937_64 rng(std::chrono::high_resolution_clock::now().time_since_epoch().count());

    int width = 800, height = 600;
    std::uint32_t triCount = 2024 * 2;
    std::uniform_int_distribution<int> heightR(0, height);
    std::uniform_int_distribution<int> widthR(0, width);

    std::vector<Triangle> tris;
    tris.reserve(triCount);
    for (size_t i = 0; i < triCount; i++) {
        tris.push_back({ {widthR(rng),heightR(rng)}, {widthR(rng),heightR(rng)}, {widthR(rng),heightR(rng)}, {0}, {0}, {0.0f,0.0f,0.0f,1.0f} });
    }
    std::cout << "Triangle count = " << tris.size() << "\n";

    // SSBOs
    GLuint triSSBO;
    glGenBuffers(1, &triSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, triSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, tris.size()*sizeof(Triangle), tris.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, triSSBO);

    GLuint fitSSBO;
    glGenBuffers(1, &fitSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, fitSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, tris.size()*sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, fitSSBO);

    std::string compSrc = loadFile("../../src/shaders/compute.comp");
    GLuint compProg = makeProgram({ makeShader(compSrc, GL_COMPUTE_SHADER) });

    // Run fittnes comp
    glUseProgram(compProg);
    glUniform2i(glGetUniformLocation(compProg, "imageSize"), width, height);
    glDispatchCompute((GLuint)triCount, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, fitSSBO);
    float* fitness = (float*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, tris.size()*sizeof(float), GL_MAP_READ_BIT);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, triSSBO);
    Triangle* triangles = (Triangle*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, tris.size()*sizeof(Triangle), GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);

    // bubble sort
    for (size_t i = 0; i < tris.size() - 1; ++i) {
        for (size_t j = 0; j < tris.size() - i - 1; ++j) {
            if (fitness[j] < fitness[j + 1]) {
                // swap fitness
                float tmpF = fitness[j];
                fitness[j] = fitness[j + 1];
                fitness[j + 1] = tmpF;
                // swap triangles
                Triangle tmpT = triangles[j];
                triangles[j] = triangles[j + 1];
                triangles[j + 1] = tmpT;
            }
        }
    }

    std::cout << "Top fitness = " << fitness[0] << "\n";
    std::cout << "Top triangle pos: (" << triangles[0].p1.x << "," << triangles[0].p1.y << ") "
              << "(" << triangles[0].p2.x << "," << triangles[0].p2.y << ") "
              << "(" << triangles[0].p3.x << "," << triangles[0].p3.y << ")\n";
    std::cout << "Top color: (" << triangles[0].color.r << "," << triangles[0].color.g << "," << triangles[0].color.b << "," << triangles[0].color.a << ")\n";

    // Unmap buffers
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, fitSSBO);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, triSSBO);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

    GLuint outputTex;
    glGenTextures(1, &outputTex);
    glBindTexture(GL_TEXTURE_2D, outputTex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    std::string drawSrc = loadFile("../../src/shaders/draw.comp");
    GLuint drawProg = makeProgram({ makeShader(drawSrc, GL_COMPUTE_SHADER) });
    glUseProgram(drawProg);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, triSSBO);
    glBindImageTexture(0, outputTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    glUniform2i(glGetUniformLocation(drawProg, "imageSize"), width, height);
    glDispatchCompute((width + 15) / 16, (height + 15) / 16, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Vertex shader
    const char* vsSrc = R"glsl(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aUV;
        out vec2 vUV;
        void main() {
            vUV = aUV;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )glsl";
    // Fragment shader
    const char* fsSrc = R"glsl(
        #version 330 core
        in vec2 vUV;
        out vec4 oColor;
        uniform sampler2D uTex;
        void main() {
            oColor = texture(uTex, vUV);
        }
    )glsl";

    GLuint vs = makeShader(vsSrc, GL_VERTEX_SHADER);
    GLuint fs = makeShader(fsSrc, GL_FRAGMENT_SHADER);
    GLuint screenProg = makeProgram({ vs, fs });

    // fullscreen quad
    float quad[] = {
        // x, y,   u, v
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    GLuint quadVBO, quadVAO;
    glGenBuffers(1, &quadVBO);
    glGenVertexArrays(1, &quadVAO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    glUseProgram(screenProg);
    glUniform1i(glGetUniformLocation(screenProg, "uTex"), 0);

    // Main loop
    glViewport(0, 0, width, height);
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, outputTex);

        glUseProgram(screenProg);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
    }

    // Add to the trash pile
    glDeleteProgram(compProg);
    glDeleteProgram(drawProg);
    glDeleteProgram(screenProg);
    glDeleteBuffers(1, &triSSBO);
    glDeleteBuffers(1, &fitSSBO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteTextures(1, &outputTex);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
