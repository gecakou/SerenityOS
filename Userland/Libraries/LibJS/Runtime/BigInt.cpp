/*
 * Copyright (c) 2020-2022, Linus Groh <linusg@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCrypto/BigInt/SignedBigInteger.h>
#include <LibJS/Heap/Heap.h>
#include <LibJS/Runtime/BigInt.h>
#include <LibJS/Runtime/GlobalObject.h>

namespace JS {

BigInt::BigInt(Crypto::SignedBigInteger big_integer)
    : m_big_integer(move(big_integer))
{
    VERIFY(!m_big_integer.is_invalid());
}

BigInt* js_bigint(Heap& heap, Crypto::SignedBigInteger big_integer)
{
    return heap.allocate_without_realm<BigInt>(move(big_integer));
}

BigInt* js_bigint(VM& vm, Crypto::SignedBigInteger big_integer)
{
    return js_bigint(vm.heap(), move(big_integer));
}

// 21.2.1.1.1 NumberToBigInt ( number ), https://tc39.es/ecma262/#sec-numbertobigint
ThrowCompletionOr<BigInt*> number_to_bigint(VM& vm, Value number)
{
    VERIFY(number.is_number());

    // 1. If IsIntegralNumber(number) is false, throw a RangeError exception.
    if (!number.is_integral_number())
        return vm.throw_completion<RangeError>(ErrorType::BigIntFromNonIntegral);

    // 2. Return the BigInt value that represents ℝ(number).
    return js_bigint(vm, Crypto::SignedBigInteger { number.as_double() });
}

}
