#include "config.h"
#include "triangle_mesh.h"

unsigned int make_module(const std::string& filepath, unsigned int module_type) 
{
    std::ifstream file;
    std::stringstream bufferedLines;
    std::string line;

    file.open(filepath);
    while (std::getline(file, line))
    {
        bufferedLines << line << "\n";
    }
    std::string shaderSource = bufferedLines.str();
    const char* shaderSrc = shaderSource.c_str();
    bufferedLines.str("");
    file.close();

    unsigned int shaderModule = glCreateShader(module_type);
    glShaderSource(shaderModule, 1, &shaderSrc, NULL);
    glCompileShader(shaderModule);

    int success;
    glGetShaderiv(shaderModule, GL_COMPILE_STATUS, &success);
    if (!success) 
    {
        char errorLog[1024];
        glGetShaderInfoLog(shaderModule, 1024, NULL, errorLog);
        std::cout << "Shader Module compilation error:\n" << errorLog << std::endl;
	}

	return shaderModule;
}

unsigned int make_shader(const std:: string& vertex_filepath, const std::string& fragment_filepath)
{
    std::vector<unsigned int> modules;
    modules.push_back(make_module(vertex_filepath, GL_VERTEX_SHADER));
    modules.push_back(make_module(fragment_filepath, GL_FRAGMENT_SHADER));

    unsigned int shader = glCreateProgram();
    for (unsigned int shaderModule : modules)
    {
        glAttachShader(shader, shaderModule);
    }
    glLinkProgram(shader);

    int success;
    glGetProgramiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) 
    {
        char errorLog[1024];
        glGetProgramInfoLog(shader, 1024, NULL, errorLog);
        std::cout << "Shader Linking error:\n" << errorLog << std::endl;
	}

    for (unsigned int shaderModule : modules)
    {
        glDeleteShader(shaderModule);
    }

    return shader;
}

int main() {
    
    // std::ifstream file;
    // std::string line;

    // file.open("../../src/shaders/fragment.glsl");
    // while (std::getline(file, line))
    // {
    //     std::cout << line << std::endl;
    // }

    GLFWwindow* window;
    
    if (!glfwInit())
    {
        std::cout << "GLFW couldn't start" << std::endl;
        return -1;
    }

    window = glfwCreateWindow(640, 480, "Image Generater", NULL, NULL);
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        glfwTerminate();
        return -1;
    }

    // Window Color
    glClearColor(0.15f, 0.2f, 0.3f, 0.1f);

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    glViewport(0, 0, w, h);

    TriangleMesh* triangle = new TriangleMesh();

    unsigned int shader = make_shader("../../src/shaders/vertex.glsl", "../../src/shaders/fragment.glsl");

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader);
        triangle->draw();

        glfwSwapBuffers(window);
    }

    glDeleteProgram(shader);
    glfwTerminate();
    return 0;
}

