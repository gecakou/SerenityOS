/*
 * Copyright (c) 2020-2021, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2020-2022, Linus Groh <linusg@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Checked.h>
#include <AK/Function.h>
#include <AK/Utf16View.h>
#include <LibJS/Heap/Heap.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Completion.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/Intl/AbstractOperations.h>
#include <LibJS/Runtime/Intl/Collator.h>
#include <LibJS/Runtime/Intl/CollatorCompareFunction.h>
#include <LibJS/Runtime/Intl/CollatorConstructor.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/RegExpObject.h>
#include <LibJS/Runtime/StringIterator.h>
#include <LibJS/Runtime/StringObject.h>
#include <LibJS/Runtime/StringPrototype.h>
#include <LibJS/Runtime/ThrowableStringBuilder.h>
#include <LibJS/Runtime/Utf16String.h>
#include <LibJS/Runtime/Value.h>
#include <LibLocale/Locale.h>
#include <LibUnicode/CharacterTypes.h>
#include <LibUnicode/Normalize.h>
#include <string.h>

namespace JS {

static ThrowCompletionOr<DeprecatedString> ak_string_from(VM& vm)
{
    auto this_value = TRY(require_object_coercible(vm, vm.this_value()));
    return TRY(this_value.to_string(vm));
}

static ThrowCompletionOr<Utf16String> utf16_string_from(VM& vm)
{
    auto this_value = TRY(require_object_coercible(vm, vm.this_value()));
    return TRY(this_value.to_utf16_string(vm));
}

// 22.1.3.21.1 SplitMatch ( S, q, R ), https://tc39.es/ecma262/#sec-splitmatch
// FIXME: This no longer exists in the spec!
static Optional<size_t> split_match(Utf16View const& haystack, size_t start, Utf16View const& needle)
{
    auto r = needle.length_in_code_units();
    auto s = haystack.length_in_code_units();
    if (start + r > s)
        return {};
    for (size_t i = 0; i < r; ++i) {
        if (haystack.code_unit_at(start + i) != needle.code_unit_at(i))
            return {};
    }
    return start + r;
}

// 6.1.4.1 StringIndexOf ( string, searchValue, fromIndex ), https://tc39.es/ecma262/#sec-stringindexof
static Optional<size_t> string_index_of(Utf16View const& string, Utf16View const& search_value, size_t from_index)
{
    size_t string_length = string.length_in_code_units();
    size_t search_length = search_value.length_in_code_units();

    if ((search_length == 0) && (from_index <= string_length))
        return from_index;

    if (search_length > string_length)
        return {};

    for (size_t i = from_index; i <= string_length - search_length; ++i) {
        auto candidate = string.substring_view(i, search_length);
        if (candidate == search_value)
            return i;
    }

    return {};
}

// 7.2.9 Static Semantics: IsStringWellFormedUnicode ( string )
static bool is_string_well_formed_unicode(Utf16View string)
{
    // 1. Let strLen be the length of string.
    auto length = string.length_in_code_units();

    // 2. Let k be 0.
    size_t k = 0;

    // 3. Repeat, while k ≠ strLen,
    while (k != length) {
        // a. Let cp be CodePointAt(string, k).
        auto code_point = code_point_at(string, k);

        // b. If cp.[[IsUnpairedSurrogate]] is true, return false.
        if (code_point.is_unpaired_surrogate)
            return false;

        // c. Set k to k + cp.[[CodeUnitCount]].
        k += code_point.code_unit_count;
    }

    // 4. Return true.
    return true;
}

// 11.1.4 CodePointAt ( string, position ), https://tc39.es/ecma262/#sec-codepointat
CodePoint code_point_at(Utf16View const& string, size_t position)
{
    VERIFY(position < string.length_in_code_units());

    auto first = string.code_unit_at(position);
    auto code_point = static_cast<u32>(first);

    if (!Utf16View::is_high_surrogate(first) && !Utf16View::is_low_surrogate(first))
        return { false, code_point, 1 };

    if (Utf16View::is_low_surrogate(first) || (position + 1 == string.length_in_code_units()))
        return { true, code_point, 1 };

    auto second = string.code_unit_at(position + 1);

    if (!Utf16View::is_low_surrogate(second))
        return { true, code_point, 1 };

    code_point = Utf16View::decode_surrogate_pair(first, second);
    return { false, code_point, 2 };
}

StringPrototype::StringPrototype(Realm& realm)
    : StringObject(*PrimitiveString::create(realm.vm(), DeprecatedString::empty()), *realm.intrinsics().object_prototype())
{
}

void StringPrototype::initialize(Realm& realm)
{
    auto& vm = this->vm();
    StringObject::initialize(realm);
    u8 attr = Attribute::Writable | Attribute::Configurable;

    // 22.1.3 Properties of the String Prototype Object, https://tc39.es/ecma262/#sec-properties-of-the-string-prototype-object
    define_native_function(realm, vm.names.at, at, 1, attr);
    define_native_function(realm, vm.names.charAt, char_at, 1, attr);
    define_native_function(realm, vm.names.charCodeAt, char_code_at, 1, attr);
    define_native_function(realm, vm.names.codePointAt, code_point_at, 1, attr);
    define_native_function(realm, vm.names.concat, concat, 1, attr);
    define_native_function(realm, vm.names.endsWith, ends_with, 1, attr);
    define_native_function(realm, vm.names.includes, includes, 1, attr);
    define_native_function(realm, vm.names.indexOf, index_of, 1, attr);
    define_native_function(realm, vm.names.isWellFormed, is_well_formed, 0, attr);
    define_native_function(realm, vm.names.lastIndexOf, last_index_of, 1, attr);
    define_native_function(realm, vm.names.localeCompare, locale_compare, 1, attr);
    define_native_function(realm, vm.names.match, match, 1, attr);
    define_native_function(realm, vm.names.matchAll, match_all, 1, attr);
    define_native_function(realm, vm.names.normalize, normalize, 0, attr);
    define_native_function(realm, vm.names.padEnd, pad_end, 1, attr);
    define_native_function(realm, vm.names.padStart, pad_start, 1, attr);
    define_native_function(realm, vm.names.repeat, repeat, 1, attr);
    define_native_function(realm, vm.names.replace, replace, 2, attr);
    define_native_function(realm, vm.names.replaceAll, replace_all, 2, attr);
    define_native_function(realm, vm.names.search, search, 1, attr);
    define_native_function(realm, vm.names.slice, slice, 2, attr);
    define_native_function(realm, vm.names.split, split, 2, attr);
    define_native_function(realm, vm.names.startsWith, starts_with, 1, attr);
    define_native_function(realm, vm.names.substring, substring, 2, attr);
    define_native_function(realm, vm.names.toLocaleLowerCase, to_locale_lowercase, 0, attr);
    define_native_function(realm, vm.names.toLocaleUpperCase, to_locale_uppercase, 0, attr);
    define_native_function(realm, vm.names.toLowerCase, to_lowercase, 0, attr);
    define_native_function(realm, vm.names.toString, to_string, 0, attr);
    define_native_function(realm, vm.names.toUpperCase, to_uppercase, 0, attr);
    define_native_function(realm, vm.names.toWellFormed, to_well_formed, 0, attr);
    define_native_function(realm, vm.names.trim, trim, 0, attr);
    define_native_function(realm, vm.names.trimEnd, trim_end, 0, attr);
    define_native_function(realm, vm.names.trimStart, trim_start, 0, attr);
    define_native_function(realm, vm.names.valueOf, value_of, 0, attr);
    define_native_function(realm, *vm.well_known_symbol_iterator(), symbol_iterator, 0, attr);

    // B.2.2 Additional Properties of the String.prototype Object, https://tc39.es/ecma262/#sec-additional-properties-of-the-string.prototype-object
    define_native_function(realm, vm.names.substr, substr, 2, attr);
    define_native_function(realm, vm.names.anchor, anchor, 1, attr);
    define_native_function(realm, vm.names.big, big, 0, attr);
    define_native_function(realm, vm.names.blink, blink, 0, attr);
    define_native_function(realm, vm.names.bold, bold, 0, attr);
    define_native_function(realm, vm.names.fixed, fixed, 0, attr);
    define_native_function(realm, vm.names.fontcolor, fontcolor, 1, attr);
    define_native_function(realm, vm.names.fontsize, fontsize, 1, attr);
    define_native_function(realm, vm.names.italics, italics, 0, attr);
    define_native_function(realm, vm.names.link, link, 1, attr);
    define_native_function(realm, vm.names.small, small, 0, attr);
    define_native_function(realm, vm.names.strike, strike, 0, attr);
    define_native_function(realm, vm.names.sub, sub, 0, attr);
    define_native_function(realm, vm.names.sup, sup, 0, attr);
    define_direct_property(vm.names.trimLeft, get_without_side_effects(vm.names.trimStart), attr);
    define_direct_property(vm.names.trimRight, get_without_side_effects(vm.names.trimEnd), attr);
}

// thisStringValue ( value ), https://tc39.es/ecma262/#thisstringvalue
static ThrowCompletionOr<PrimitiveString*> this_string_value(VM& vm, Value value)
{
    if (value.is_string())
        return &value.as_string();
    if (value.is_object() && is<StringObject>(value.as_object()))
        return &static_cast<StringObject&>(value.as_object()).primitive_string();
    return vm.throw_completion<TypeError>(ErrorType::NotAnObjectOfType, "String");
}

// 22.1.3.1 String.prototype.at ( index ), https://tc39.es/ecma262/#sec-string.prototype.at
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::at)
{
    // 1. Let O be ? ToObject(this value).
    auto string = TRY(utf16_string_from(vm));
    // 2. Let len be ? LengthOfArrayLike(O).
    auto length = string.length_in_code_units();

    // 3. Let relativeIndex be ? ToIntegerOrInfinity(index).
    auto relative_index = TRY(vm.argument(0).to_integer_or_infinity(vm));
    if (Value(relative_index).is_infinity())
        return js_undefined();

    Checked<size_t> index { 0 };
    // 4. If relativeIndex ≥ 0, then
    if (relative_index >= 0) {
        // a. Let k be relativeIndex.
        index += relative_index;
    }
    // 5. Else,
    else {
        // a. Let k be len + relativeIndex.
        index += length;
        index -= -relative_index;
    }
    // 6. If k < 0 or k ≥ len, return undefined.
    if (index.has_overflow() || index.value() >= length)
        return js_undefined();

    // 7. Return ? Get(O, ! ToString(𝔽(k))).
    return PrimitiveString::create(vm, string.substring_view(index.value(), 1));
}

// 22.1.3.2 String.prototype.charAt ( pos ), https://tc39.es/ecma262/#sec-string.prototype.charat
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::char_at)
{
    auto string = TRY(utf16_string_from(vm));
    auto position = TRY(vm.argument(0).to_integer_or_infinity(vm));
    if (position < 0 || position >= string.length_in_code_units())
        return PrimitiveString::create(vm, DeprecatedString::empty());

    return PrimitiveString::create(vm, string.substring_view(position, 1));
}

// 22.1.3.3 String.prototype.charCodeAt ( pos ), https://tc39.es/ecma262/#sec-string.prototype.charcodeat
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::char_code_at)
{
    auto string = TRY(utf16_string_from(vm));
    auto position = TRY(vm.argument(0).to_integer_or_infinity(vm));
    if (position < 0 || position >= string.length_in_code_units())
        return js_nan();

    return Value(string.code_unit_at(position));
}

// 22.1.3.4 String.prototype.codePointAt ( pos ), https://tc39.es/ecma262/#sec-string.prototype.codepointat
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::code_point_at)
{
    auto string = TRY(utf16_string_from(vm));
    auto position = TRY(vm.argument(0).to_integer_or_infinity(vm));
    if (position < 0 || position >= string.length_in_code_units())
        return js_undefined();

    auto code_point = JS::code_point_at(string.view(), position);
    return Value(code_point.code_point);
}

// 22.1.3.5 String.prototype.concat ( ...args ), https://tc39.es/ecma262/#sec-string.prototype.concat
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::concat)
{
    // 1. Let O be ? RequireObjectCoercible(this value).
    auto object = TRY(require_object_coercible(vm, vm.this_value()));

    // 2. Let S be ? ToString(O).
    auto* string = TRY(object.to_primitive_string(vm));

    // 3. Let R be S.
    auto* result = string;

    // 4. For each element next of args, do
    for (size_t i = 0; i < vm.argument_count(); ++i) {
        // a. Let nextString be ? ToString(next).
        auto* next_string = TRY(vm.argument(i).to_primitive_string(vm));

        // b. Set R to the string-concatenation of R and nextString.
        result = PrimitiveString::create(vm, *result, *next_string);
    }

    // 5. Return R.
    return result;
}

// 22.1.3.7 String.prototype.endsWith ( searchString [ , endPosition ] ), https://tc39.es/ecma262/#sec-string.prototype.endswith
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::ends_with)
{
    auto string = TRY(utf16_string_from(vm));

    auto search_string_value = vm.argument(0);

    bool search_is_regexp = TRY(search_string_value.is_regexp(vm));
    if (search_is_regexp)
        return vm.throw_completion<TypeError>(ErrorType::IsNotA, "searchString", "string, but a regular expression");

    auto search_string = TRY(search_string_value.to_utf16_string(vm));
    auto string_length = string.length_in_code_units();
    auto search_length = search_string.length_in_code_units();

    size_t end = string_length;
    if (!vm.argument(1).is_undefined()) {
        auto position = TRY(vm.argument(1).to_integer_or_infinity(vm));
        end = clamp(position, static_cast<double>(0), static_cast<double>(string_length));
    }

    if (search_length == 0)
        return Value(true);
    if (search_length > end)
        return Value(false);

    size_t start = end - search_length;

    auto substring_view = string.substring_view(start, end - start);
    return Value(substring_view == search_string.view());
}

// 22.1.3.8 String.prototype.includes ( searchString [ , position ] ), https://tc39.es/ecma262/#sec-string.prototype.includes
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::includes)
{
    auto string = TRY(utf16_string_from(vm));

    auto search_string_value = vm.argument(0);

    bool search_is_regexp = TRY(search_string_value.is_regexp(vm));
    if (search_is_regexp)
        return vm.throw_completion<TypeError>(ErrorType::IsNotA, "searchString", "string, but a regular expression");

    auto search_string = TRY(search_string_value.to_utf16_string(vm));

    size_t start = 0;
    if (!vm.argument(1).is_undefined()) {
        auto position = TRY(vm.argument(1).to_integer_or_infinity(vm));
        start = clamp(position, static_cast<double>(0), static_cast<double>(string.length_in_code_units()));
    }

    auto index = string_index_of(string.view(), search_string.view(), start);
    return Value(index.has_value());
}

// 22.1.3.9 String.prototype.indexOf ( searchString [ , position ] ), https://tc39.es/ecma262/#sec-string.prototype.indexof
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::index_of)
{
    auto string = TRY(utf16_string_from(vm));

    auto search_string = TRY(vm.argument(0).to_utf16_string(vm));
    auto utf16_string_view = string.view();
    auto utf16_search_view = search_string.view();

    size_t start = 0;
    if (vm.argument_count() > 1) {
        auto position = TRY(vm.argument(1).to_integer_or_infinity(vm));
        start = clamp(position, static_cast<double>(0), static_cast<double>(utf16_string_view.length_in_code_units()));
    }

    auto index = string_index_of(utf16_string_view, utf16_search_view, start);
    return index.has_value() ? Value(*index) : Value(-1);
}

// 22.1.3.10 String.prototype.isWellFormed ( ), https://tc39.es/proposal-is-usv-string/#sec-string.prototype.iswellformed
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::is_well_formed)
{
    // 1. Let O be ? RequireObjectCoercible(this value).
    // 2. Let S be ? ToString(O).
    auto string = TRY(utf16_string_from(vm));

    // 3. Return IsStringWellFormedUnicode(S).
    return is_string_well_formed_unicode(string.view());
}

// 22.1.3.10 String.prototype.lastIndexOf ( searchString [ , position ] ), https://tc39.es/ecma262/#sec-string.prototype.lastindexof
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::last_index_of)
{
    auto string = TRY(utf16_string_from(vm));

    auto search_string = TRY(vm.argument(0).to_utf16_string(vm));
    auto string_length = string.length_in_code_units();
    auto search_length = search_string.length_in_code_units();

    auto position = TRY(vm.argument(1).to_number(vm));
    double pos = position.is_nan() ? static_cast<double>(INFINITY) : TRY(position.to_integer_or_infinity(vm));

    size_t start = clamp(pos, static_cast<double>(0), static_cast<double>(string_length));
    Optional<size_t> last_index;

    for (size_t k = 0; (k <= start) && (k + search_length <= string_length); ++k) {
        bool is_match = true;

        for (size_t j = 0; j < search_length; ++j) {
            if (string.code_unit_at(k + j) != search_string.code_unit_at(j)) {
                is_match = false;
                break;
            }
        }

        if (is_match)
            last_index = k;
    }

    return last_index.has_value() ? Value(*last_index) : Value(-1);
}

// 22.1.3.11 String.prototype.localeCompare ( that [ , reserved1 [ , reserved2 ] ] ), https://tc39.es/ecma262/#sec-string.prototype.localecompare
// 19.1.1 String.prototype.localeCompare ( that [ , locales [ , options ] ] ), https://tc39.es/ecma402/#sup-String.prototype.localeCompare
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::locale_compare)
{
    auto& realm = *vm.current_realm();

    // 1. Let O be ? RequireObjectCoercible(this value).
    auto object = TRY(require_object_coercible(vm, vm.this_value()));

    // 2. Let S be ? ToString(O).
    auto string = TRY(object.to_string(vm));

    // 3. Let thatValue be ? ToString(that).
    auto that_value = TRY(vm.argument(0).to_string(vm));

    // 4. Let collator be ? Construct(%Collator%, « locales, options »).
    auto collator = TRY(construct(vm, *realm.intrinsics().intl_collator_constructor(), vm.argument(1), vm.argument(2)));

    // 5. Return CompareStrings(collator, S, thatValue).
    return Intl::compare_strings(static_cast<Intl::Collator&>(*collator), Utf8View(string), Utf8View(that_value));
}

// 22.1.3.12 String.prototype.match ( regexp ), https://tc39.es/ecma262/#sec-string.prototype.match
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::match)
{
    auto this_object = TRY(require_object_coercible(vm, vm.this_value()));
    auto regexp = vm.argument(0);
    if (!regexp.is_nullish()) {
        if (auto* matcher = TRY(regexp.get_method(vm, *vm.well_known_symbol_match())))
            return TRY(call(vm, *matcher, regexp, this_object));
    }

    auto string = TRY(this_object.to_utf16_string(vm));

    auto rx = TRY(regexp_create(vm, regexp, js_undefined()));
    return TRY(Value(rx).invoke(vm, *vm.well_known_symbol_match(), PrimitiveString::create(vm, move(string))));
}

// 22.1.3.13 String.prototype.matchAll ( regexp ), https://tc39.es/ecma262/#sec-string.prototype.matchall
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::match_all)
{
    auto this_object = TRY(require_object_coercible(vm, vm.this_value()));
    auto regexp = vm.argument(0);
    if (!regexp.is_nullish()) {
        auto is_regexp = TRY(regexp.is_regexp(vm));
        if (is_regexp) {
            auto flags = TRY(regexp.as_object().get("flags"));
            auto flags_object = TRY(require_object_coercible(vm, flags));
            auto flags_string = TRY(flags_object.to_string(vm));
            if (!flags_string.contains('g'))
                return vm.throw_completion<TypeError>(ErrorType::StringNonGlobalRegExp);
        }
        if (auto* matcher = TRY(regexp.get_method(vm, *vm.well_known_symbol_match_all())))
            return TRY(call(vm, *matcher, regexp, this_object));
    }

    auto string = TRY(this_object.to_utf16_string(vm));

    auto rx = TRY(regexp_create(vm, regexp, PrimitiveString::create(vm, "g")));
    return TRY(Value(rx).invoke(vm, *vm.well_known_symbol_match_all(), PrimitiveString::create(vm, move(string))));
}

// 22.1.3.14 String.prototype.normalize ( [ form ] ), https://tc39.es/ecma262/#sec-string.prototype.normalize
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::normalize)
{
    // 1. Let O be ? RequireObjectCoercible(this value).
    // 2. Let S be ? ToString(O).
    auto string = TRY(ak_string_from(vm));

    // 3. If form is undefined, let f be "NFC".
    // 4. Else, let f be ? ToString(form).
    DeprecatedString form = "NFC";
    auto form_value = vm.argument(0);
    if (!form_value.is_undefined())
        form = TRY(form_value.to_string(vm));

    // 5. If f is not one of "NFC", "NFD", "NFKC", or "NFKD", throw a RangeError exception.
    if (!form.is_one_of("NFC"sv, "NFD"sv, "NFKC"sv, "NFKD"sv))
        return vm.throw_completion<RangeError>(ErrorType::InvalidNormalizationForm, form);

    // 6. Let ns be the String value that is the result of normalizing S into the normalization form named by f as specified in https://unicode.org/reports/tr15/.
    auto unicode_form = Unicode::normalization_form_from_string(form);
    auto ns = Unicode::normalize(string, unicode_form);

    // 7. return ns.
    return PrimitiveString::create(vm, move(ns));
}

enum class PadPlacement {
    Start,
    End,
};

// 22.1.3.16.1 StringPad ( O, maxLength, fillString, placement ), https://tc39.es/ecma262/#sec-stringpad
static ThrowCompletionOr<Value> pad_string(VM& vm, Utf16String string, PadPlacement placement)
{
    auto string_length = string.length_in_code_units();

    auto max_length = TRY(vm.argument(0).to_length(vm));
    if (max_length <= string_length)
        return PrimitiveString::create(vm, move(string));

    Utf16String fill_string(Utf16Data { 0x20 });
    if (!vm.argument(1).is_undefined()) {
        fill_string = TRY(vm.argument(1).to_utf16_string(vm));
        if (fill_string.is_empty())
            return PrimitiveString::create(vm, move(string));
    }

    auto fill_code_units = fill_string.length_in_code_units();
    auto fill_length = max_length - string_length;

    ThrowableStringBuilder filler_builder(vm);
    for (size_t i = 0; i < fill_length / fill_code_units; ++i)
        TRY(filler_builder.append(fill_string.view()));

    TRY(filler_builder.append(fill_string.substring_view(0, fill_length % fill_code_units)));
    auto filler = filler_builder.build();

    auto formatted = placement == PadPlacement::Start
        ? DeprecatedString::formatted("{}{}", filler, string.view())
        : DeprecatedString::formatted("{}{}", string.view(), filler);
    return PrimitiveString::create(vm, move(formatted));
}

// 22.1.3.15 String.prototype.padEnd ( maxLength [ , fillString ] ), https://tc39.es/ecma262/#sec-string.prototype.padend
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::pad_end)
{
    auto string = TRY(utf16_string_from(vm));
    return pad_string(vm, move(string), PadPlacement::End);
}

// 22.1.3.16 String.prototype.padStart ( maxLength [ , fillString ] ), https://tc39.es/ecma262/#sec-string.prototype.padstart
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::pad_start)
{
    auto string = TRY(utf16_string_from(vm));
    return pad_string(vm, move(string), PadPlacement::Start);
}

// 22.1.3.17 String.prototype.repeat ( count ), https://tc39.es/ecma262/#sec-string.prototype.repeat
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::repeat)
{
    auto string = TRY(ak_string_from(vm));

    auto n = TRY(vm.argument(0).to_integer_or_infinity(vm));

    if (n < 0)
        return vm.throw_completion<RangeError>(ErrorType::StringRepeatCountMustBe, "positive");

    if (Value(n).is_positive_infinity())
        return vm.throw_completion<RangeError>(ErrorType::StringRepeatCountMustBe, "finite");

    if (n == 0)
        return PrimitiveString::create(vm, DeprecatedString::empty());

    // NOTE: This is an optimization, it is not required by the specification but it produces equivalent behavior
    if (string.is_empty())
        return PrimitiveString::create(vm, DeprecatedString::empty());

    ThrowableStringBuilder builder(vm);
    for (size_t i = 0; i < n; ++i)
        TRY(builder.append(string));
    return PrimitiveString::create(vm, builder.to_deprecated_string());
}

// 22.1.3.18 String.prototype.replace ( searchValue, replaceValue ), https://tc39.es/ecma262/#sec-string.prototype.replace
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::replace)
{
    auto this_object = TRY(require_object_coercible(vm, vm.this_value()));
    auto search_value = vm.argument(0);
    auto replace_value = vm.argument(1);

    if (!search_value.is_nullish()) {
        if (auto* replacer = TRY(search_value.get_method(vm, *vm.well_known_symbol_replace())))
            return TRY(call(vm, *replacer, search_value, this_object, replace_value));
    }

    auto string = TRY(this_object.to_utf16_string(vm));
    auto search_string = TRY(search_value.to_utf16_string(vm));

    if (!replace_value.is_function()) {
        auto replace_string = TRY(replace_value.to_utf16_string(vm));
        replace_value = PrimitiveString::create(vm, move(replace_string));
    }

    Optional<size_t> position = string_index_of(string.view(), search_string.view(), 0);
    if (!position.has_value())
        return PrimitiveString::create(vm, move(string));

    auto preserved = string.substring_view(0, position.value());
    DeprecatedString replacement;

    if (replace_value.is_function()) {
        auto result = TRY(call(vm, replace_value.as_function(), js_undefined(), PrimitiveString::create(vm, search_string), Value(position.value()), PrimitiveString::create(vm, string)));
        replacement = TRY(result.to_string(vm));
    } else {
        replacement = TRY(get_substitution(vm, search_string.view(), string.view(), *position, {}, js_undefined(), replace_value));
    }

    ThrowableStringBuilder builder(vm);
    TRY(builder.append(preserved));
    TRY(builder.append(replacement));
    TRY(builder.append(string.substring_view(*position + search_string.length_in_code_units())));

    return PrimitiveString::create(vm, builder.build());
}

// 22.1.3.19 String.prototype.replaceAll ( searchValue, replaceValue ), https://tc39.es/ecma262/#sec-string.prototype.replaceall
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::replace_all)
{
    auto this_object = TRY(require_object_coercible(vm, vm.this_value()));
    auto search_value = vm.argument(0);
    auto replace_value = vm.argument(1);

    if (!search_value.is_nullish()) {
        bool is_regexp = TRY(search_value.is_regexp(vm));

        if (is_regexp) {
            auto flags = TRY(search_value.as_object().get(vm.names.flags));
            auto flags_object = TRY(require_object_coercible(vm, flags));
            auto flags_string = TRY(flags_object.to_string(vm));
            if (!flags_string.contains('g'))
                return vm.throw_completion<TypeError>(ErrorType::StringNonGlobalRegExp);
        }

        auto* replacer = TRY(search_value.get_method(vm, *vm.well_known_symbol_replace()));
        if (replacer)
            return TRY(call(vm, *replacer, search_value, this_object, replace_value));
    }

    auto string = TRY(this_object.to_utf16_string(vm));
    auto search_string = TRY(search_value.to_utf16_string(vm));

    if (!replace_value.is_function()) {
        auto replace_string = TRY(replace_value.to_utf16_string(vm));
        replace_value = PrimitiveString::create(vm, move(replace_string));
    }

    auto string_length = string.length_in_code_units();
    auto search_length = search_string.length_in_code_units();

    Vector<size_t> match_positions;
    size_t advance_by = max(1u, search_length);
    auto position = string_index_of(string.view(), search_string.view(), 0);

    while (position.has_value()) {
        match_positions.append(*position);
        position = string_index_of(string.view(), search_string.view(), *position + advance_by);
    }

    size_t end_of_last_match = 0;
    ThrowableStringBuilder result(vm);

    for (auto position : match_positions) {
        auto preserved = string.substring_view(end_of_last_match, position - end_of_last_match);
        DeprecatedString replacement;

        if (replace_value.is_function()) {
            auto result = TRY(call(vm, replace_value.as_function(), js_undefined(), PrimitiveString::create(vm, search_string), Value(position), PrimitiveString::create(vm, string)));
            replacement = TRY(result.to_string(vm));
        } else {
            replacement = TRY(get_substitution(vm, search_string.view(), string.view(), position, {}, js_undefined(), replace_value));
        }

        TRY(result.append(preserved));
        TRY(result.append(replacement));

        end_of_last_match = position + search_length;
    }

    if (end_of_last_match < string_length)
        TRY(result.append(string.substring_view(end_of_last_match)));

    return PrimitiveString::create(vm, result.build());
}

// 22.1.3.20 String.prototype.search ( regexp ), https://tc39.es/ecma262/#sec-string.prototype.search
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::search)
{
    auto this_object = TRY(require_object_coercible(vm, vm.this_value()));
    auto regexp = vm.argument(0);
    if (!regexp.is_nullish()) {
        if (auto* searcher = TRY(regexp.get_method(vm, *vm.well_known_symbol_search())))
            return TRY(call(vm, *searcher, regexp, this_object));
    }

    auto string = TRY(this_object.to_utf16_string(vm));

    auto rx = TRY(regexp_create(vm, regexp, js_undefined()));
    return TRY(Value(rx).invoke(vm, *vm.well_known_symbol_search(), PrimitiveString::create(vm, move(string))));
}

// 22.1.3.21 String.prototype.slice ( start, end ), https://tc39.es/ecma262/#sec-string.prototype.slice
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::slice)
{
    auto string = TRY(utf16_string_from(vm));
    auto string_length = static_cast<double>(string.length_in_code_units());

    auto int_start = TRY(vm.argument(0).to_integer_or_infinity(vm));
    if (Value(int_start).is_negative_infinity())
        int_start = 0;
    else if (int_start < 0)
        int_start = max(string_length + int_start, 0);
    else
        int_start = min(int_start, string_length);

    auto int_end = string_length;
    if (!vm.argument(1).is_undefined()) {
        int_end = TRY(vm.argument(1).to_integer_or_infinity(vm));
        if (Value(int_end).is_negative_infinity())
            int_end = 0;
        else if (int_end < 0)
            int_end = max(string_length + int_end, 0);
        else
            int_end = min(int_end, string_length);
    }

    if (int_start >= int_end)
        return PrimitiveString::create(vm, DeprecatedString::empty());

    return PrimitiveString::create(vm, string.substring_view(int_start, int_end - int_start));
}

// 22.1.3.22 String.prototype.split ( separator, limit ), https://tc39.es/ecma262/#sec-string.prototype.split
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::split)
{
    auto& realm = *vm.current_realm();

    auto object = TRY(require_object_coercible(vm, vm.this_value()));

    auto separator_argument = vm.argument(0);
    auto limit_argument = vm.argument(1);

    if (!separator_argument.is_nullish()) {
        auto splitter = TRY(separator_argument.get_method(vm, *vm.well_known_symbol_split()));
        if (splitter)
            return TRY(call(vm, *splitter, separator_argument, object, limit_argument));
    }

    auto string = TRY(object.to_utf16_string(vm));

    auto array = MUST(Array::create(realm, 0));
    size_t array_length = 0;

    auto limit = NumericLimits<u32>::max();
    if (!limit_argument.is_undefined())
        limit = TRY(limit_argument.to_u32(vm));

    auto separator = TRY(separator_argument.to_utf16_string(vm));

    if (limit == 0)
        return array;

    auto string_length = string.length_in_code_units();
    auto separator_length = separator.length_in_code_units();

    if (separator_argument.is_undefined()) {
        MUST(array->create_data_property_or_throw(0, PrimitiveString::create(vm, move(string))));
        return array;
    }

    if (string_length == 0) {
        if (separator_length > 0)
            MUST(array->create_data_property_or_throw(0, PrimitiveString::create(vm, move(string))));
        return array;
    }

    size_t start = 0;      // 'p' in the spec.
    auto position = start; // 'q' in the spec.
    while (position != string_length) {
        auto match = split_match(string.view(), position, separator.view()); // 'e' in the spec.
        if (!match.has_value() || match.value() == start) {
            ++position;
            continue;
        }

        auto segment = string.substring_view(start, position - start);
        MUST(array->create_data_property_or_throw(array_length, PrimitiveString::create(vm, segment)));
        ++array_length;
        if (array_length == limit)
            return array;
        start = match.value();
        position = start;
    }

    auto rest = string.substring_view(start);
    MUST(array->create_data_property_or_throw(array_length, PrimitiveString::create(vm, rest)));

    return array;
}

// 22.1.3.23 String.prototype.startsWith ( searchString [ , position ] ), https://tc39.es/ecma262/#sec-string.prototype.startswith
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::starts_with)
{
    auto string = TRY(utf16_string_from(vm));

    auto search_string_value = vm.argument(0);

    bool search_is_regexp = TRY(search_string_value.is_regexp(vm));
    if (search_is_regexp)
        return vm.throw_completion<TypeError>(ErrorType::IsNotA, "searchString", "string, but a regular expression");

    auto search_string = TRY(search_string_value.to_utf16_string(vm));
    auto string_length = string.length_in_code_units();
    auto search_length = search_string.length_in_code_units();

    size_t start = 0;
    if (!vm.argument(1).is_undefined()) {
        auto position = TRY(vm.argument(1).to_integer_or_infinity(vm));
        start = clamp(position, static_cast<double>(0), static_cast<double>(string_length));
    }

    if (search_length == 0)
        return Value(true);

    size_t end = start + search_length;
    if (end > string_length)
        return Value(false);

    auto substring_view = string.substring_view(start, end - start);
    return Value(substring_view == search_string.view());
}

// 22.1.3.24 String.prototype.substring ( start, end ), https://tc39.es/ecma262/#sec-string.prototype.substring
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::substring)
{
    // 1. Let O be ? RequireObjectCoercible(this value).
    // 2. Let S be ? ToString(O).
    auto string = TRY(utf16_string_from(vm));

    // 3. Let len be the length of S.
    auto string_length = static_cast<double>(string.length_in_code_units());

    // 4. Let intStart be ? ToIntegerOrInfinity(start).
    auto start = TRY(vm.argument(0).to_integer_or_infinity(vm));
    // 5. If end is undefined, let intEnd be len; else let intEnd be ? ToIntegerOrInfinity(end).
    auto end = string_length;
    if (!vm.argument(1).is_undefined())
        end = TRY(vm.argument(1).to_integer_or_infinity(vm));

    // 6. Let finalStart be the result of clamping intStart between 0 and len.
    size_t final_start = clamp(start, static_cast<double>(0), string_length);
    // 7. Let finalEnd be the result of clamping intEnd between 0 and len.
    size_t final_end = clamp(end, static_cast<double>(0), string_length);

    // 8. Let from be min(finalStart, finalEnd).
    size_t from = min(final_start, final_end);
    // 9. Let to be max(finalStart, finalEnd).
    size_t to = max(final_start, final_end);

    // 10. Return the substring of S from from to to.
    return PrimitiveString::create(vm, string.substring_view(from, to - from));
}

enum class TargetCase {
    Lower,
    Upper,
};

// 19.1.2.1 TransformCase ( S, locales, targetCase ), https://tc39.es/ecma402/#sec-transform-case
static ThrowCompletionOr<DeprecatedString> transform_case(VM& vm, StringView string, Value locales, TargetCase target_case)
{
    // 1. Let requestedLocales be ? CanonicalizeLocaleList(locales).
    auto requested_locales = TRY(Intl::canonicalize_locale_list(vm, locales));

    Optional<Locale::LocaleID> requested_locale;

    // 2. If requestedLocales is not an empty List, then
    if (!requested_locales.is_empty()) {
        // a. Let requestedLocale be requestedLocales[0].
        requested_locale = Locale::parse_unicode_locale_id(requested_locales[0]);
    }
    // 3. Else,
    else {
        // a. Let requestedLocale be ! DefaultLocale().
        requested_locale = Locale::parse_unicode_locale_id(Locale::default_locale());
    }
    VERIFY(requested_locale.has_value());

    // 4. Let noExtensionsLocale be the String value that is requestedLocale with any Unicode locale extension sequences (6.2.1) removed.
    requested_locale->remove_extension_type<Locale::LocaleExtension>();
    auto no_extensions_locale = requested_locale->to_deprecated_string();

    // 5. Let availableLocales be a List with language tags that includes the languages for which the Unicode Character Database contains language sensitive case mappings. Implementations may add additional language tags if they support case mapping for additional locales.
    // 6. Let locale be ! BestAvailableLocale(availableLocales, noExtensionsLocale).
    auto locale = Intl::best_available_locale(no_extensions_locale);

    // 7. If locale is undefined, set locale to "und".
    if (!locale.has_value())
        locale = "und"sv;

    // 8. Let codePoints be StringToCodePoints(S).

    DeprecatedString new_code_points;

    switch (target_case) {
    // 9. If targetCase is lower, then
    case TargetCase::Lower:
        // a. Let newCodePoints be a List whose elements are the result of a lowercase transformation of codePoints according to an implementation-derived algorithm using locale or the Unicode Default Case Conversion algorithm.
        new_code_points = Unicode::to_unicode_lowercase_full(string, *locale);
        break;
    // 10. Else,
    case TargetCase::Upper:
        // a. Assert: targetCase is upper.
        // b. Let newCodePoints be a List whose elements are the result of an uppercase transformation of codePoints according to an implementation-derived algorithm using locale or the Unicode Default Case Conversion algorithm.
        new_code_points = Unicode::to_unicode_uppercase_full(string, *locale);
        break;
    default:
        VERIFY_NOT_REACHED();
    }

    // 11. Return CodePointsToString(newCodePoints).
    return new_code_points;
}

// 22.1.3.25 String.prototype.toLocaleLowerCase ( [ reserved1 [ , reserved2 ] ] ), https://tc39.es/ecma262/#sec-string.prototype.tolocalelowercase
// 19.1.2 String.prototype.toLocaleLowerCase ( [ locales ] ), https://tc39.es/ecma402/#sup-string.prototype.tolocalelowercase
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::to_locale_lowercase)
{
    auto locales = vm.argument(0);

    // 1. Let O be ? RequireObjectCoercible(this value).
    // 2. Let S be ? ToString(O).
    auto string = TRY(ak_string_from(vm));

    // 3. Return ? TransformCase(S, locales, lower).
    return PrimitiveString::create(vm, TRY(transform_case(vm, string, locales, TargetCase::Lower)));
}

// 22.1.3.26 String.prototype.toLocaleUpperCase ( [ reserved1 [ , reserved2 ] ] ), https://tc39.es/ecma262/#sec-string.prototype.tolocaleuppercase
// 19.1.3 String.prototype.toLocaleUpperCase ( [ locales ] ), https://tc39.es/ecma402/#sup-string.prototype.tolocaleuppercase
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::to_locale_uppercase)
{
    auto locales = vm.argument(0);

    // 1. Let O be ? RequireObjectCoercible(this value).
    // 2. Let S be ? ToString(O).
    auto string = TRY(ak_string_from(vm));

    // 3. Return ? TransformCase(S, locales, upper).
    return PrimitiveString::create(vm, TRY(transform_case(vm, string, locales, TargetCase::Upper)));
}

// 22.1.3.27 String.prototype.toLowerCase ( ), https://tc39.es/ecma262/#sec-string.prototype.tolowercase
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::to_lowercase)
{
    auto string = TRY(ak_string_from(vm));
    auto lowercase = Unicode::to_unicode_lowercase_full(string);
    return PrimitiveString::create(vm, move(lowercase));
}

// 22.1.3.28 String.prototype.toString ( ), https://tc39.es/ecma262/#sec-string.prototype.tostring
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::to_string)
{
    return TRY(this_string_value(vm, vm.this_value()));
}

// 22.1.3.29 String.prototype.toUpperCase ( ), https://tc39.es/ecma262/#sec-string.prototype.touppercase
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::to_uppercase)
{
    auto string = TRY(ak_string_from(vm));
    auto uppercase = Unicode::to_unicode_uppercase_full(string);
    return PrimitiveString::create(vm, move(uppercase));
}

// 22.1.3.11 String.prototype.toWellFormed ( ), https://tc39.es/proposal-is-usv-string/#sec-string.prototype.towellformed
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::to_well_formed)
{
    // 1. Let O be ? RequireObjectCoercible(this value).
    // 2. Let S be ? ToString(O).
    auto string = TRY(utf16_string_from(vm));

    // 3. Let strLen be the length of S.
    auto length = string.length_in_code_units();

    // 4. Let k be 0.
    size_t k = 0;

    // 5. Let result be the empty String.
    ThrowableStringBuilder result(vm);

    // 6. Repeat, while k < strLen,
    while (k < length) {
        // a. Let cp be CodePointAt(S, k).
        auto code_point = JS::code_point_at(string.view(), k);

        // b. If cp.[[IsUnpairedSurrogate]] is true, then
        if (code_point.is_unpaired_surrogate) {
            // i. Set result to the string-concatenation of result and 0xFFFD (REPLACEMENT CHARACTER).
            TRY(result.append_code_point(0xfffd));
        }
        // c. Else,
        else {
            // i. Set result to the string-concatenation of result and UTF16EncodeCodePoint(cp.[[CodePoint]]).
            TRY(result.append_code_point(code_point.code_point));
        }

        // d. Set k to k + cp.[[CodeUnitCount]].
        k += code_point.code_unit_count;
    }

    // 7. Return result.
    return PrimitiveString::create(vm, result.build());
}

ThrowCompletionOr<DeprecatedString> trim_string(VM& vm, Value input_value, TrimMode where)
{
    // 1. Let str be ? RequireObjectCoercible(string).
    auto input_string = TRY(require_object_coercible(vm, input_value));

    // 2. Let S be ? ToString(str).
    auto string = TRY(input_string.to_string(vm));

    // 3. If where is start, let T be the String value that is a copy of S with leading white space removed.
    // 4. Else if where is end, let T be the String value that is a copy of S with trailing white space removed.
    // 5. Else,
    // a. Assert: where is start+end.
    // b. Let T be the String value that is a copy of S with both leading and trailing white space removed.
    auto trimmed_string = Utf8View(string).trim(whitespace_characters, where).as_string();

    // 6. Return T.
    return trimmed_string;
}

// 22.1.3.30 String.prototype.trim ( ), https://tc39.es/ecma262/#sec-string.prototype.trim
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::trim)
{
    return PrimitiveString::create(vm, TRY(trim_string(vm, vm.this_value(), TrimMode::Both)));
}

// 22.1.3.31 String.prototype.trimEnd ( ), https://tc39.es/ecma262/#sec-string.prototype.trimend
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::trim_end)
{
    return PrimitiveString::create(vm, TRY(trim_string(vm, vm.this_value(), TrimMode::Right)));
}

// 22.1.3.32 String.prototype.trimStart ( ), https://tc39.es/ecma262/#sec-string.prototype.trimstart
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::trim_start)
{
    return PrimitiveString::create(vm, TRY(trim_string(vm, vm.this_value(), TrimMode::Left)));
}

// 22.1.3.33 String.prototype.valueOf ( ), https://tc39.es/ecma262/#sec-string.prototype.valueof
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::value_of)
{
    return TRY(this_string_value(vm, vm.this_value()));
}

// 22.1.3.34 String.prototype [ @@iterator ] ( ), https://tc39.es/ecma262/#sec-string.prototype-@@iterator
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::symbol_iterator)
{
    auto& realm = *vm.current_realm();

    auto this_object = TRY(require_object_coercible(vm, vm.this_value()));
    auto string = TRY(this_object.to_string(vm));
    return StringIterator::create(realm, string);
}

// B.2.2.1 String.prototype.substr ( start, length ), https://tc39.es/ecma262/#sec-string.prototype.substr
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::substr)
{
    // 1. Let O be ? RequireObjectCoercible(this value).
    // 2. Let S be ? ToString(O).
    auto string = TRY(utf16_string_from(vm));

    // 3. Let size be the length of S.
    auto size = string.length_in_code_units();

    // 4. Let intStart be ? ToIntegerOrInfinity(start).
    auto int_start = TRY(vm.argument(0).to_integer_or_infinity(vm));

    // 5. If intStart is -∞, set intStart to 0.
    if (Value(int_start).is_negative_infinity())
        int_start = 0;
    // 6. Else if intStart < 0, set intStart to max(size + intStart, 0).
    else if (int_start < 0)
        int_start = max(size + int_start, 0);
    // 7. Else, set intStart to min(intStart, size).
    else
        int_start = min(int_start, size);

    // 8. If length is undefined, let intLength be size; otherwise let intLength be ? ToIntegerOrInfinity(length).
    auto length = vm.argument(1);
    auto int_length = length.is_undefined() ? size : TRY(length.to_integer_or_infinity(vm));

    // 9. Set intLength to the result of clamping intLength between 0 and size.
    int_length = clamp(int_length, 0, size);

    // 10. Let intEnd be min(intStart + intLength, size).
    auto int_end = min((i32)(int_start + int_length), size);

    if (int_start >= int_end)
        return PrimitiveString::create(vm, DeprecatedString::empty());

    // 11. Return the substring of S from intStart to intEnd.
    return PrimitiveString::create(vm, string.substring_view(int_start, int_end - int_start));
}

// B.2.2.2.1 CreateHTML ( string, tag, attribute, value ), https://tc39.es/ecma262/#sec-createhtml
static ThrowCompletionOr<Value> create_html(VM& vm, Value string, DeprecatedString const& tag, DeprecatedString const& attribute, Value value)
{
    TRY(require_object_coercible(vm, string));
    auto str = TRY(string.to_string(vm));
    ThrowableStringBuilder builder(vm);
    TRY(builder.append('<'));
    TRY(builder.append(tag));
    if (!attribute.is_empty()) {
        auto value_string = TRY(value.to_string(vm));
        TRY(builder.append(' '));
        TRY(builder.append(attribute));
        TRY(builder.append("=\""sv));
        TRY(builder.append(value_string.replace("\""sv, "&quot;"sv, ReplaceMode::All)));
        TRY(builder.append('"'));
    }
    TRY(builder.append('>'));
    TRY(builder.append(str));
    TRY(builder.append("</"sv));
    TRY(builder.append(tag));
    TRY(builder.append('>'));
    return PrimitiveString::create(vm, builder.build());
}

// B.2.2.2 String.prototype.anchor ( name ), https://tc39.es/ecma262/#sec-string.prototype.anchor
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::anchor)
{
    return create_html(vm, vm.this_value(), "a", "name", vm.argument(0));
}

// B.2.2.3 String.prototype.big ( ), https://tc39.es/ecma262/#sec-string.prototype.big
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::big)
{
    return create_html(vm, vm.this_value(), "big", DeprecatedString::empty(), Value());
}

// B.2.2.4 String.prototype.blink ( ), https://tc39.es/ecma262/#sec-string.prototype.blink
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::blink)
{
    return create_html(vm, vm.this_value(), "blink", DeprecatedString::empty(), Value());
}

// B.2.2.5 String.prototype.bold ( ), https://tc39.es/ecma262/#sec-string.prototype.bold
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::bold)
{
    return create_html(vm, vm.this_value(), "b", DeprecatedString::empty(), Value());
}

// B.2.2.6 String.prototype.fixed ( ), https://tc39.es/ecma262/#sec-string.prototype.fixed
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::fixed)
{
    return create_html(vm, vm.this_value(), "tt", DeprecatedString::empty(), Value());
}

// B.2.2.7 String.prototype.fontcolor ( color ), https://tc39.es/ecma262/#sec-string.prototype.fontcolor
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::fontcolor)
{
    return create_html(vm, vm.this_value(), "font", "color", vm.argument(0));
}

// B.2.2.8 String.prototype.fontsize ( size ), https://tc39.es/ecma262/#sec-string.prototype.fontsize
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::fontsize)
{
    return create_html(vm, vm.this_value(), "font", "size", vm.argument(0));
}

// B.2.2.9 String.prototype.italics ( ), https://tc39.es/ecma262/#sec-string.prototype.italics
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::italics)
{
    return create_html(vm, vm.this_value(), "i", DeprecatedString::empty(), Value());
}

// B.2.2.10 String.prototype.link ( url ), https://tc39.es/ecma262/#sec-string.prototype.link
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::link)
{
    return create_html(vm, vm.this_value(), "a", "href", vm.argument(0));
}

// B.2.2.11 String.prototype.small ( ), https://tc39.es/ecma262/#sec-string.prototype.small
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::small)
{
    return create_html(vm, vm.this_value(), "small", DeprecatedString::empty(), Value());
}

// B.2.2.12 String.prototype.strike ( ), https://tc39.es/ecma262/#sec-string.prototype.strike
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::strike)
{
    return create_html(vm, vm.this_value(), "strike", DeprecatedString::empty(), Value());
}

// B.2.2.13 String.prototype.sub ( ), https://tc39.es/ecma262/#sec-string.prototype.sub
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::sub)
{
    return create_html(vm, vm.this_value(), "sub", DeprecatedString::empty(), Value());
}

// B.2.2.14 String.prototype.sup ( ), https://tc39.es/ecma262/#sec-string.prototype.sup
JS_DEFINE_NATIVE_FUNCTION(StringPrototype::sup)
{
    return create_html(vm, vm.this_value(), "sup", DeprecatedString::empty(), Value());
}

}
