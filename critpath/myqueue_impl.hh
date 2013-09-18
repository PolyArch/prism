
#ifndef CRITPATH_MY_QUEUE_IMPL_HH
#define CRITPATH_MY_QUEUE_IMPL_HH

#include <stdint.h>

#include <cassert>

#define QUEUE_MAX (4*1024)

template<class T>
class insert_event_handler {
public:
  virtual ~insert_event_handler() {}
  virtual bool before_insert(T &) = 0;
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

  unsigned size() const { return _size; }

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
    _storage[head]->remove(); 
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

  void register_insert_handler(insert_event_handler<T> *func) {
    inserted_handler = func;
  }
private:
  T _storage[QUEUE_MAX];
  int head, tail, last;
  unsigned _size;
  insert_event_handler<T>* inserted_handler;
};
#undef QUEUE_MAX
#endif
