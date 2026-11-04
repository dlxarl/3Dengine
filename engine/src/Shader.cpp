#include "Shader.h"
#include "Log.h"

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

static std::string ReadTextFileOrThrow(const char* path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error(std::string("Cannot open shader file: ") + path);
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    if (content.empty())
    {
        throw std::runtime_error(std::string("Shader file is empty: ") + path);
    }

    return content;
}

Shader::Shader(const char* vertexPath, const char* fragmentPath)
{
    ID = 0;

    std::string vertexCode;
    std::string fragmentCode;

    try
    {
        vertexCode   = ReadTextFileOrThrow(vertexPath);
        fragmentCode = ReadTextFileOrThrow(fragmentPath);
    }
    catch (const std::exception& e)
    {
        Log::error("Shader", e.what());
        // НІЧОГО не компілюємо, інакше можна зловити crash у драйвері
        return;
    }

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, nullptr);
    glCompileShader(vertex);
    checkCompileErrors(vertex, "VERTEX");

    unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, nullptr);
    glCompileShader(fragment);
    checkCompileErrors(fragment, "FRAGMENT");

    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    checkCompileErrors(ID, "PROGRAM");

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

Shader::~Shader()
{
    if (ID != 0) glDeleteProgram(ID);
}

void Shader::use()
{
    // якщо шейдер не створився — не падаємо
    if (ID == 0) return;
    glUseProgram(ID);
}

int Shader::location(const std::string& name) const {
    auto found = locations.find(name);
    if (found != locations.end()) return found->second;

    const int resolved = glGetUniformLocation(ID, name.c_str());
    locations.emplace(name, resolved);
    return resolved;
}

void Shader::setBool(const std::string &name, bool value) const {
    if (ID == 0) return;
    glUniform1i(location(name), (int)value);
}

void Shader::setInt(const std::string &name, int value) const {
    if (ID == 0) return;
    glUniform1i(location(name), value);
}

void Shader::setFloat(const std::string &name, float value) const {
    if (ID == 0) return;
    glUniform1f(location(name), value);
}

void Shader::setMat4(const std::string &name, const glm::mat4 &mat) const {
    if (ID == 0) return;
    glUniformMatrix4fv(location(name), 1, GL_FALSE, &mat[0][0]);
}

void Shader::setVec2(const std::string &name, const glm::vec2 &value) const {
    if (ID == 0) return;
    glUniform2fv(location(name), 1, &value[0]);
}

void Shader::setVec3(const std::string &name, const glm::vec3 &value) const {
    if (ID == 0) return;
    glUniform3fv(location(name), 1, &value[0]);
}

void Shader::setVec4(const std::string &name, const glm::vec4 &value) const {
    if (ID == 0) return;
    glUniform4fv(location(name), 1, &value[0]);
}

void Shader::checkCompileErrors(unsigned int shader, std::string type)
{
    int success = 0;
    char infoLog[1024];

    if (type != "PROGRAM")
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            Log::error("Shader", type + " compilation failed:\n" + infoLog);
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
            Log::error("Shader", std::string("Linking failed:\n") + infoLog);
        }
    }
}
