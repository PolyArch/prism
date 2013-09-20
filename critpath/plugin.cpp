
#include "plugin.hh"

#include <sys/types.h>
#include <dirent.h>
#include <dlfcn.h>

#include <iostream>
#include <string>

void load_plugins(const char *dirname)
{

  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(dirname)) == NULL) {
    //std::cerr << "cannot open directory: " << dirname << "\n";
    return;
  }

  while ((ent = readdir(dir)) != NULL) {
    std::string fname(ent->d_name);
    size_t idx = fname.find_last_of('.');
    if (idx == std::string::npos)
      continue;
    std::string ext = fname.substr(idx+1);
    if (ext != "so")
      continue;
    // try to load this
    std::string fullpath = std::string(dirname) + "/" + fname;
    void *plugin = dlopen(fullpath.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if (plugin == NULL) {
      std::cout << "error loading " << dlerror() << "\n";
      continue;
    }
    //std::cout << "plugin " << ent->d_name << " loaded successfully.\n";
  }
  closedir(dir);
}
