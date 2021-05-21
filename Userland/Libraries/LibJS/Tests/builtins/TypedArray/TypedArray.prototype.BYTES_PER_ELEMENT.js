// Update when more typed arrays get added
const TYPED_ARRAYS = [
    { array: Uint8Array, expected: 1 },
    { array: Uint16Array, expected: 2 },
    { array: Uint32Array, expected: 4 },
    { array: Int8Array, expected: 1 },
    { array: Int16Array, expected: 2 },
    { array: Int32Array, expected: 4 },
    { array: Float32Array, expected: 4 },
    { array: Float64Array, expected: 8 },
];

test("basic functionality", () => {
    TYPED_ARRAYS.forEach(T => {
        const typedArray = new T.array();
        expect(typedArray.BYTES_PER_ELEMENT).toBe(T.expected);
    });
});
