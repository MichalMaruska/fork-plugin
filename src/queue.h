#ifndef _QUEUE_H_
#define	_QUEUE_H_ 1


#include <ext/slist>
#include <string>
#include "debug.h"

using namespace std;
using namespace __gnu_cxx;


/* LIFO
   + slice operation,

   Memory/ownerhip:
   the implementing container(list) handles pointers to objects.
   The objects are owned by the application!
   if a Reference is push()-ed, we make a Clone! But we never delete the objects.
   pop() returns the pointer!
*/
template <typename T>
class my_queue
{
private:
  slist<T*> list;
  string m_name;     // for debug string const char*
  typename slist<T*>::iterator last_node;

public:
  const char* get_name()
  {
    return m_name.c_str();//  ?:"(unknown)"
  }

#if 0
  // move!!! take ownership
  void set_name(string&& name)
  {
    m_name = std::move(name);
  }
#endif

  [[nodiscard]] int length() const
  {
    return list.size();
  }

  [[nodiscard]] bool empty() const
  {
    return (list.empty());
  }

  const T* front () const
  {
    return list.front();
  }

  T* pop()                    // top_and_pop()
  {
    // not thread-safe!
    T* pointer = list.front();
    list.pop_front();
    // invalidate iterators
    if (list.empty())
      last_node = list.begin();
    return pointer;
  }


  void push(T* value)
  {
#if DEBUG > 1
    DB(("%s: %s: now %d + 1\n", __FUNCTION__, get_name(), length()));
#endif
    if (!empty ()) {
      last_node = list.insert_after(last_node, value);
    } else {
      list.push_front(value);
      last_node = list.begin();
    }
  }

  // fixme: move-?
  void push(const T& value)   // we clone the value!
  {
    T* clone = new T(value);
    push(clone);
  }

  /* move the content of appendix to the END of this queue
   *      this      appendix    this        appendix
   *     xxxxxxx   yyyyy   ->   xxxxyyyy       (empty)
   */
  void slice (my_queue<T>& suffix) // appendix
  {
#if DEBUG > 1
    DB(("%s: %s: appending/moving all from %s:\n", __FUNCTION__, get_name(),
        suffix.get_name()));
#endif

    if (! suffix.list.empty())
    {
      list.splice_after(last_node, suffix.list);
      last_node=suffix.last_node;
    }
#if DEBUG > 1
    DB(("%s now has %d\n", get_name(), length()));
    DB(("%s now has %d\n", suffix.get_name(), suffix.length()));
#endif
  }


  ~my_queue()
  {
#if 0
    if (m_name)
    {
      m_name = NULL;
    }
#endif
  }

  // const char* name = NULL
  my_queue<T>(string name) : m_name(name)
  {
    DB("constructor\n");
    last_node = list.end();
  };

  void swap (my_queue<T>& peer)
  {
    typename slist<T*>::iterator temp;
    temp = last_node;

    list.swap(peer.list);

    // iter_swap(last_node,peer.last_node);
    if (list.empty())
      last_node = list.begin();
    else {
      last_node = peer.last_node;
    }

    if (peer.list.empty())
      peer.last_node = peer.list.begin();
    else
      peer.last_node = temp;
  }
};

#endif
