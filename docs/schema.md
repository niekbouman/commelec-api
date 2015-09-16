## Explanation of the Schema
To understand the text below, you should first have a look at the definition of the [schema](https://github.com/niekbouman/commelec-api/blob/master/commelec-api/schema.capnp).

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

