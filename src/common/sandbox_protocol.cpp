#include "common/sandbox_protocol.hpp"

#include <cerrno>
#include <unistd.h>







bool write_all(int fd, const void* buffer, std::size_t size)
{
    const char* data = static_cast<const char*>(buffer);
    std::size_t total_written = 0;

    while (total_written < size) {
        ssize_t bytes_written =
            write(fd, data + total_written, size - total_written);

        if (bytes_written == -1) {
            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        if (bytes_written == 0) {
            return false;
        }

        total_written += static_cast<std::size_t>(bytes_written);
    }

    return true;
}

bool read_all(int fd, void* buffer, std::size_t size)
{
    char* data = static_cast<char*>(buffer);
    std::size_t total_read = 0;

    while (total_read < size) {
        ssize_t bytes_read =
            read(fd, data + total_read, size - total_read);

        if (bytes_read == -1) {
            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        if (bytes_read == 0) {
            return false;
        }

        total_read += static_cast<std::size_t>(bytes_read);
    }

    return true;
}


bool send_message(
    int fd,
    SandboxMessageType type,
    const void* payload,
    std::uint32_t payload_size)
{
    const SandboxMessageHeader header{
        .type = type,
        .payload_size = payload_size
    };

    if (!write_all(fd, &header, sizeof(header))) {
        return false;
    }

    if (payload_size == 0) {
        return true;
    }

    return write_all(fd, payload, payload_size);
}

bool receive_message(
    int fd,
    SandboxMessageHeader& header,
    void* payload,
    std::uint32_t payload_capacity)
{
    if (!read_all(fd, &header, sizeof(header))) {
        return false;
    }

    if (header.payload_size > payload_capacity) {
        return false;
    }

    if (header.payload_size == 0) {
        return true;
    }

    return read_all(fd, payload, header.payload_size);
}
StatusPipeReadResult read_setup_failure(
    int fd,
    SandboxSetupFailedPayload& payload)
{
    char* data = reinterpret_cast<char*>(&payload);

    std::size_t total_read = 0;

    while (total_read < sizeof(payload)) {
        ssize_t bytes_read = read(
            fd,
            data + total_read,
            sizeof(payload) - total_read
        );
        if (bytes_read == -1) {
            if (errno == EINTR) {
                continue;
            }

            return StatusPipeReadResult::Error;
        }

        if (bytes_read == 0) {
            if (total_read == 0) {
                return StatusPipeReadResult::Eof;
            }

            return StatusPipeReadResult::Error;
        }

        total_read += static_cast<std::size_t>(bytes_read);

                
            }

    return StatusPipeReadResult::Payload;
}