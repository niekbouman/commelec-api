# commelec-api

## Table of Contents
[Build instructions](docs/building.md)

## Message format specification and API for the Commelec Smart-Grid-control platform.

*Commelec* is distributed real-time a control framework for modern electrical grids. By a "modern grid", we mean a grid involving volatile and weather-dependent sources, like wind turbines and photo-voltaic (PV) panels, loads such as heat pumps, and storage elements, like batteries, supercapacitors, or an electrolyser combined with a fuel cell.

Resources and (distributed) controllers exchange control information over a packet network. Resources inform their local controller about their capabilities, current state and desired operating point by means of sending a collection of mathematical objects; we call this an *advertisement*. More precisely, but informally, an advertisement consists of:
* the capability curve of the associated inverter or generator (essentially, a convex set in the P-Q plane, where P represents active power and Q represents reactive power). Let us denote this set by *A*.
* a set-valued function defined on *A*, which aims to capture the uncertainty that is present in the process of implementing a requested *power setpoint* (i.e., a point in *A*)
* a real-valued cost function defined on *A*.
We use the term *resource agent* for the software agent that is responsible for communicating with the (decentralized) controller. Hence, one of the tasks of a resource agent is to -- typically periodically -- construct an advertisement. Another common task of a resource agent is to parse a *request* message sent from the controller to a resource.

