#pragma once

#include "FileWatcher.hpp"

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>
#include <boost/regex.hpp>

#include "../vkbase/core/EngineBase.h"

namespace fs=std::filesystem;
using namespace std::ranges;

class Project
{

public:

    std::unordered_set<std::filesystem::path> modifiedFiles;
    std::unordered_map<std::filesystem::path,std::filesystem::path> renamedFiles;

    Project():projectRoot(){}
    ~Project();
    void updateDependencies(const std::string& src);
    void create_builtin_file(const fs::path& file) const;
    void setRoot(const std::filesystem::path& project_file);
    std::filesystem::path getRoot();


private:

    std::filesystem::path projectRoot;

    struct DepInfo
    {
        long watch_id;
        std::filesystem::path path;
    };
    std::unordered_map<fs::path,long> dependencies;

    void fileModified(const std::filesystem::path& file, FileAction action);
    static std::unordered_set<std::filesystem::path> getDependencies(const std::string& src, const std::filesystem::path& directory);
    static std::vector<std::string> readDependencies(const std::string& src);
};