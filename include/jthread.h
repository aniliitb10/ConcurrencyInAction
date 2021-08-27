#pragma once

#include "date_util.h"

#include <thread>
#include <fmt/format.h>
#include <type_traits>

/*
 * A _thread wrapper class which will join in its destructor
 * */
class j_thread
{
public:
  explicit j_thread(std::thread&& thread):
  _thread(std::move(thread)){}

  // allow constructing from function and its arguments but ensure that these are syntactically correct
  // without this validation, compilation error message is really weird! and IDE has no clue!
  template<class Callable, class... Args, typename = typename std::enable_if_t<std::is_invocable_v<Callable, Args...>>>
  explicit j_thread(Callable&& callable, Args&&... args):
  _thread(std::forward<Callable&&>(callable), std::forward<Args&&>(args)...)
  {}

  // disable copy operations
  j_thread(const j_thread&) = delete;
  j_thread& operator=(const j_thread&) = delete;

  // explicitly enable move operations
  j_thread(j_thread&&) = default;
  j_thread& operator=(j_thread&&) = default;

  // custom destructor
  ~j_thread()
  {
    if (_thread.joinable())
    {
      // bad! never do this! but doing here just to get better sense of code flow
      // this just helped me understand that the this destructor got called from main _thread!
      // - which makes sense now!
      fmt::print("_thread id: {} Attempting to join _thread at: {}\n",
              std::this_thread::get_id(), dt::datetime::datetime_IST());
      _thread.join();
    }
  }

  // providing a handle of the _thread
  std::thread& get_thread()
  {
    return _thread;
  }

  [[nodiscard]] const std::thread& get_thread() const
  {
    return _thread;
  }

private:
  std::thread _thread;
};