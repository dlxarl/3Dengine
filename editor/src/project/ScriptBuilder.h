#pragma once
#include "Project.h"

#include <filesystem>
#include <string>
#include <vector>

struct ScriptInfo {
    std::string name;                  // the class, as an object's Script field names it
    std::filesystem::path file;
};

namespace ScriptBuilder {
    // The scripts the project's source files declare with AUREON_SCRIPT, by reading the files.
    std::vector<ScriptInfo> scan(const Project& project);

    // Compiles every .cpp under Assets into one library. An empty library path with success
    // means the project has no sources. On failure the compiler's output is in log.
    bool build(const Project& project, std::filesystem::path& library, std::string& log);

    struct Template {
        const char* name;          // also the file name and the class
        const char* description;
        const char* source;
    };
    const std::vector<Template>& templates();

    // Writes a template into folder under a new class name; returns the file, or empty with error set.
    std::filesystem::path createFromTemplate(const Template& source, const std::string& className,
                                             const std::filesystem::path& folder, std::string& error);
}
