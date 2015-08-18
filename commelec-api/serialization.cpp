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
#include "serialization.hpp"
#include <capnp/serialize-packed.h>

size_t messageByteSize(::capnp::MessageBuilder &builder) {
  size_t sz = 0;
  auto segments = builder.getSegmentsForOutput();
  for (auto segment : segments) {
    sz += segment.asBytes().size();
  }
  return sz;
}

VectorBuffer::VectorBuffer(std::vector<kj::byte> &vec)
    : _vector(vec), bytesWritten(0) {}

kj::ArrayPtr<kj::byte> VectorBuffer::getWriteBuffer() {
  return kj::ArrayPtr<kj::byte>(_vector.data() + bytesWritten, available());
}

size_t VectorBuffer::available() {
  return _vector.size() - bytesWritten;
}

void VectorBuffer::write(const void *src, size_t size) {
  if (src == _vector.data() + bytesWritten) {
    // caller wrote directly into our buffer.
    bytesWritten += size;
  } else {
    if (size > available()) {
      // resize vector (can be expensive, but can be avoided by initialising
      // the vector with a large enough size)
      _vector.resize(_vector.size() + size);
    }
    memcpy(_vector.data() + bytesWritten, src, size);
    bytesWritten += size;
  }
}

void writePackedMessage(std::vector<uint8_t> &output,
                        ::capnp::MessageBuilder &builder) {
  VectorBuffer buf(output);
  writePackedMessage(buf, builder.getSegmentsForOutput());
  output.resize(output.size() - buf.available());
}

size_t packToByteArray(capnp::MallocMessageBuilder &builder, uint8_t *buffer,
                       size_t bufsize) {

  std::vector<uint8_t> packedDataBuffer(messageByteSize(builder));
  // buffer to hold the packed advertisement
  // ( messageByteSize(msg) is a crude upper bound for the buffer size.
  // The exact size that we will actually need is unknown at this point)

  writePackedMessage(packedDataBuffer, builder);
  // will resize packedDataBuffer to the exact size of the packed data

  auto advBytesize = packedDataBuffer.size();

  if (advBytesize <= bufsize)
    memcpy(buffer, packedDataBuffer.data(), advBytesize);

  // do not copy data if there is too little space in the buffer

  return advBytesize;
}

