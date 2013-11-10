
#ifndef CP_UTILS_HH
#define CP_UTILS_HH

#include <string>

static std::string exec(const char* cmd)
{
  FILE* pipe = popen(cmd, "r");
  if (!pipe) return "ERROR";
  char buffer[128];
  std::string result = "";
  while(!feof(pipe)) {
    if(fgets(buffer, 128, pipe) != NULL)
      result += buffer;
  }
  pclose(pipe);
  return result;
}

static std::string grepF(std::string &fname, const char* sval,
                         int lineoff, int field) {
  std::stringstream rss;
  rss << "grep -irA" << lineoff-1 <<  " \"" << std::string(sval) << "\" "
      << fname << " | tail -1 | tr -s \" \" | cut -d\" \" -f" << field;

  std::string rs = rss.str();
  //std::cout << rs << "\n";
  return exec(rs.c_str());
}

static void execMcPAT(std::string& inf, std::string& outf) {
  const char *mcpat = getenv("MCPAT");
  if (!mcpat) {
    mcpat = "mcpat";
    std::string ms = std::string(mcpat) + std::string(" -opt_for_clk 0 -print_level 5 -infile ")
                   + inf + std::string(" > ") + outf;
    //std::cout << ms << "\n";
    system(ms.c_str());
  }
}


#endif
