#include <efsw/efsw.hpp>
#include <iostream>
#include <map>
#include <functional>


enum FileAction {
    /// Sent when a file is created or renamed
    Add = 1,
    /// Sent when a file is deleted or renamed
    Delete = 2,
    /// Sent when a file is modified
    Modified = 3,
    /// Sent when a file is moved
    Moved = 4
};

using FunCallback_T=std::function<void(FileAction)>;

bool addFileToWatch(const std::string& filename, FunCallback_T callback);
void removeFileFromWatch(const std::string& filename);


