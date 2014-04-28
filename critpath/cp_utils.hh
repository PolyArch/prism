
#ifndef CP_UTILS_HH
#define CP_UTILS_HH

#include <string>
#include <sstream>
#include <unistd.h>
#include <fstream>
#include <boost/tokenizer.hpp>

static void msleep( int s ) {
  usleep( s * 1000 );
}


/*
static std::string exec(const char* cmd)
{
  msleep(50);
  FILE* pipe = popen(cmd, "r");
  if (!pipe) return "ERROR";
  char buffer[128];
  std::string result = "";
  while(!feof(pipe)) {
    if(fgets(buffer, 128, pipe) != NULL)
      result += buffer;
  }
  pclose(pipe);
  msleep(50);

  return result;
}
*/

//Note: called grepF for historical purposes only
static std::string grepF(std::string fname, const char* sval,
                         int lineoff, int field) {
  using namespace boost;

  //Sometimes, AFS is a total peice of crap.  So, when you open a file, it will just
  //not work.  Lets try this at least 100 times before we give up.

  for(int i = 0; i < 100; ++i) {
    std::ifstream infile(fname);
    std::string line;
    while (std::getline(infile, line)) {
      if(line.find(sval) != std::string::npos) {
        for(int i = 0; i < lineoff-1; ++i) { //increment line offset times
          std::getline(infile, line);
        }
  
        std::istringstream iss(line);
        char_separator<char> sep(" ");
        tokenizer<char_separator<char>> tokens(line, sep);
    
        int i = 0;
        for (const auto& t : tokens) {
          ++i;
          if(i==field) {
            return t;
          }
        }
        return std::string("");
      }
    }
    msleep(200);
  }
  std::cerr << fname << " had issues finding string \"" 
            << sval << "\"\n";
  assert(0 && "ERROR OPENING FILE");
  return std::string("ERROR STRING NOT FOUND!");
}
/*
//This code was the source of so much pain.  Lets try to wash those
//memories away.  :) 
static std::string grepF(std::string &fname, const char* sval,
                         int lineoff, int field) {
  std::stringstream rss;
  rss << "grep -irA" << lineoff-1 <<  " \"" << std::string(sval) << "\" "
      << fname << " | tail -1 | tr -s \" \" | cut -d\" \" -f" << field;

  std::string rs = rss.str();
  //std::cout << rs << "\n";
  return exec(rs.c_str());
}
*/

static bool is_empty(std::string fname) {
  std::ifstream infile(fname);
  return infile.peek() == std::ifstream::traits_type::eof();
}

static void execMcPAT(std::string& inf, std::string& outf) {
  const char *mcpat = getenv("MCPAT");
  if (!mcpat) {
    mcpat = "mcpat";
    std::string ms = std::string(mcpat) + std::string(" -opt_for_clk 0 -print_level 5 -infile ")
                   + inf + std::string(" > ") + outf + " 2> " + outf + ".err";
    //std::cout << ms << "\n";
    for(int i = 0; i < 10; i++) {
      int ret = system(ms.c_str());
      if(ret==0 && !is_empty(outf)) {
        return;
      }
      if(i%5==0) {
        std::cerr<<"retrying to make file: " << outf << "\n";
      }
      msleep(500);
    }

  }
}



inline static unsigned int mylog2 (uint64_t val) {
  unsigned int ret = -1;
  while (val != 0) {
      val >>= 1;
      ret++;
  }
  return ret;
} 


#endif
