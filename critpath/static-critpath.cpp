
#include <getopt.h>
#include <stdint.h>

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>

#include <sys/time.h>

#include "gzstream.hh"

#define STANDALONE_CRITPATH 1
#include "../../src/cpu/critpath_node.hh"



int FETCH_WIDTH = 4;
int COMMIT_WIDTH = 4;
int IQ_WIDTH = 64;
int ROB_SIZE = 192;

int BR_MISS_PENALTY = (1 //squash width (rename+decode+fetch+dispatch)
                       + 4 //ROB squashing (16 instruction to squash)
                       + 1 //restarting fetch
                       );


#define QUEUE_MAX (16*1024)
class CP_Node;
class insert_event_handler {
public:
  virtual bool before_insert(CP_Node &) = 0;
  virtual void inserted() = 0;
};

template<class T>
class MyQueueImpl
{
public:
  MyQueueImpl(): head(0), tail(0), last(0),  _size(0),
                 inserted_handler(0) {}
  virtual ~MyQueueImpl() {}

  bool empty() const { return head == tail; }
  T &front() { return _storage[head]; }

  T &back() { return _storage[last]; }

  size_t size() const { return _size; }

  void push(T &d) {
    if (_size == QUEUE_MAX)
      pop();
    if (inserted_handler) {
      inserted_handler->before_insert(d);
    }
    assert( _size < QUEUE_MAX );
    _storage[tail] = d;
    ++_size;
    last = tail;
    tail = (tail+1)%QUEUE_MAX;
    if (inserted_handler) {
      inserted_handler->inserted();
    }
  }
  void next() {
    assert( _size < QUEUE_MAX);
    ++_size;
    last = tail;
    tail = (tail+1)%QUEUE_MAX;
  }

  int next(int i) {
    int j = (i+1);
    if (j >= QUEUE_MAX) {
      return 0;
    }
    return j;
  }
  int prev(int i) {
    int j = (i-1);
    if (j < 0) {
      return QUEUE_MAX-1;
    }
    return j;
  }

  void pop() {
    assert(_size != 0);
    head = (head+1)%QUEUE_MAX;
    -- _size;
  }

  void clear() {
    head = tail = last = 0;
    _size = 0;
  }

  T &at(unsigned i) { return _storage[i]; }

  //  T & operator[](const unsigned i) { return _storage[i];}

  T &operator[](const int i) {
    if (i < 0) {
      int p = last;
      if (p + 1 + i < 0) {
        p = p + 1 + i + QUEUE_MAX;
      } else {
        p = p + 1 + i;
      }
      int p1 = last;
      for (int j = 1; j < -i; ++j)
        p1 = prev(p1);
      assert(p1 == p);
      return _storage[p];
    }
    return _storage[i];}

  int getLastIndex() const { return last;}
  int getHeadIndex() const { return head;}
  int getTailIndex() const { return tail;}

  void register_insert_handler(insert_event_handler *func) {
    inserted_handler = func;
  }
private:
  T _storage[QUEUE_MAX];
  int head, tail, last;
  unsigned _size;
  insert_event_handler* inserted_handler;
};
#undef QUEUE_MAX


class CP_Node {

public:
  uint64_t fetch_cycle;
  uint64_t dispatch_cycle;
  uint64_t ready_cycle;
  uint64_t execute_cycle;
  uint64_t complete_cycle;
  uint64_t committed_cycle;
  uint64_t index;
  bool ctrl_miss;

  uint64_t ff_cycle;
  uint64_t icache_cycle;
  uint64_t other_fetch_cycle;
  uint64_t fd_cycle;
  uint64_t de_cycle;
  uint64_t ec_cycle;
  uint64_t cc_cycle;

  CP_NodeDiskImage _img;
  CP_Node(): fetch_cycle(0),
             dispatch_cycle(0), execute_cycle(0),
             complete_cycle(0), committed_cycle(0), index(0),
             ctrl_miss(false),
             ff_cycle(0), icache_cycle(0), other_fetch_cycle(0),
             fd_cycle(0),
             de_cycle(0), ec_cycle(0), cc_cycle(0)
  {}
  CP_Node(const CP_NodeDiskImage &img, uint64_t index):
    fetch_cycle(0),
    dispatch_cycle(0),
    ready_cycle(0),
    execute_cycle(0), complete_cycle(0),
    committed_cycle(0), index(index),
    ctrl_miss(img._ctrl_miss),
    ff_cycle(img._fc),
    icache_cycle(img._icache_lat),
    other_fetch_cycle(img._fc - img._icache_lat),
    fd_cycle(img._dc),
    de_cycle(img._ec - img._dc),
    ec_cycle(img._cc - img._ec),
    cc_cycle(img._cmpc - img._cc), _img(img)
  {}
  void print_to_stream(std::ostream &out) {
    _img.write_to_stream(out);
    out << fetch_cycle << "::"
        << dispatch_cycle << " :: " << committed_cycle << "\n";
  }
};