Commelec's *message format* specifies how the *requests* and *advertisements* are to be encoded into a sequence of bytes, suitable for transmission over a packet network. The message format is defined in terms of a [Cap'n Proto](https://capnproto.org) [schema](https://capnproto.org/language.html). The schema can be found [here](https://github.com/niekbouman/commelec-api/blob/master/src/schema.capnp).

## High-Level and Low-Level API

A main feature of the Cap'n Proto serialisation framework is that it can, given a schema definition, automatically generate an API for reading and writing the data structures defined in that schema. Currently, Cap'n Proto schema compilers exist for C++, Java, Javascript, Python, Rust and some other languages. Note that our code is written in modern C++ (C++11).

We will refer to the automatically generated API as the *low-level API*. 

We use the term *high-level API* (and the abbreviation *hlapi*) for a collection of functions (each with a C function prototype) for constructing advertisements for various commonly used resources. Each such function is tailored to a specific resource. Currently, we provide functions for constructing advertisements for a battery, a fuel cell, and a PV, as well a function to parse a request (the latter is independent of the resource). 

By means of a small server program, a *daemon*, we make the above functions accessible via a simple interface that is based on sending JSON objects over a UDP socket. This makes it easy to use our high-level API without having to link to our library (one only has to compile the daemon).

The implementation of the high-level API demonstrates the use of the low-level API. Hence, when you want to write a function for constructing an advertisement for a resource that is not supported in the high-level API, the implementation code of the high-level API could serve as an example and/or as a starting point for your custom function.

We have written (in C++11) some convenience functions for easily constructing certain mathematical objects that we define in the schema, like polynomials or (convex) polytopes. Note that we view those convenience functions as part of the low-level API.

## Usage Example of the High-Level API
Let us demonstrate how to parse a request.

```C++
// parsing a Request

#include <iostream>
#include <commelec-api/hlapi.h>

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
* a *Belief Function* (a set-valued function) which is also encoded as a `SetExpr` that has real-valued symbolic variables "P" and "Q" (i.e., both encoded via `RealExpr`essions of type *Variable*), 
* and a *Cost Function*, which is encoded as a `RealExpr` having real-valued symbolic variables "P" and "Q".
* the *Implemented Setpoint*, which is the power setpoint, encoded as the list "[P,Q]" (of length 2), that the follower (the sender of the Advertisement) has just implemented. 

### The `SetExpr` struct type
A `SetExpr` represents a closed convex set in arbitrary dimension. We will mostly consider one- and two-dimensional sets. Currently, one can encode the following types
* a singleton (encoded as a list of `RealExpr`essions)
* an *n*-ball (in 2-D, this simplifies to a disk)
* an *n*-orthotope or hyperrectangle (in 1-D, this simplifies to an interval, and in 2-D to a rectangle)
* a polytope, encoded in the "H-representation" (i.e., as an intersection of half-spaces)

Also, one can make intersections between the above types.
We currently do not support taking unions, because the result of a union of convex sets need not be convex.

### The `RealExpr` struct type
The `RealExpr` struct represents a real-valued expression, encoded as an abstract syntax tree. 
Just like a `SetExpr` encodes a particular set type (for example ball or rectangle), a `RealExpr` encodes a particular expression type. Currently, we support
* an immediate value of type double (IEEE 754 double-precision floating point)
* a symbolic variable name (for example "x"), via the *Variable* field
* an *operation*, acting on:
  * another `RealExpr`, i.e., a `UnaryOperation` (for example, sin, cos, log)
  * two `RealExpr`essions, i.e., a `BinaryOperation` (to express addition, multiplication, min, max, etc.)
  * a list of `RealExpr`, called a `ListOperation` (currently, only addition and multiplication)
* a uni- or multivariate polynomial. Note that we call the `Polynomial` struct a "short-cut" type, because a polynomial can in principle be encoded via immediate values, symbolic variables and operations (sums, powers), however, having a dedicated polynomial type has some implementational benefits and its use typically leads to smaller message sizes.

### Naming `RealExpr`essions and `SetExpr`essions and referring back to them
Additionally, a `RealExpr` can be given a name, which is useful if you define an expression that you want to reuse. For example, say that you define a `RealExpr` as part of the definion of a PQ profile, and you need the same `RealExpr` also in the cost function. In such case, you can give the `RealExpr` a name by setting the `name` field to some string, for example, "a", and then use a `RealExpr` of type *Reference* set to "a" somewhere in the definition of that cost function. Similarly, a `SetExpr` can be given a name, too.
Note that the name string appears directly in the advertisement, so assigning short names is good practice as it will result in shorter message sizes. 

### Example: Defining a Real-Valued Function
Let us demonstrate how we can express the function `f: R x R -> R`, `(P,Q) \mapsto P + sin(4*Q)` as a `RealExpr`. First, we will do so "by hand", using the functions provided by Cap'n Proto's API (see also [this page](https://capnproto.org/cxx.html)). Next, we will show how we can define the same message in a much simpler way using the `buildRealExpr` convenience function, using less lines of code. 
The reason for showing the more involved, "by-hand" approach first, is because it provides insight into the structure of a `RealExpr`-encoded syntax tree, and with that, in the basic ideas that underlie our message format. 
 
Let us break down the expression `P + sin(4*Q)` as a binary operation (`+`, i.e., addition) between the symbolic variable `P` and the unary operation (`sin`) applied to (the result of) a binary operation (`*`, i.e., multiplication) between the real (immediate) value 4 and the symbolic variable `Q`.

```C++
//we assume f is of type msg::RealExpr::Builder

#include <commelec-api/src/schema.capnp.h>

auto binOp = f.initBinaryOperation();
binOp.getOperation().setSum();
binOp.getArgA().setVariable("P");
auto unOp binOp.getArgB().initUnaryOperation();
unOp.getOperation().setSin();
auto sinArg = unOp.getArg().initBinaryOperation();
sinArg.getOperation().setProd();
sinArg.getArgA().setReal(4);
sinArg.getArgB().setVariable("Q");
```

To simplify the task of defining a `RealExpr`, we provide a convenience function for parsing human-writable mathematical (real-valued) expressions at compile-time (using C++ expression templates) into the syntax tree.
Let us now show how we can define exactly the same `RealExpr` as above using the `buildRealExpr` convenience function:

```C++
// defining f using the convenience function

#include <commelec-api/src/realexpr-convenience.hpp>
using namespace cv; // namespace for the convenience functions

Var P("P");
Var Q("Q");
buildRealExpr(f,P+sin(4*Q));

```

...more to come...

