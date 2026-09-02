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

// Writes a wait_status forwarding record into the status pipe.
// Format: 1-byte tag (0xFF) followed by sizeof(int) bytes of wait_status.
bool write_wait_status(int fd, int wait_status)
{
    const std::uint8_t tag = SandboxWaitStatusPayload::TAG;
    if (!write_all(fd, &tag, sizeof(tag))) {
        return false;
    }
    return write_all(fd, &wait_status, sizeof(wait_status));
}

// Reads a discriminated status pipe record. The first byte determines type:
//   0xFF → WaitStatus (followed by sizeof(int) bytes)
//   other → treat as first byte of SandboxSetupFailedPayload
StatusPipeReadResult read_status_pipe(int fd, SandboxSetupFailedPayload& failure, int& out_wait_status)
{
    std::uint8_t first = 0;

    while (true) {
        ssize_t n = read(fd, &first, 1);
        if (n == -1) {
            if (errno == EINTR) continue;
            return StatusPipeReadResult::Error;
        }
        if (n == 0) {
            return StatusPipeReadResult::Eof;
        }
        break;
    }

    if (first == SandboxWaitStatusPayload::TAG) {
        // Read the wait_status int
        int ws = 0;
        if (!read_all(fd, &ws, sizeof(ws))) {
            return StatusPipeReadResult::Error;
        }
        out_wait_status = ws;
        return StatusPipeReadResult::WaitStatus;
    }

    // It's the first byte of SandboxSetupFailedPayload — read the rest
    char* data = reinterpret_cast<char*>(&failure);
    data[0] = static_cast<char>(first);
    std::size_t total_read = 1;

    while (total_read < sizeof(failure)) {
        ssize_t bytes_read = read(
            fd,
            data + total_read,
            sizeof(failure) - total_read
        );
        if (bytes_read == -1) {
            if (errno == EINTR) continue;
            return StatusPipeReadResult::Error;
        }
        if (bytes_read == 0) {
            return StatusPipeReadResult::Error;
        }
        total_read += static_cast<std::size_t>(bytes_read);
    }

    return StatusPipeReadResult::Payload;
}