function assert(x) { if (!x) throw 1; }

try {
    assert((true && true) === true);
    assert((false && false) === false);
    assert((true && false) === false);
    assert((false && true) === false);
    assert((false && (1 === 2)) === false);
    assert((true && (1 === 2)) === false);
    assert(("" && "") === "");
    assert(("" && false) === "");
    assert(("" && true) === "");
    assert((false && "") === false);
    assert((true && "") === "");
    assert(("foo" && "bar") === "bar");
    assert(("foo" && false) === false);
    assert(("foo" && true) === true);
    assert((false && "bar") === false);
    assert((true && "bar") === "bar");
    assert((null && true) === null);
    assert((0 && false) === 0);
    assert((0 && true) === 0);
    assert((42 && false) === false);
    assert((42 && true) === true);
    assert((false && 0) === false);
    assert((true && 0) === 0);
    assert((false && 42) === false);
    assert((true && 42) === 42);
    assert(([] && false) === false);
    assert(([] && true) === true);
    assert((false && []) === false);
    assert((true && []).length === 0);
    assert((null && false) === null);
    assert((null && true) === null);
    assert((false && null) === false);
    assert((true && null) === null);
    assert((undefined && false) === undefined);
    assert((undefined && true) === undefined);
    assert((false && undefined) === false);
    assert((true && undefined) === undefined);

    assert((true || true) === true);
    assert((false || false) === false);
    assert((true || false) === true);
    assert((false || true) === true);
    assert((false || (1 === 2)) === false);
    assert((true || (1 === 2)) === true);
    assert(("" || "") === "");
    assert(("" || false) === false);
    assert(("" || true) === true);
    assert((false || "") === "");
    assert((true || "") === true);
    assert(("foo" || "bar") === "foo");
    assert(("foo" || false) === "foo");
    assert(("foo" || true) === "foo");
    assert((false || "bar") === "bar");
    assert((true || "bar") === true);
    assert((null || true) === true);
    assert((0 || false) === false);
    assert((0 || true) === true);
    assert((42 || false) === 42);
    assert((42 || true) === 42);
    assert((false || 0) === 0);
    assert((true || 0) === true);
    assert((false || 42) === 42);
    assert((true || 42) === true);
    assert(([] || false).length === 0);
    assert(([] || true).length === 0);
    assert((false || []).length === 0);
    assert((true || []) === true);
    assert((null || false) === false);
    assert((null || true) === true);
    assert((false || null) === null);
    assert((true || null) === true);
    assert((undefined || false) === false);
    assert((undefined || true) === true);
    assert((false || undefined) === undefined);
    assert((true || undefined) === true);

    console.log("PASS");
} catch (e) {
    console.log("FAIL: " + e);
}
