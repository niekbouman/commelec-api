## Example: Building a RealExpr and evaluating it

Below, we first define a real-valued function. Then, using our interpreter (the class `AdvFunc`), we can evaluate the function. (Note that this can be viewed as an implementation of the Gang-of-Four interpreter pattern.)

The main feature is that the function is defined in a Cap'n Proto message.
Hence, it can be efficiently serialised, and parsed in (possibly) a different programming language
on (possibly) a different machine. Or, vice versa, one can define the function in a different language and parse it using the C++ interpreter.

```cpp
#include <commelec-api/schema.capnp.h>
#include <commelec-api/realexpr-convenience.hpp>
#include <commelec-interpreter/adv-interpreter.hpp>
#include <capnp/message.h>

::capnp::MallocMessageBuilder message;
auto adv = message.initRoot<msg::Advertisement>();
auto expr = adv.initCostFunction();

using namespace cv;

double a = 3.0;
double b = 6.0;
double c = 2.4;

Var x("X");
Var y("Y");
// "X" and "Y" are the names of the variables as they will appear in 
// the Cap'n Proto message (We use 'x' and 'y' for these 
// 'Var' (variable) objects in our C++ code)

buildRealExpr(expr, Real(a) * x + sin(y));
// this uses expression templates, to construct the expression tree at
// compile time

AdvFunc interpreter(adv);
// create the interpreter

double result = interpreter.evaluate(expr,{{"X",b},{"Y",c}});
// evaluate expr using x=b and y=c

```

