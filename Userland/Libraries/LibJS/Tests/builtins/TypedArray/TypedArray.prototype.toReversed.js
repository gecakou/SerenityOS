const TYPED_ARRAYS = [
    Uint8Array,
    Uint8ClampedArray,
    Uint16Array,
    Uint32Array,
    Int8Array,
    Int16Array,
    Int32Array,
    Float32Array,
    Float64Array,
];

const BIGINT_TYPED_ARRAYS = [BigUint64Array, BigInt64Array];

test("length is 0", () => {
    TYPED_ARRAYS.forEach(T => {
        expect(T.prototype.toReversed).toHaveLength(0);
    });

    BIGINT_TYPED_ARRAYS.forEach(T => {
        expect(T.prototype.toReversed).toHaveLength(0);
    });
});

describe("basic functionality", () => {
    test("Odd length array", () => {
        TYPED_ARRAYS.forEach(T => {
            const array = new T([1, 2, 3]);

            expect(array.toReversed()).toEqual([3, 2, 1]);
            expect(array).toEqual([1, 2, 3]);
        });

        BIGINT_TYPED_ARRAYS.forEach(T => {
            const array = new T([1n, 2n, 3n]);
            expect(array.toReversed()).toEqual([3n, 2n, 1n]);
            expect(array).toEqual([1n, 2n, 3n]);
        });
    });

    test("Even length array", () => {
        TYPED_ARRAYS.forEach(T => {
            const array = new T([1, 2]);
            expect(array.toReversed()).toEqual([2, 1]);
            expect(array).toEqual([1, 2]);
        });

        BIGINT_TYPED_ARRAYS.forEach(T => {
            const array = new T([1n, 2n]);
            expect(array.toReversed()).toEqual([2n, 1n]);
            expect(array).toEqual([1n, 2n]);
        });
    });

    test("Empty array", () => {
        TYPED_ARRAYS.forEach(T => {
            const array = new T([]);
            expect(array.toReversed()).toEqual([]);
            expect(array).toEqual([]);
        });

        BIGINT_TYPED_ARRAYS.forEach(T => {
            const array = new T([]);
            expect(array.toReversed()).toEqual([]);
            expect(array).toEqual([]);
        });
    });
});
