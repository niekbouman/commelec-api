# commelec-api

## What is this?

This repository contains an API for the Commelec real-time microgrid framework. 

More concretely, we have defined a message format, based on [Cap'n Proto](https://capnproto.org), for encoding certain mathematical objects (i.e., real-valued symbolic expressions, sets and set-valued functions) as sequences of bytes, such that: 
* the size of the byte representation is small, 
* the encoding and decoding time is small, and
* applying operations on the expressions using our interpreter (evaluation, computing derivatives, testing set membership, etc.) is fast.

Although our primary use case is the Commelec control framework, the convenience functions to encode the objects and the interpreter might be of independent interest.

## Table of Contents
* [Build instructions](docs/building.md)
* [Tutorial: Writing a resource agent in Python](docs/pytutorial.md)
* [Using commelecd, Commelec's communication daemon](docs/daemon.md)
* [Explanation of the (capnproto) schema of our message format](docs/schema.md)
* [Low-level API usage examples](docs/llapi.md)
* [ARM (CompactRIO) cross-compile instructions](docs/cross.md)

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
