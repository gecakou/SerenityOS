function assert(x) { if (!x) throw 1; }

try {
    var nan = undefined + 1;
    assert(nan + "" == "NaN");
    assert(NaN + "" == "NaN");
    assert(nan !== nan);
    assert(NaN !== NaN);
    assert(isNaN(nan) === true);
    assert(isNaN(NaN) === true);
    assert(isNaN(0) === false);
    assert(isNaN(undefined) === true);
    assert(isNaN(null) === false);
    console.log("PASS");
} catch (e) {
    console.log("FAIL: " + e);
}
