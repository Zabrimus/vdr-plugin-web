#pragma once

#include <memory>
#include <thrift/transport/TBufferTransports.h>

// TBufferedTransport reads at most its buffer size (default 512 bytes) per read() from the
// socket. An OSD frame or a TS chunk needed thousands of system calls. A larger buffer
// doesn't change the data on the wire.
#define THRIFT_TRANSPORT_BUFFER_SIZE (256 * 1024)

class TLargeBufferedTransportFactory : public apache::thrift::transport::TTransportFactory {
public:
    std::shared_ptr<apache::thrift::transport::TTransport> getTransport(std::shared_ptr<apache::thrift::transport::TTransport> trans) override {
        return std::make_shared<apache::thrift::transport::TBufferedTransport>(trans, THRIFT_TRANSPORT_BUFFER_SIZE, THRIFT_TRANSPORT_BUFFER_SIZE);
    }
};
