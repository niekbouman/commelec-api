# The MIT License (MIT)
# 
# Copyright (c) 2015 Andrey Bernstein and Niek J. Bouman
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# 
@0xdb1b4800e80dbd2d;
using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("msg");
#
# COMMELEC message format v1.0
#
# Specified in Cap'n Proto's schema language (Requires Cap'n Proto v0.5
#   [ http://kentonv.github.io/capnproto/language.html ] 
#
# Copyright (c) 2015 by Andrey Bernstein & Niek J. Bouman
#
# EPFL - Swiss Federal Institute of Technology Lausanne

#=====================================================
# Top-Level "Message" Type, Request and Advertisement 
#=====================================================

struct Message {
    agentId                   @0 :UInt32;
    union {
      request                 @1 :Request;
      advertisement           @2 :Advertisement;
    }
}

struct Request {
    setpoint                  @0 :List(Float64);
}

struct Advertisement {
    pQProfile                 @0 :SetExpr;
    beliefFunction            @1 :SetExpr;
    costFunction              @2 :RealExpr;
    implementedSetpoint       @3 :List(Float64);
}

#==============================================
# Definition of RealExpr and its child structs
#==============================================

struct RealExpr {
    name                      @0 :Text;
    union {
      real                    @1 :Float64;
      polynomial              @2 :Polynomial;
      unaryOperation          @3 :UnaryOperation;
      binaryOperation         @4 :BinaryOperation;
      listOperation           @5 :ListOperation;
      caseDistinction         @6 :CaseDistinction(RealExpr);
      reference               @7 :Text;
     }
}

struct UnaryOperation {
    arg                       @0 :RealExpr;
    operation :union {
      negate                  @1  :Void;
      abs                     @2  :Void;
      sign                    @3  :Void;
      multInv                 @4  :Void;
      square                  @5  :Void;
      sqrt                    @6  :Void;
      sin                     @7  :Void;
      cos                     @8  :Void;
      tan                     @9  :Void;
      exp                     @10 :Void;
      ln                      @11 :Void;
      log10                   @12 :Void;
      round                   @13 :Void;
      floor                   @14 :Void;
      ceil                    @15 :Void;
    }
}

struct BinaryOperation {
    argA                      @0 :RealExpr;
    argB                      @1 :RealExpr;
    operation :union {
      sum                     @2 :Void;
      prod                    @3 :Void;
      pow                     @4 :Void;
      min                     @5 :Void;
      max                     @6 :Void;
      lessEqThan              @7 :Void; # expression yields a boolean
      greaterThan             @8 :Void; # expression yields a boolean
    }
}

struct ListOperation {
    args                      @0 :List(RealExpr);
    operation :union {
      sum                     @1 :Void;
      prod                    @2 :Void;
    }
}

struct Polynomial {
    variables                 @0 :List(Text);
    maxVarDegree              @1 :UInt8; 
    coefficients              @2 :List(SparseCoeff);
}

struct SparseCoeff {
    offset                    @0 :UInt32;
    value                     @1 :Float64;
}

struct CaseDistinction(CaseType) {
    variables                 @0 :List(Text);
    cases                     @1 :List(ExprCase(CaseType)); 
}

struct ExprCase(CaseType) {
    # Representation of a single case for use in CaseDistinction
    # (order of evaluation follows List ordering)
    set                       @0 :SetExpr;
    expression                @1 :CaseType;
}

#=============================================
# Definition of SetExpr and its child structs
#=============================================

struct SetExpr {
    name                      @0 :Text;
    union {
      singleton               @1  :List(RealExpr); # dimension of the point equals the length of the list 
      ball                    @2  :Ball;
      rectangle               @3  :List(BoundaryPair);
      convexPolytope          @4  :ConvexPolytope;
      intersection            @5  :List(SetExpr);
      caseDistinction         @6  :CaseDistinction(SetExpr);
      reference               @7  :Text;
    }
}

struct Ball {
    center                    @0 :List(RealExpr);
    radius                    @1 :RealExpr;
}

struct BoundaryPair {
    boundA                    @0 :RealExpr; # the "interpreter" is responsible for sorting 
    boundB                    @1 :RealExpr; # the bounds, i.e., which bound is min and max 
}

struct ConvexPolytope {
    # A convex polytope formed by linear constraints
    # In case of a polygon, i.e., a two-dimensional polytope, the
    # length of the inner list of field 'a' will be equal to two. 
    a                         @0 :List(List(RealExpr));
    b                         @1 :List(RealExpr);
}
