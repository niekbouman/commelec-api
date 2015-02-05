#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include <kj/io.h>
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
