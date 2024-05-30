#include <filesystem>
#include "FileWatcher.hpp"

class FileWatcherListener : public efsw::FileWatchListener
{
public:
    //map file name to shader model
    std::map<std::string, FunCallback_T> files;

    void handleFileAction( efsw::WatchID watchid, const std::string& dir,
                           const std::string& filename, efsw::Action action,
                           std::string oldFilename ) override
    {
        std::filesystem::path p(dir);
        p.append(filename);
        for(auto& pair:files)
        {
            std::cout<<p.compare(pair.first)<<std::endl;
            if(p.compare(pair.first)==0)
            {
                switch(action)
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

                pair.second((FileAction)action);
                return;
            }
        }
    }
};

static FileWatcherListener* listener = nullptr;
static efsw::FileWatcher* fileWatcher = nullptr;

std::string getDirectory(const std::string& fileName)
{
    std::filesystem::path p(fileName);
    return p.parent_path().string();
}

bool addFileToWatch(const std::string& filename, FunCallback_T callback)
{
    //check if file exists
    std::filesystem::path p(filename);

//    if(!std::filesystem::exists(filename))
//    {
//        std::cerr<<"File "<<filename<<" does not exist"<<std::endl;
//        return false;
//    }

    if(!fileWatcher)//lazy initialization
    {
        fileWatcher = new efsw::FileWatcher();
        listener = new FileWatcherListener();
        fileWatcher->watch();
    }

    if(listener->files.contains(filename))
    {
        std::cerr<<"File "<<filename<<" is already being watched"<<std::endl;
        return false;
    }

    listener->files[p.generic_string()] = callback;
    auto id=fileWatcher->addWatch(getDirectory(filename), listener, false);

    std::cout<<"Watching "<<filename<<std::endl;
    return true;
}

void removeFileFromWatch(const std::string& filename)
{
    if(fileWatcher)
    {
        fileWatcher->removeWatch(getDirectory(filename));
        listener->files.erase(filename);
    }
}
