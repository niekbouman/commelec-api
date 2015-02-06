# commelec-api

## Message format specification and API for the Commelec Smart-Grid-control platform.

*Commelec* is a control framework for *modern* electrical grids. By a modern grid, we mean a grid involving volatile and weather-dependent sources, like wind turbines and photo-voltaic (PV) panels, loads such as heat pumps, and storage elements, like batteries, supercapacitors, or an electrolyser+fuel cell.

Commelec is a distributed real-time control framework. Resources inform their local controller about their state and desired operating point by means of sending a collection of mathematical objects; we call this an *advertisement*. More precisely, but informally, an advertisement consists of a cost function, another set-valued function, and the common domain of those functions (which is a convex set). We use the term *resource agent* for the software agent that is responsible for communicating with the (decentralized) controller. Hence, one of the tasks is to -- typically periodically -- construct an advertisement. Another common task is to parse a *request* message sent from the controller to a resource.

Commelec's *message format* specifies how the *requests* and *advertisements* are to be encoded into a sequence of bytes, suitable for transmission over a packet network. The message format is defined in terms of a [Cap'n Proto](https://capnproto.org) [schema](https://capnproto.org/language.html). The schema can be found [here](https://github.com/niekbouman/commelec-api/blob/master/src/schema.capnp).

## High-Level and Low-Level API

A main feature of the Cap'n Proto serialisation framework is that it can, given a schema definition, automatically generate an API for reading and writing the data structures defined in that schema. Currently, Cap'n Proto schema compilers exist for C++, Java, Javascript, Python, Rust and some other languages. Note that our code is written in modern C++ (C++11).

We will refer to the automatically generated API as the *low-level API*. 

We use the term *high-level API* (and the abbreviation *hlapi*) for a collection of functions (each with a C function prototype) for constructing advertisements for various commonly used resources. Each such function is tailored to a specific resource. Currently, we provide functions for constructing advertisements for a battery, a fuel cell, and a PV, as well a function to parse a request (the latter is independent of the resource). 

Also, the high-level API demonstrates the use of the low-level API. Hence, when you want to write a function for constructing an advertisement for your resources which is not supported in the high-level API, the implementation code of the high-level API could serve as an example and/or as a starting point for your custom function.

We have written (in C++11) some convenience functions for easily constructing certain mathematical objects that we define in the schema, like polynomials or (convex) polytopes. Note that we view those convenience functions as part of the low-level API.

## Explanation of the Schema
The top-level structs `Message`, `Request`, and `Advertisement` should be self-explaining.

The `RealExpr` struct represents a real-valued expression, encoded as an abstract syntax tree. 
For example, to express the function `f: R x R -> R`, `(P,Q) \mapsto P + sin(4*Q)` by hand, we break down the expression as a binary operation (`+`, i.e., addition) between the symbolic variable `P` and the unary operation (`sin`) applied to (the result of) a binary operation (`*`, i.e., multiplication) between the real (immediate) value 4 and the symbolic variable `Q`.

```C++
//we assume f is of type msg::RealExpr::Builder

#include <commelec-api/src/schema.capnp.h>

auto binOp = f.initBinaryOperation();
binOp.getOperation().setSum();
binOp.getArgA().setReference("P");
auto unOp binOp.getArgB().initUnaryOperation();
unOp.getOperation().setSin();
auto sinArg = unOp.getArg().initBinaryOperation();
sinArg.getOperation().setProd();
sinArg.getArgA().setReal(4);
sinArg.getArgB().setReference("Q");
```

The above code shows that we can express `f` by using Cap'n Proto's C++ API (see also [this page](https://capnproto.org/cxx.html)), but it also shows that we have to write quite some lines of code to implement a trivial function.

Hence, we provide a convenience function for parsing human-writable mathematical (real-valued) expressions at compile-time (using C++ expression templates) into the syntax tree:


```C++
// defining f using the convenience function

#include <commelec-api/src/realexpr-convenience.hpp>
using namespace cv; // namespace for the convenience functions

Ref P("P");
Ref Q("Q");
buildRealExpr(f,P+sin(4*Q));

```

...more to come...

