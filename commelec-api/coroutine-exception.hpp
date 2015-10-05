#ifndef _COROUTINEEXCEPT_H_
#define _COROUTINEEXCEPT_H_

#include <stdexcept>
#include <boost/exception/all.hpp>
#include <boost/asio/spawn.hpp>

inline boost::exception_ptr convertExceptionPtr(std::exception_ptr ex) {
  try {
    throw boost::enable_current_exception(ex);
  } catch (...) {
    return boost::current_exception();
  }
}

inline void rethrow_boost_compatible_exception_pointer() {
  // Ensure that any exception gets correctly transferred by
  // Boost.Exception's functions to somewhere outside the coroutine
  // (This is needed because Boost.Coroutine relies on Boost.Exception for
  // exception propagation)

  // boost::current_exception does not always work well with exceptions
  // that have not been thrown using boost's boost::throw_exception(e) or
  // with throw(boost::enable_current_exception(e)), hence we use
  // std::current_exception (from the standard library), convert it to a
  // boost::exception_ptr type and rethrow it in a Boost.Exception-compatible
  // way

  std::exception_ptr sep;
  sep = std::current_exception();
  boost::exception_ptr bep = convertExceptionPtr(sep);
  boost::rethrow_exception(bep);
}

template <typename Handler, typename Function>
void spawn_coroutine(Handler h, Function f) {
  boost::asio::spawn(std::forward<Handler>(h),
                     [f](boost::asio::yield_context yield) {
    try {
      f(yield);
    } catch (const boost::coroutines::detail::forced_unwind &) {
      throw;
      // needed for proper stack unwinding when coroutines are destroyed
    } catch (...) {
      rethrow_boost_compatible_exception_pointer();
      // handle uncaught exceptions at the location where io_service.run() is
      // called (typically in "int main()")
    }
  });
}

#endif
