# commelec-api

## Message format specification and API for the Commelec Smart-Grid-control platform.

*Commelec* is a control framework for modern electrical grids. By a "modern grid", we mean a grid involving volatile and weather-dependent sources, like wind turbines and photo-voltaic (PV) panels, loads such as heat pumps, and storage elements, like batteries, supercapacitors, or an electrolyser combined with a fuel cell.

Commelec is a distributed real-time control framework. Resources inform their local controller about their state and desired operating point by means of sending a collection of mathematical objects; we call this an *advertisement*. More precisely, but informally, an advertisement consists of a cost function, another set-valued function, and the common domain of those functions (which is a convex set). We use the term *resource agent* for the software agent that is responsible for communicating with the (decentralized) controller. Hence, one of the tasks is to -- typically periodically -- construct an advertisement. Another common task is to parse a *request* message sent from the controller to a resource.

Commelec's *message format* specifies how the *requests* and *advertisements* are to be encoded into a sequence of bytes, suitable for transmission over a packet network. The message format is defined in terms of a [Cap'n Proto](https://capnproto.org) [schema](https://capnproto.org/language.html). The schema can be found [here](https://github.com/niekbouman/commelec-api/blob/master/src/schema.capnp).

## High-Level and Low-Level API

A main feature of the Cap'n Proto serialisation framework is that it can, given a schema definition, automatically generate an API for reading and writing the data structures defined in that schema. Currently, Cap'n Proto schema compilers exist for C++, Java, Javascript, Python, Rust and some other languages. Note that our code is written in modern C++ (C++11).

We will refer to the automatically generated API as the *low-level API*. 

We use the term *high-level API* (and the abbreviation *hlapi*) for a collection of functions (each with a C function prototype) for constructing advertisements for various commonly used resources. Each such function is tailored to a specific resource. Currently, we provide functions for constructing advertisements for a battery, a fuel cell, and a PV, as well a function to parse a request (the latter is independent of the resource). 

Also, the high-level API demonstrates the use of the low-level API. Hence, when you want to write a function for constructing an advertisement for your resources which is not supported in the high-level API, the implementation code of the high-level API could serve as an example and/or as a starting point for your custom function.

We have written (in C++11) some convenience functions for easily constructing certain mathematical objects that we define in the schema, like polynomials or (convex) polytopes. Note that we view those convenience functions as part of the low-level API.

## Library and Compiler Dependencies
The C++ API code depends on the following libraries:
* [Cap'n Proto](https://capnproto.org), >= 0.5.0
* [Eigen3](http://eigen.tuxfamily.org/), >= 3.0.0 (a header-only library)

The C++ API uses [C++11](http://en.wikipedia.org/wiki/C++11), hence needs a newer compiler (e.g., one of those listed below):
* GNU C/C++ compiler >= 4.8 (Linux)
* Clang >= 3.3 (MacOS, FreeBSD)
* Visual C++ >= 2015 (Windows)
* MinGW-w64 >= 3 (for cross-compiling to Windows)

## Pre-Compiled Builds of the High-Level API
We currently provide [precompiled builds](http://smartgrid.epfl.ch/?q=node/14) for Windows (x86, 32 bit) and ARM.

## Usage Example of the High-Level API

```C++
// parsing a Request

#include <iostream>
#include <commelec-api/src/hlapi.h>

constexpr int32_t sz = 1024;
uint8_t buffer[sz];

// ... buffer is filled with the serialised Request data (e.g., by reading data from a socket)

double P = 0.0;
double Q = 0.0;
uint32_t senderId;

auto status = parseRequest(buffer, sz, &P, &Q, &senderId);

if (status==1)
{
  std::cout << "Received request with power setpoint: P = " << P << ", Q = " << Q << std::endl;
  std::cout << "Agent ID of the sender (our leader) " << senderId << std::endl;
}
else if(status==0)
{
  std::cout << "Received a request for an advertisement. Request does not include a setpoint." << std::endl;
  std::cout << "Agent ID of the sender (our leader) " << senderId << std::endl;
}
else if(status<0)
  std::cout << "Error while parsing request" << std::endl; 
else
  std::cout << "Unknown return code" << std::endl; 
```

## Explanation of the Schema
To understand the text below, you should first have a look at the definition of the [schema](https://github.com/niekbouman/commelec-api/blob/master/src/schema.capnp).

The top-level struct is `Message`, which either contains a `Request`, or an `Advertisement`. 
The `Advertisement` message is the most interesting of the latter two. It contains the following fields:
* a *PQ Profile*, which is encoded as a `SetExpr`,
* a *Belief Function* (a set-valued function) which is also encoded as a `SetExpr` that has real-valued parameters "P" and "Q" (i.e., both encoded via `RealExpr`essions of type *Reference*), 
* and a *Cost Function*, which is encoded as a `RealExpr` having real-valued parameters "P" and "Q".
* the *Implemented Setpoint*, which is the power setpoint, encoded as the list "[P,Q]" (of length 2), that the follower (the sender of the Advertisement) has just implemented. 

### A `RealExpr` has a *type*
The `RealExpr` struct represents a real-valued expression, encoded as an abstract syntax tree. 
Each `RealExpr` is of some type, for example `Real`, `Reference` or `UnaryOperation`.

### Naming `RealExpr`essions and `SetExpr`essions and referring back to them
Additionally, a `RealExpr` can be given a name, which is useful if you define an expression that you want to reuse. For example, say that you define a `RealExpr` as part of the definion of a PQ profile, and you need the same `RealExpr` also in the cost function. In such case, you can give the `RealExpr` a name by setting the `name` field to some string, for example, "a", and then use a `RealExpr` of type *Reference* set to "a" somewhere in the definition of that cost function. Similarly, a `SetExpr` can be given a name, too.
Note that the name string appears directly in the advertisement, so assigning short names is good practice as it will result in shorter message sizes. 

### Example: Defining a `RealExpr`
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