class StaticPath : public insert_event_handler {
public:
  StaticPath() {
    _nodes.register_insert_handler(this);
  }
  virtual ~StaticPath() {
  }
  MyQueueImpl<CP_Node> _nodes;

  std::map<uint64_t, uint64_t> pc2NumExecuted;

  virtual bool before_insert(CP_Node &n) {
    pc2NumExecuted[n._img._pc] ++;
  }
  void print_top_nodes() {
    std::cout << "Num of static inst: " << pc2NumExecuted.size() << "\n";
  }
  void inserted() {}
  void insert(const CP_NodeDiskImage &img, uint64_t index)
  {
    CP_Node node(img, index);
    _nodes.push(node);
  }

};
StaticPath staticpath;

int main(int argc, char *argv[])
{

  static struct option long_options[] =
    {
      {"help", no_argument, 0, 'h'},
      {"fetch-width", required_argument, 0, 'f'},
      {"commit-width", required_argument, 0, 'c'},
      {"rob-size", required_argument, 0, 'r'},
      {"branch-miss-penalty", required_argument, 0, 'b'},
      {"iq-size", required_argument, 0, 'i'},
      {0,0,0,0}
    };

  while (1) {
    int option_index = 0;

    int c = getopt_long(argc, argv, "hf:c:r:b:i:",
                        long_options, &option_index);
    if (c == -1)
      break;

    switch(c) {
    case 'h':
      std::cout << argv[0] << " [-f F|-c C|-r R|-b B|-h] file\n";
      return(0);
    case 'f': FETCH_WIDTH = atoi(optarg); break;
    case 'c': COMMIT_WIDTH = atoi(optarg); break;
    case 'r': ROB_SIZE = atoi(optarg); break;
    case 'b': BR_MISS_PENALTY = atoi(optarg); break;
    case 'i': IQ_WIDTH = atoi(optarg); break;
    case '?': break;
    default:
      abort();
    }
  }

  if (FETCH_WIDTH <= 0) {
    FETCH_WIDTH = 4;
  }
  if (COMMIT_WIDTH <= 0) {
    COMMIT_WIDTH = 4;
  }
  if (ROB_SIZE <= 0) {
    ROB_SIZE = 192;
  }
  if (BR_MISS_PENALTY <= 0) {
    BR_MISS_PENALTY = 6;
  }
  if (IQ_WIDTH <= 0) {
    IQ_WIDTH = 64;
  }

  if (argc - optind != 1) {
    std::cerr << "Requires one argument.\n";
    return 1;
  }
  igzstream inf(argv[optind], std::ios::in | std::ios::binary);

  if (!inf.is_open()) {
    std::cerr << "Cannot open file " << argv[optind] << "\n";
    return 1;
  }
  uint64_t count = 0;
  uint64_t numCycles =  0;
  struct timeval start;
  struct timeval end;
  gettimeofday(&start, 0);
  while (!inf.eof()) {
    CP_NodeDiskImage img = CP_NodeDiskImage::read_from_file(inf);
    if (inf.eof()) {
      break;
    }
    if (count == 0) {
      img._fc = 0;
    }
    //img.write_to_stream(std::cout);
    ++count;
    staticpath.insert(img, count);
  }
  inf.close();
  gettimeofday(&end, 0);

  uint64_t start_time = start.tv_sec*1000000 + start.tv_usec;
  uint64_t end_time   = end.tv_sec * 1000000 + end.tv_usec;
  std::cout << "runtime : " << (double)(end_time - start_time)/1000000 << "  seconds\n";
  std::cout << "rate....: "
            <<  1000000*(double)count/((double)(end_time - start_time))
            << " recs/sec\n";
  staticpath.print_top_nodes();

  return 0;
}
