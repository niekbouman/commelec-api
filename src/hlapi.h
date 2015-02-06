// The MIT License (MIT)
// 
// Copyright (c) 2015 Niek J. Bouman
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// 
#ifndef HIGHLEVEL_API_H_
#define HIGHLEVEL_API_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
// no name mangling
#endif

// error codes
static const int32_t hlapi_unknown_error = -1;
static const int32_t hlapi_illegal_input = -2;
static const int32_t hlapi_buffer_too_small = -3;
static const int32_t hlapi_malformed_request = -4;
static const int32_t hlapi_malformed_advertisement = -5;

typedef uint32_t AgentIdType;

int32_t parseRequest(const uint8_t *inBuffer, int32_t bufSize, double *P,
                     double *Q, uint32_t *senderId);
// extract setpoint from a request (encoded according to the COMMELEC schema)
//
// in:
//   inBuffer = ptr to byte array, bytelength specified in bufSize
//
// in/out params:
//   P = real power setpoint
//   Q = reactive power setpoint
//   senderId = agent identifier of the sender (leader) of the request
//
// return value:
//   1 = setpoint (P,Q) requested for implementation
//   0 = request does not contain a setpoint, follower should reply with an
//   advertisement (P and Q should be discarded)
//  -1 = parsing failed

int32_t makeBatteryAdvertisement(uint8_t *outBuffer, int32_t maxBufSize,
                                 int32_t *packedBytesize, uint32_t agentId,
                                 double Pmin, double Pmax, double Srated,
                                 double coeffP, double coeffPsquared,
                                 double coeffPcubed);
// Make advertisement for a battery resource agent
//
// Packed advertisement (the serialized message) is written to buffer.
// Set maxBufSize to the maximum size of your buffer (this parameter is there to
// prevent buffer overflows)
//
// return value:
//     returns 0 if success
//     returns negative value if error occurred (see error codes above)
//
// packedBytesize
//   After calling the function, packedBytesize contains the byte size of the
//   packed advertisment. If this value exceeds maxBufSize, then the buffer was
//   too small, and no data is copied into the buffer.
//   If this happens, you should make allocate a buffer at least as large as 
//   packedBytesize and then call makeBatteryAdvertisement again
//
// agentId: number that uniquely identifies the resource
//
// PQ profile:
//   rated power: Srated
//   contraints on real power: Pmin, Pmax
//
// Belief: the identity belief function
//
// Cost: a polynomial cost function, i.e.,
//     cf(p,q) = coeffPcubed * p^3 + coeffPsquared * p^2 + coeffP * p
//

inline int32_t makeFuelCellAdvertisement(uint8_t *outBuffer, int32_t maxBufSize,
                                  int32_t *packedBytesize, uint32_t agentId,
                                  double Pmin, double Pmax, double Srated,
                                  double coeffP, double coeffPsquared,
                                  double coeffPcubed) {
  // fuel cell is like battery but cannot consume power
  if ((Pmin < 0) || (Pmax < 0))
    return hlapi_illegal_input;
  return makeBatteryAdvertisement(outBuffer, maxBufSize, packedBytesize,
                                  agentId, Pmin, Pmax, Srated, coeffP,
                                  coeffPsquared, coeffPcubed);
}

int32_t makePVAdvertisement(uint8_t *outBuffer, int32_t maxbufsize,
                            int32_t *packedBytesize, uint32_t deviceId,
                            double Srated, double Pmax, double Pdelta,
                            double tanPhi, double a_pv, double b_pv);

#ifdef __cplusplus
}
#endif

#endif

