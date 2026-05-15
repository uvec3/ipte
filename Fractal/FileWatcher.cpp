#include "FileWatcher.hpp"

#include <filesystem>
#include <boost/multi_index_container.hpp>
#include <ranges>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/lambda/lambda.hpp>

using namespace std::ranges;
using namespace boost::multi_index;
using namespace boost::lambda;
namespace fs = std::filesystem;

struct id_tag{};
struct path_tag{};
struct dir_tag{};

class FileWatcherListener : public efsw::FileWatchListener
{
public:
    struct FileData
    {
        long id;
        efsw::WatchID watch_dir_id;
        fs::path path;
        FunCallback_T callback;
    };


    multi_index_container<FileData,
                          indexed_by<
                              ordered_unique<tag<id_tag>, member<FileData, long, &FileData::id>>,
                              ordered_non_unique<tag<dir_tag>, member<FileData, efsw::WatchID, &FileData::watch_dir_id>>
                              ,
                              ordered_non_unique<tag<path_tag>, member<FileData, fs::path, &FileData::path>>
                          >
    > files;

    void handleFileAction(efsw::WatchID watchid, const std::string& dir,
                          const std::string& current_filename, efsw::Action action,
                          std::string oldFilename) override
    {
        std::string_view filename = oldFilename.empty() ? current_filename : oldFilename;


        fs::path p(dir);
        p.append(filename);
        auto range = files.get<path_tag>().equal_range(p);

        for (auto& file : subrange(range.first, range.second))
        {
            if (p.compare(file.path) == 0)
            {
                switch (action)
                {
                case efsw::Actions::Add:
                    std::cout << "DIR (" << dir << ") FILE (" << filename << ") has event Added"
                        << std::endl;
                    break;
                case efsw::Actions::Delete:
                    std::cout << "DIR (" << dir << ") FILE (" << filename << ") has event Delete"
                        << std::endl;
                    break;
                case efsw::Actions::Modified:
                    std::cout << "DIR (" << dir << ") FILE (" << filename << ") has event Modified"
                        << std::endl;
                    break;
                case efsw::Actions::Moved:
                    std::cout << "DIR (" << dir << ") FILE (" << filename << ") has event Moved from ("
                        << oldFilename << ")" << std::endl;
                    break;
                default:
                    std::cout << "Should never happen!" << std::endl;
                }

                file.callback((FileAction)action);
            }
        }
    }
};

static FileWatcherListener* listener = nullptr;
static efsw::FileWatcher* fileWatcher = nullptr;
long currentId = 0;

std::string getDirectory(const std::string& fileName)
{
    fs::path p(fileName);
    return p.parent_path().string();
}

long addFileToWatch(const std::string& filename, FunCallback_T callback)
{
    //check if file exists
    fs::path p(filename);

    if (!fs::exists(filename))
    {
        throw std::runtime_error("File " + filename + " does not exist");
    }

    if (!fileWatcher) //lazy initialization
    {
        fileWatcher = new efsw::FileWatcher();
        listener = new FileWatcherListener();
        fileWatcher->watch();
    }


    auto parent_path_str = p.parent_path().generic_string();
    fs::path parent_path(parent_path_str+"/");
    auto parent_path_end = parent_path_str + (static_cast<char>('/' + 1));
    auto range = listener->files.get<path_tag>().range(parent_path < _1, _1 < parent_path_end);

    efsw::WatchID id;
    //if directory is not already watched, watch it
    if (range.first == range.second)
    {
        id = fileWatcher->addWatch(p.parent_path().string(), listener, false);
        if (id < 0)
        {
            throw std::runtime_error("Failed to watch file " + p.generic_string());
        }
    }
    //get the watch id of the already watched directory
    else
    {
        id = range.first->watch_dir_id;
    }


    listener->files.insert({++currentId, id, p, std::move(callback)});

    std::cout << "Watching " << p.generic_string() << std::endl;
    return currentId;
}

void removeFileFromWatch(long watch_id)
{
    if (fileWatcher)
    {
        auto& index = listener->files.get<id_tag>();
        auto it = index.find(watch_id);
        if (it != index.end())
        {
            auto dir_id = it->watch_dir_id;
            std::cout << "Stopped watching " << it->path.generic_string() << std::endl;
            index.erase(it);

            //check if there are any other files being watched in the same directory
            auto& dir_index = listener->files.get<dir_tag>();
            auto range = dir_index.equal_range(dir_id);
            if (range.first == range.second)
            {
                std::cout<< "No more files in directory with watch id " << dir_id << ", stopping watch" << std::endl;
                fileWatcher->removeWatch(dir_id);
            }
        }
        else
        {
            std::cout << "No watch with id " << watch_id << " found" << std::endl;
        }
    }
}
