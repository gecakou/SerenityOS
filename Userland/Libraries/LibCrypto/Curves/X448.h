/*
 * Copyright (c) 2022, stelar7 <dudedbz@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteBuffer.h>
#include <LibCrypto/Curves/EllipticCurve.h>

namespace Crypto::Curves {

class X448 : public EllipticCurve {

    static constexpr u16 BITS = 448;
    static constexpr u8 BYTES = 56;
    static constexpr u8 WORDS = 14;
    static constexpr u32 A24 = 39082;

public:
    size_t key_size() override { return BYTES; }
    ErrorOr<ByteBuffer> generate_private_key() override;
    ErrorOr<ByteBuffer> generate_public_key(ReadonlyBytes a) override;
    ErrorOr<ByteBuffer> compute_coordinate(ReadonlyBytes a, ReadonlyBytes b) override;
    ErrorOr<ByteBuffer> derive_premaster_key(ReadonlyBytes shared_point) override;

private:
    static void import_state(u32* state, ReadonlyBytes data);
    static ErrorOr<ByteBuffer> export_state(u32* data);
    static void select(u32* state, u32* a, u32* b, u32 condition);
    static void set(u32* state, u32 value);
    static void copy(u32* state, u32* value);
    static void conditional_swap(u32* first, u32* second, u32 condition);
    static void modular_multiply_single(u32* state, u32* first, u32 second);
    static void modular_square(u32* state, u32* value);
    static void modular_multiply(u32* state, u32* first, u32* second);
    static void modular_add(u32* state, u32* first, u32* second);
    static void modular_subtract(u32* state, u32* first, u32* second);
    static void modular_reduce(u32* state, u32* data, u32 data_high);
    static void to_power_of_2n(u32* state, u32* value, u8 n);
    static void modular_multiply_inverse(u32* state, u32* value);
};

}
