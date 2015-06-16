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
#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include <kj/io.h>
#include <capnp/message.h>
#include <vector>

size_t messageByteSize(::capnp::MessageBuilder &builder);

class VectorBuffer : public kj::BufferedOutputStream {
  // BufferedOutputStream that writes into a vector
  //(VectorBuffer does not own the vector, it merely holds a reference to it)
public:
  VectorBuffer(std::vector<kj::byte> &vec);
  kj::ArrayPtr<kj::byte> getWriteBuffer() override;
  size_t available();
  void write(const void *src, size_t size) override;

private:
  std::vector<kj::byte> &_vector;
  kj::byte *bufferPos;
};

void writePackedMessage(std::vector<uint8_t> &output,
                        ::capnp::MessageBuilder &builder);

size_t packToByteArray(capnp::MallocMessageBuilder &builder, uint8_t *buffer,
                       size_t bufsize);

#endif
