#pragma once
#include <glad/glad.h>
#include <string>

class Texture {
public:
    unsigned int ID;
    std::string type;
    std::string path;
    std::string assetPath;  // project-relative path when loaded through AssetManager
    bool loaded = false;

    Texture(const char* path, const std::string& type);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    void bind(int unit);
};