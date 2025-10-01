#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
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

    int imgW, imgH, imgC;
    unsigned char* pixels = stbi_load("../../src/Images/G067PzSaEAAyuA1 (1).png", &imgW, &imgH, &imgC, 4);
    if (!pixels) { std::cerr << "Failed to load image!\n"; return -1; }

    std::mt19937_64 rng(std::chrono::high_resolution_clock::now().time_since_epoch().count());

    int width = 800, height = imgH / float(imgW) * width;

    std::uint32_t triCount = 2024 * 8;

    std::uniform_int_distribution<int> heightR(0, imgH);
    std::uniform_int_distribution<int> widthR(0, imgW);

    std::vector<Triangle> tris;
    for (size_t i = 0; i < triCount; i++) {
        tris.push_back({ {widthR(rng),heightR(rng)}, {widthR(rng),heightR(rng)}, {widthR(rng),heightR(rng)}, {0}, {0}, {0.0f,0.0f,0.0f,1.0f} });
    }

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

    // Input texture
    GLuint inputTex;
    glGenTextures(1, &inputTex);
    glBindTexture(GL_TEXTURE_2D, inputTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imgW, imgH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Compute shaders
    GLuint compProg = makeProgram({ makeShader(loadFile("../../src/shaders/compute.comp"), GL_COMPUTE_SHADER) });
    GLuint drawProg = makeProgram({ makeShader(loadFile("../../src/shaders/draw.comp"), GL_COMPUTE_SHADER) });

    // Output texture
    GLuint outputTex;
    glGenTextures(1, &outputTex);
    glBindTexture(GL_TEXTURE_2D, outputTex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, imgW, imgH);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Fullscreen quad shader
    const char* vsSrc = R"glsl(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aUV;
        out vec2 vUV;
        void main() { vUV = aUV; gl_Position = vec4(aPos,0,1); }
    )glsl";
    const char* fsSrc = R"glsl(
        #version 330 core
        in vec2 vUV;
        out vec4 oColor;
        uniform sampler2D uTex;
        void main() { oColor = texture(uTex, vUV); }
    )glsl";
    GLuint screenProg = makeProgram({ makeShader(vsSrc, GL_VERTEX_SHADER), makeShader(fsSrc, GL_FRAGMENT_SHADER) });

    float quad[] = {-1,-1, 0,1, 1,-1, 1,1, -1, 1, 0,0, 1, 1, 1,0};
    GLuint quadVBO, quadVAO;
    glGenBuffers(1, &quadVBO);
    glGenVertexArrays(1, &quadVAO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void*)(2*sizeof(float)));
    glBindVertexArray(0);

    glUseProgram(screenProg);
    glUniform1i(glGetUniformLocation(screenProg, "uTex"), 0);

    bool isRunning = false;
    Triangle* triangles;

    glViewport(0,0,width,height);
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glClearColor(0.1,0.1,0.1,1);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw fullscreen quad
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, outputTex);
        glUseProgram(screenProg);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS && !isRunning) isRunning = true;
        else if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) isRunning = false;

        if (isRunning) {
            tris.clear();
            for (size_t i = 0; i < triCount; i++) 
            {
                tris.push_back({ {widthR(rng),heightR(rng)}, {widthR(rng),heightR(rng)}, {widthR(rng),heightR(rng)}, {0}, {0}, {0.0f,0.0f,0.0f,1.0f} });
            }

            for (size_t i = 0; i < 50 ; i++)        
            {
                if (i != 0) 
                {
                    std::uniform_int_distribution<int> heightR2(-imgH * 0.01, imgH * 0.01);
                    std::uniform_int_distribution<int> widthR2(-imgW  * 0.01, imgW * 0.01);
                    tris.clear();
                    for (size_t i = 0; i < 50; i++)
                    {
                        for (size_t i = 0; i < 4; i++)
                            tris.push_back({ {triangles[i].p1.x + widthR2(rng), triangles[i].p1.y + heightR2(rng)}, {triangles[i].p2.x + widthR2(rng), triangles[i].p2.y + heightR2(rng)}, {triangles[i].p3.x + widthR2(rng), triangles[i].p3.y + heightR2(rng)}, {0}, {0}, {0.0f,0.0f,0.0f,1.0f} });
                    }
                }

                glGenBuffers(1, &triSSBO);
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, triSSBO);
                glBufferData(GL_SHADER_STORAGE_BUFFER, tris.size()*sizeof(Triangle), tris.data(), GL_DYNAMIC_DRAW);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, triSSBO);

                // Fitness compute shader
                glUseProgram(compProg);
                glUniform2i(glGetUniformLocation(compProg,"imageSize"), imgW,imgH);
                glBindImageTexture(0,inputTex,0,GL_FALSE,0,GL_READ_ONLY,GL_RGBA8);
                glBindImageTexture(1, outputTex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
                glDispatchCompute((GLuint)triCount,1,1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                // Map buffers
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, fitSSBO);
                float* fitness = (float*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER,0,tris.size()*sizeof(float),GL_MAP_READ_BIT);
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, triSSBO);
                triangles = (Triangle*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER,0,tris.size()*sizeof(Triangle),GL_MAP_READ_BIT|GL_MAP_WRITE_BIT);

                // Sort
                for (size_t i=0;i<tris.size()-1;i++) 
                    for (size_t j=0;j<tris.size()-i-1;j++) 
                        if (fitness[j]<fitness[j+1]) { std::swap(fitness[j],fitness[j+1]); std::swap(triangles[j],triangles[j+1]); }

                glBindBuffer(GL_SHADER_STORAGE_BUFFER, fitSSBO);
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, triSSBO);
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            }

            // Draw compute shader
            glUseProgram(drawProg);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, triSSBO);
            glBindImageTexture(0,outputTex,0,GL_FALSE,0, GL_READ_WRITE,GL_RGBA8);
            glUniform2i(glGetUniformLocation(drawProg,"imageSize"), imgW,imgH);
            glDispatchCompute((imgW+15)/16,(imgH+15)/16,1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

            std::cout << "Shaders ran!\n";
        }

        glfwSwapBuffers(window);
    }

    // Cleanup
    glDeleteProgram(compProg);
    glDeleteProgram(drawProg);
    glDeleteProgram(screenProg);
    glDeleteBuffers(1,&triSSBO);
    glDeleteBuffers(1,&fitSSBO);
    glDeleteBuffers(1,&quadVBO);
    glDeleteVertexArrays(1,&quadVAO);
    glDeleteTextures(1,&outputTex);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
