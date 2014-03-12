#ifndef CP_ARGS_HH
#define CP_ARGS_HH

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>

class ArgumentHandler {
public:
  virtual void handle_argument(const char *name,
                               const char *optarg) = 0;

  static bool parse(const char* argname, const char* name, 
                       const char* optarg, int& arg) {
    if (strcmp(name, argname) == 0) {
      int temp;// = atoi(optarg);
      if(( std::istringstream( optarg ) >> temp ).eof()) {
//      if (temp != 0) {
        arg = temp;
        return true;
      } else {
        std::cerr << "ERROR:" << name << " arg: \"" << optarg << "\" is invalid\n";
        return false;
      }
    }
    return false;
  } 
  static bool parse(const char* argname, const char* name, 
                       const char* optarg, bool& arg) {
    int intarg;
    if(parse(argname, name, optarg, intarg)) {
      arg=intarg;
      return true;
    }
    return false;
  }


};


#endif
