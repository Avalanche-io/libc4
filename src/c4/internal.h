// SPDX-License-Identifier: Apache-2.0
// Internal shared definitions for libc4 implementation
#ifndef C4_INTERNAL_H
#define C4_INTERNAL_H

#include "c4/c4.h"
#include "c4/c4.hpp"

#include <array>
#include <cstdint>
#include <cstring>

// Define the C API opaque type
struct c4_id {
    std::array<uint8_t, c4::DigestLen> digest;
};

// Helper: create a C++ ID from raw digest bytes
inline c4::ID c4_id_from_raw(const uint8_t *digest) {
    return c4::ID::FromDigest(digest, c4::DigestLen);
}

#endif // C4_INTERNAL_H
