
#include "plugin.hh"

#include <sys/types.h>
#include <dirent.h>
#include <dlfcn.h>

#include <iostream>
#include <string>

static std::string get_dir_name(const char *fname)
{
  std::string ret = std::string(fname);
  size_t idx = ret.find_last_of('/');
  if (idx == ret.length()) {
    // skip the last /.
    idx = ret.find_last_of('/', idx);
  }
  // /bin/critpath -> /bin
  // /bin/critpath/ -> /bin
  return ret.substr(0, idx);
}

void load_plugins(const char *argv0)
{
  const char *env_dirname = getenv("CP_PLUGIN_DIR");
  std::string dirname = ((env_dirname)? std::string(env_dirname)
                         : (get_dir_name(argv0) + "/" + "plugin"));

  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir(dirname.c_str())) == NULL) {
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
