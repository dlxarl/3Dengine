#include "ScriptBuilder.h"
#include "Log.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {
    std::string quote(const std::string& text) {
        std::string result = "'";
        for (char c : text) {
            if (c == '\'') result += "'\\''";
            else result += c;
        }
        return result + "'";
    }

    std::vector<fs::path> sources(const Project& project) {
        std::vector<fs::path> found;
        std::error_code ec;
        for (fs::recursive_directory_iterator it(project.assetsDir(), ec), end; !ec && it != end; it.increment(ec)) {
            if (it->is_regular_file(ec) && it->path().extension() == ".cpp") found.push_back(it->path());
        }
        std::sort(found.begin(), found.end());
        return found;
    }

    std::string readAll(const fs::path& path) {
        std::ifstream in(path, std::ios::binary);
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }
}

std::vector<ScriptInfo> ScriptBuilder::scan(const Project& project) {
    static const std::regex declaration(R"(AUREON_SCRIPT\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\))");
    std::vector<ScriptInfo> found;
    for (const fs::path& file : sources(project)) {
        const std::string text = readAll(file);
        for (auto it = std::sregex_iterator(text.begin(), text.end(), declaration); it != std::sregex_iterator(); ++it) {
            found.push_back({(*it)[1].str(), file});
        }
    }
    return found;
}

bool ScriptBuilder::build(const Project& project, fs::path& library, std::string& log) {
    library.clear();
    const std::vector<fs::path> files = sources(project);
    if (files.empty()) return true;

#if defined(_WIN32)
    log = "Scripts are not supported on this platform yet.";
    return false;
#else
    const fs::path outputDir = project.libraryDir() / "Scripts";
    std::error_code ec;
    fs::create_directories(outputDir, ec);

    // A new name every time, because the system keeps a library of the same name loaded.
    static int counter = 0;
    for (const auto& old : fs::directory_iterator(outputDir, ec)) {
        if (old.path().extension() == ".dylib" || old.path().extension() == ".so") fs::remove(old.path(), ec);
    }
    const fs::path output = outputDir / ("scripts_" + std::to_string(++counter) + ".dylib");

    std::string command = "clang++ -std=c++17 -O1 -dynamiclib -undefined dynamic_lookup -fvisibility=default";
    std::stringstream includes(ENGINE_SCRIPT_INCLUDES);
    for (std::string dir; std::getline(includes, dir, '|');) {
        if (!dir.empty()) command += " -I" + quote(dir);
    }
    for (const fs::path& file : files) command += " " + quote(file.string());
    command += " -o " + quote(output.string()) + " 2>&1";

    Log::info("Scripts", "Compiling " + std::to_string(files.size()) + " script file(s)");
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        log = "Cannot start the compiler (clang++). Install the Xcode Command Line Tools.";
        return false;
    }
    char buffer[512];
    log.clear();
    while (fgets(buffer, sizeof(buffer), pipe)) log += buffer;
    const int status = pclose(pipe);

    if (status != 0 || !fs::exists(output, ec)) {
        if (log.empty()) log = "The compiler failed without a message.";
        return false;
    }
    library = output;
    return true;
#endif
}

std::filesystem::path ScriptBuilder::createFromTemplate(const Template& source, const std::string& className,
                                                        const fs::path& folder, std::string& error) {
    static const std::regex identifier("^[A-Za-z_][A-Za-z0-9_]*$");
    if (!std::regex_match(className, identifier)) {
        error = "A script name is a C++ class name: letters, digits and _, not starting with a digit.";
        return {};
    }

    const fs::path target = folder / (className + ".cpp");
    std::error_code ec;
    if (fs::exists(target, ec)) {
        error = "A script with this name already exists here.";
        return {};
    }

    // The template is written for its own name; the class takes the new one.
    std::string text = source.source;
    const std::string original = source.name;
    for (size_t at = text.find(original); at != std::string::npos; at = text.find(original, at + className.size())) {
        text.replace(at, original.size(), className);
    }

    fs::create_directories(folder, ec);
    std::ofstream out(target, std::ios::binary);
    out << text;
    if (!out) {
        error = "Cannot write " + target.filename().string();
        return {};
    }
    return target;
}
