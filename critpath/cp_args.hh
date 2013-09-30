

#ifndef CP_ARGS_HH
#define CP_ARGS_HH

class ArgumentHandler {
public:
  virtual void handle_argument(const char *name,
                               const char *optarg) = 0;
};


#endif
