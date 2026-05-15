#include "Project.hpp"
#include <fstream>
#include <iostream>
#include <ranges>
#include <utility>

Project::~Project()
{
    for (auto& wid: dependencies| views::values)
    {
        removeFileFromWatch(wid);
    }
}

void Project::updateDependencies(const std::string& src)
{
    auto newDependencies= getDependencies(src,projectRoot);

    for (auto it = dependencies.begin(); it != dependencies.end(); )
    {
        if (!newDependencies.contains(it->first))
        {
            removeFileFromWatch(it->second);
            it = dependencies.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (auto& p: newDependencies)
    {
        if (!dependencies.contains(p))
        {
            create_builtin_file(p);
            if (fs::exists(p))
                dependencies[p]=addFileToWatch(p.string(),[p,this](auto action){fileModified(p,action);});
        }
    }
}

void Project::create_builtin_file(const fs::path& file) const
{
    if (!fs::exists(file))
    {
        auto relative_path=file.lexically_relative(projectRoot);
        auto asset_it=vkbase::assets.find("include/"+relative_path.generic_string());
        if(asset_it!=vkbase::assets.end())
        {
            std::filesystem::create_directories(file.parent_path());
            std::ofstream f(file);
            if (!f)
                std::cout<<"Failed to create "<< file.generic_string()<<"\n";
            else
                f.write(asset_it->second.data(),asset_it->second.size());
            if (f)
            {
                std::cout<<"Created std file: "<< file.generic_string()<<"\n";
            }
        }
    }
}

void Project::setRoot(const std::filesystem::path& project_file)
{
    projectRoot=project_file.parent_path();
}

std::filesystem::path Project::getRoot()
{
    return  projectRoot;
}

void Project::fileModified(const std::filesystem::path& file, FileAction action)
{
    if (action==FileAction::Modified||FileAction::Delete)
        modifiedFiles.insert(file);
    else if (action==FileAction::Moved)
        renamedFiles[file]="";
}

std::unordered_set<std::filesystem::path> Project::getDependencies(const std::string& src,
    const std::filesystem::path& directory)
{
    std::unordered_set<std::filesystem::path> read;
    using namespace std::ranges::views;
    auto dep=readDependencies(src);
    auto dep_paths_t= dep | views::transform( [&](auto& file_name){return std::filesystem::path(directory/file_name); });
    std::vector<std::filesystem::path> dep_paths(dep_paths_t.begin(),dep_paths_t.end());
    std::vector<std::filesystem::path> next_dep;


    while (!dep_paths.empty())
    {
        for (const auto& p:dep_paths)
        {
            if (!read.contains(p))
            {
                std::ifstream f(p, std::ios::binary|std::ios::in);
                if (f)
                {
                    auto dep_src=std::string(std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>());
                    auto new_deps=readDependencies(dep_src);auto new_paths_t=new_deps | views::transform( [&](auto& file_name){return std::filesystem::path(p.parent_path()/file_name); });
                    next_dep.append_range(new_paths_t);
                }

                read.insert(p);
            }
        }
        dep_paths=std::move(next_dep);
    }

    return read;
}

std::vector<std::string> Project::readDependencies(const std::string& src)
{
    std::vector<std::string> included_files;
    boost::regex include_regex(R"(^\s*#include\s*["<]([^">]+)[">])");
    boost::sregex_iterator it(src.begin(), src.end(), include_regex);
    boost::sregex_iterator end;

    for (; it != end; ++it) {
        included_files.push_back(it->str(1));
    }

    return included_files;
}
