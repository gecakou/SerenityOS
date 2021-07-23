describe("correct behavior", () => {
    test("basic functionality", () => {
        const date = new Temporal.PlainDate(2021, 7, 23);
        expect(date.dayOfWeek).toBe(5);
    });
});

test("errors", () => {
    test("this value must be a Temporal.PlainDate object", () => {
        expect(() => {
            Reflect.get(Temporal.PlainDate.prototype, "dayOfWeek", "foo");
        }).toThrowWithMessage(TypeError, "Not a Temporal.PlainDate");
    });
});
