/*
 * Copyright (c) 2023-2024, Matthew Olsson <mattco@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/QuickSort.h>
#include <LibJS/Runtime/Iterator.h>
#include <LibWeb/Animations/KeyframeEffect.h>
#include <LibWeb/CSS/Parser/Parser.h>
#include <LibWeb/WebIDL/ExceptionOr.h>

namespace Web::Animations {

JS_DEFINE_ALLOCATOR(KeyframeEffect);

template<typename T>
WebIDL::ExceptionOr<Variant<T, Vector<T>>> convert_value_to_maybe_list(JS::Realm& realm, JS::Value value, Function<WebIDL::ExceptionOr<T>(JS::Value)>& value_converter)
{
    auto& vm = realm.vm();

    if (TRY(value.is_array(vm))) {
        Vector<T> offsets;

        auto iterator = TRY(JS::get_iterator(vm, value, JS::IteratorHint::Sync));
        auto values = TRY(JS::iterator_to_list(vm, iterator));
        for (auto const& element : values) {
            if (element.is_undefined()) {
                offsets.append({});
            } else {
                offsets.append(TRY(value_converter(element)));
            }
        }

        return offsets;
    }

    return TRY(value_converter(value));
}

enum AllowLists {
    Yes,
    No,
};

template<AllowLists AL>
using KeyframeType = Conditional<AL == AllowLists::Yes, BasePropertyIndexedKeyframe, BaseKeyframe>;

// https://www.w3.org/TR/web-animations-1/#process-a-keyframe-like-object
template<AllowLists AL>
static WebIDL::ExceptionOr<KeyframeType<AL>> process_a_keyframe_like_object(JS::Realm& realm, JS::GCPtr<JS::Object> keyframe_input)
{
    auto& vm = realm.vm();

    Function<WebIDL::ExceptionOr<Optional<double>>(JS::Value)> to_nullable_double = [&vm](JS::Value value) -> WebIDL::ExceptionOr<Optional<double>> {
        if (value.is_undefined())
            return Optional<double> {};
        return TRY(value.to_double(vm));
    };

    Function<WebIDL::ExceptionOr<String>(JS::Value)> to_string = [&vm](JS::Value value) -> WebIDL::ExceptionOr<String> {
        return TRY(value.to_string(vm));
    };

    Function<WebIDL::ExceptionOr<Bindings::CompositeOperationOrAuto>(JS::Value)> to_composite_operation = [&vm](JS::Value value) -> WebIDL::ExceptionOr<Bindings::CompositeOperationOrAuto> {
        if (value.is_undefined())
            return Bindings::CompositeOperationOrAuto::Auto;

        auto string_value = TRY(value.to_string(vm));
        if (string_value == "replace")
            return Bindings::CompositeOperationOrAuto::Replace;
        if (string_value == "add")
            return Bindings::CompositeOperationOrAuto::Add;
        if (string_value == "accumulate")
            return Bindings::CompositeOperationOrAuto::Accumulate;
        if (string_value == "auto")
            return Bindings::CompositeOperationOrAuto::Auto;

        return WebIDL::SimpleException { WebIDL::SimpleExceptionType::TypeError, "Invalid composite value"sv };
    };

    // 1. Run the procedure to convert an ECMAScript value to a dictionary type with keyframe input as the ECMAScript
    //    value, and the dictionary type depending on the value of the allow lists flag as follows:
    //
    //    -> If allow lists is true, use the following dictionary type: <BasePropertyIndexedKeyframe>.
    //    -> Otherwise, use the following dictionary type: <BaseKeyframe>.
    //
    //    Store the result of this procedure as keyframe output.

    KeyframeType<AL> keyframe_output;
    auto offset = TRY(keyframe_input->get("offset"));
    auto easing = TRY(keyframe_input->get("easing"));
    if (easing.is_undefined())
        easing = JS::PrimitiveString::create(vm, "linear"_string);
    auto composite = TRY(keyframe_input->get("composite"));
    if (composite.is_undefined())
        composite = JS::PrimitiveString::create(vm, "auto"_string);

    if constexpr (AL == AllowLists::Yes) {
        keyframe_output.offset = TRY(convert_value_to_maybe_list(realm, offset, to_nullable_double));
        keyframe_output.composite = TRY(convert_value_to_maybe_list(realm, composite, to_composite_operation));

        auto easing_maybe_list = TRY(convert_value_to_maybe_list(realm, easing, to_string));
        easing_maybe_list.visit(
            [&](String const& value) {
                keyframe_output.easing = EasingValue { value };
            },
            [&](Vector<String> const& values) {
                Vector<EasingValue> easing_values;
                for (auto& easing_value : values)
                    easing_values.append(easing_value);
                keyframe_output.easing = move(easing_values);
            });
    } else {
        keyframe_output.offset = TRY(to_nullable_double(offset));
        keyframe_output.easing = TRY(to_string(easing));
        keyframe_output.composite = TRY(to_composite_operation(composite));
    }

    // 2. Build up a list of animatable properties as follows:
    //
    //    1. Let animatable properties be a list of property names (including shorthand properties that have longhand
    //       sub-properties that are animatable) that can be animated by the implementation.
    //    2. Convert each property name in animatable properties to the equivalent IDL attribute by applying the
    //       animation property name to IDL attribute name algorithm.

    // 3. Let input properties be the result of calling the EnumerableOwnNames operation with keyframe input as the
    //    object.

    // 4. Make up a new list animation properties that consists of all of the properties that are in both input
    //    properties and animatable properties, or which are in input properties and conform to the
    //    <custom-property-name> production.
    auto input_properties = TRY(keyframe_input->internal_own_property_keys());

    Vector<String> animation_properties;
    for (auto const& input_property : input_properties) {
        if (!input_property.is_string())
            continue;

        auto name = input_property.as_string().utf8_string();
        if (auto property = CSS::property_id_from_camel_case_string(name); property.has_value()) {
            if (CSS::is_animatable_property(property.value()))
                animation_properties.append(name);
        }
    }

    // 5. Sort animation properties in ascending order by the Unicode codepoints that define each property name.
    quick_sort(animation_properties);

    // 6. For each property name in animation properties,
    for (auto const& property_name : animation_properties) {
        // 1. Let raw value be the result of calling the [[Get]] internal method on keyframe input, with property name
        //    as the property key and keyframe input as the receiver.
        // 2. Check the completion record of raw value.
        auto raw_value = TRY(keyframe_input->get(ByteString { property_name }));

        using PropertyValuesType = Conditional<AL == AllowLists::Yes, Vector<String>, String>;
        PropertyValuesType property_values;

        // 3. Convert raw value to a DOMString or sequence of DOMStrings property values as follows:

        // -> If allow lists is true,
        if constexpr (AL == AllowLists::Yes) {
            // Let property values be the result of converting raw value to IDL type (DOMString or sequence<DOMString>)
            // using the procedures defined for converting an ECMAScript value to an IDL value [WEBIDL].
            auto intermediate_property_values = TRY(convert_value_to_maybe_list(realm, raw_value, to_string));

            // If property values is a single DOMString, replace property values with a sequence of DOMStrings with the
            // original value of property values as the only element.
            if (intermediate_property_values.has<String>())
                property_values = Vector { intermediate_property_values.get<String>() };
            else
                property_values = intermediate_property_values.get<Vector<String>>();
        }
        // -> Otherwise,
        else {
            // Let property values be the result of converting raw value to a DOMString using the procedure for
            // converting an ECMAScript value to a DOMString [WEBIDL].
            property_values = TRY(raw_value.to_string(vm));
        }

        // 4. Calculate the normalized property name as the result of applying the IDL attribute name to animation
        //    property name algorithm to property name.
        // Note: We do not need to do this, since we did not need to do the reverse step (animation property name to IDL
        //       attribute name) in the steps above.

        // 5. Add a property to keyframe output with normalized property name as the property name, and property values
        //    as the property value.
        if constexpr (AL == AllowLists::Yes) {
            keyframe_output.properties.set(property_name, property_values);
        } else {
            keyframe_output.unparsed_properties().set(property_name, property_values);
        }
    }

    return keyframe_output;
}

// https://www.w3.org/TR/web-animations-1/#compute-missing-keyframe-offsets
static void compute_missing_keyframe_offsets(Vector<BaseKeyframe>& keyframes)
{
    // 1. For each keyframe, in keyframes, let the computed keyframe offset of the keyframe be equal to its keyframe
    //    offset value.
    for (auto& keyframe : keyframes)
        keyframe.computed_offset = keyframe.offset;

    // 2. If keyframes contains more than one keyframe and the computed keyframe offset of the first keyframe in
    //    keyframes is null, set the computed keyframe offset of the first keyframe to 0.
    if (keyframes.size() > 1 && !keyframes[0].computed_offset.has_value())
        keyframes[0].computed_offset = 0.0;

    // 3. If the computed keyframe offset of the last keyframe in keyframes is null, set its computed keyframe offset
    //    to 1.
    if (!keyframes.is_empty() && !keyframes.last().computed_offset.has_value())
        keyframes.last().computed_offset = 1.0;

    // 4. For each pair of keyframes A and B where:
    //    - A appears before B in keyframes, and
    //    - A and B have a computed keyframe offset that is not null, and
    //    - all keyframes between A and B have a null computed keyframe offset,
    auto find_next_index_of_keyframe_with_computed_offset = [&](size_t starting_index) -> Optional<size_t> {
        for (size_t index = starting_index; index < keyframes.size(); index++) {
            if (keyframes[index].computed_offset.has_value())
                return index;
        }

        return {};
    };

    auto maybe_index_a = find_next_index_of_keyframe_with_computed_offset(0);
    if (!maybe_index_a.has_value())
        return;

    auto index_a = maybe_index_a.value();
    auto maybe_index_b = find_next_index_of_keyframe_with_computed_offset(index_a + 1);

    while (maybe_index_b.has_value()) {
        auto index_b = maybe_index_b.value();

        // calculate the computed keyframe offset of each keyframe between A and B as follows:
        for (size_t keyframe_index = index_a + 1; keyframe_index < index_b; keyframe_index++) {
            // 1. Let offsetk be the computed keyframe offset of a keyframe k.
            auto offset_a = keyframes[index_a].computed_offset.value();
            auto offset_b = keyframes[index_b].computed_offset.value();

            // 2. Let n be the number of keyframes between and including A and B minus 1.
            auto n = static_cast<double>(index_b - index_a);

            // 3. Let index refer to the position of keyframe in the sequence of keyframes between A and B such that the
            //    first keyframe after A has an index of 1.
            auto index = static_cast<double>(keyframe_index - index_a);

            // 4. Set the computed keyframe offset of keyframe to offsetA + (offsetB − offsetA) × index / n.
            keyframes[keyframe_index].computed_offset = (offset_a + (offset_b - offset_a)) * index / n;
        }

        index_a = index_b;
        maybe_index_b = find_next_index_of_keyframe_with_computed_offset(index_b + 1);
    }
}

// https://www.w3.org/TR/web-animations-1/#loosely-sorted-by-offset
static bool is_loosely_sorted_by_offset(Vector<BaseKeyframe> const& keyframes)
{
    // The list of keyframes for a keyframe effect must be loosely sorted by offset which means that for each keyframe
    // in the list that has a keyframe offset that is not null, the offset is greater than or equal to the offset of the
    // previous keyframe in the list with a keyframe offset that is not null, if any.

    Optional<double> last_offset;
    for (auto const& keyframe : keyframes) {
        if (!keyframe.offset.has_value())
            continue;

        if (last_offset.has_value() && keyframe.offset.value() < last_offset.value())
            return false;

        last_offset = keyframe.offset;
    }

    return true;
}

// https://www.w3.org/TR/web-animations-1/#process-a-keyframes-argument
[[maybe_unused]] static WebIDL::ExceptionOr<Vector<BaseKeyframe>> process_a_keyframes_argument(JS::Realm& realm, JS::GCPtr<JS::Object> object)
{
    auto& vm = realm.vm();

    auto parse_easing_string = [&](auto& value) -> RefPtr<CSS::StyleValue const> {
        auto maybe_parser = CSS::Parser::Parser::create(CSS::Parser::ParsingContext(realm), value);
        if (maybe_parser.is_error())
            return {};

        if (auto style_value = maybe_parser.release_value().parse_as_css_value(CSS::PropertyID::AnimationTimingFunction)) {
            if (style_value->is_easing())
                return style_value;
        }

        return {};
    };

    // 1. If object is null, return an empty sequence of keyframes.
    if (!object)
        return Vector<BaseKeyframe> {};

    // 2. Let processed keyframes be an empty sequence of keyframes.
    Vector<BaseKeyframe> processed_keyframes;

    // 3. Let method be the result of GetMethod(object, @@iterator).
    // 4. Check the completion record of method.
    auto method = TRY(JS::Value(object).get_method(vm, vm.well_known_symbol_iterator()));

    // 5. Perform the steps corresponding to the first matching condition from below,

    // -> If method is not undefined,
    if (method) {
        // 1. Let iter be GetIterator(object, method).
        // 2. Check the completion record of iter.
        auto iter = TRY(JS::get_iterator_from_method(vm, object, *method));

        // 3. Repeat:
        while (true) {
            // 1. Let next be IteratorStep(iter).
            // 2. Check the completion record of next.
            auto next = TRY(JS::iterator_step(vm, iter));

            // 3. If next is false abort this loop.
            if (!next)
                break;

            // 4. Let nextItem be IteratorValue(next).
            // 5. Check the completion record of nextItem.
            auto next_item = TRY(JS::iterator_value(vm, *next));

            // 6. If Type(nextItem) is not Undefined, Null or Object, then throw a TypeError and abort these steps.
            if (!next_item.is_nullish() && !next_item.is_object())
                return vm.throw_completion<JS::TypeError>(JS::ErrorType::NotAnObjectOrNull, next_item.to_string_without_side_effects());

            // 7. Append to processed keyframes the result of running the procedure to process a keyframe-like object
            //    passing nextItem as the keyframe input and with the allow lists flag set to false.
            processed_keyframes.append(TRY(process_a_keyframe_like_object<AllowLists::No>(realm, next_item.as_object())));
        }
    }
    // -> Otherwise,
    else {
        // FIXME: Support processing a single keyframe-like object
        TODO();
    }

    // 6. If processed keyframes is not loosely sorted by offset, throw a TypeError and abort these steps.
    if (!is_loosely_sorted_by_offset(processed_keyframes))
        return WebIDL::SimpleException { WebIDL::SimpleExceptionType::TypeError, "Keyframes are not in ascending order based on offset"sv };

    // 7. If there exist any keyframe in processed keyframes whose keyframe offset is non-null and less than zero or
    //    greater than one, throw a TypeError and abort these steps.
    for (size_t i = 0; i < processed_keyframes.size(); i++) {
        auto const& keyframe = processed_keyframes[i];
        if (!keyframe.offset.has_value())
            continue;

        auto offset = keyframe.offset.value();
        if (offset < 0.0 || offset > 1.0)
            return WebIDL::SimpleException { WebIDL::SimpleExceptionType::TypeError, MUST(String::formatted("Keyframe {} has invalid offset value {}"sv, i, offset)) };
    }

    // 8. For each frame in processed keyframes, perform the following steps:
    for (auto& keyframe : processed_keyframes) {
        // 1. For each property-value pair in frame, parse the property value using the syntax specified for that
        //    property.
        //
        //    If the property value is invalid according to the syntax for the property, discard the property-value pair.
        //    User agents that provide support for diagnosing errors in content SHOULD produce an appropriate warning
        //    highlight
        BaseKeyframe::ParsedProperties parsed_properties;
        for (auto& [property_string, value_string] : keyframe.unparsed_properties()) {
            if (auto property = CSS::property_id_from_camel_case_string(property_string); property.has_value()) {
                auto maybe_parser = CSS::Parser::Parser::create(CSS::Parser::ParsingContext(realm), value_string);
                if (maybe_parser.is_error())
                    continue;

                if (auto style_value = maybe_parser.release_value().parse_as_css_value(*property))
                    parsed_properties.set(*property, *style_value);
            }
        }
        keyframe.properties.set(move(parsed_properties));

        // 2. Let the timing function of frame be the result of parsing the "easing" property on frame using the CSS
        //    syntax defined for the easing member of the EffectTiming dictionary.
        //
        //    If parsing the "easing" property fails, throw a TypeError and abort this procedure.
        auto easing_string = keyframe.easing.get<String>();
        auto easing_value = parse_easing_string(easing_string);

        if (!easing_value)
            return WebIDL::SimpleException { WebIDL::SimpleExceptionType::TypeError, MUST(String::formatted("Invalid animation easing value: \"{}\"", easing_string)) };

        keyframe.easing.set(NonnullRefPtr<CSS::StyleValue const> { *easing_value });
    }

    // FIXME:
    // 9. Parse each of the values in unused easings using the CSS syntax defined for easing member of the EffectTiming
    //    interface, and if any of the values fail to parse, throw a TypeError and abort this procedure.

    return processed_keyframes;
}

JS::NonnullGCPtr<KeyframeEffect> KeyframeEffect::create(JS::Realm& realm)
{
    return realm.heap().allocate<KeyframeEffect>(realm, realm);
}

// https://www.w3.org/TR/web-animations-1/#dom-keyframeeffect-keyframeeffect
WebIDL::ExceptionOr<JS::NonnullGCPtr<KeyframeEffect>> KeyframeEffect::construct_impl(
    JS::Realm& realm,
    JS::Handle<DOM::Element> const& target,
    Optional<JS::Handle<JS::Object>> const& keyframes,
    Variant<double, KeyframeEffectOptions> options)
{
    auto& vm = realm.vm();

    // 1. Create a new KeyframeEffect object, effect.
    auto effect = vm.heap().allocate<KeyframeEffect>(realm, realm);

    // 2. Set the target element of effect to target.
    effect->set_target(target);

    // 3. Set the target pseudo-selector to the result corresponding to the first matching condition from below.

    //    If options is a KeyframeEffectOptions object with a pseudoElement property,
    if (options.has<KeyframeEffectOptions>()) {
        // Set the target pseudo-selector to the value of the pseudoElement property.
        //
        // When assigning this property, the error-handling defined for the pseudoElement setter on the interface is
        // applied. If the setter requires an exception to be thrown, this procedure must throw the same exception and
        // abort all further steps.
        effect->set_pseudo_element(options.get<KeyframeEffectOptions>().pseudo_element);
    }
    //     Otherwise,
    else {
        // Set the target pseudo-selector to null.
        // Note: This is the default when constructed
    }

    // 4. Let timing input be the result corresponding to the first matching condition from below.
    KeyframeEffectOptions timing_input;

    //     If options is a KeyframeEffectOptions object,
    if (options.has<KeyframeEffectOptions>()) {
        // Let timing input be options.
        timing_input = options.get<KeyframeEffectOptions>();
    }
    //     Otherwise (if options is a double),
    else {
        // Let timing input be a new EffectTiming object with all members set to their default values and duration set
        // to options.
        timing_input.duration = options.get<double>();
    }

    // 5. Call the procedure to update the timing properties of an animation effect of effect from timing input.
    //    If that procedure causes an exception to be thrown, propagate the exception and abort this procedure.
    TRY(effect->update_timing(timing_input.to_optional_effect_timing()));

    // 6. If options is a KeyframeEffectOptions object, assign the composite property of effect to the corresponding
    //    value from options.
    //
    //    When assigning this property, the error-handling defined for the corresponding setter on the KeyframeEffect
    //    interface is applied. If the setter requires an exception to be thrown for the value specified by options,
    //    this procedure must throw the same exception and abort all further steps.
    if (options.has<KeyframeEffectOptions>())
        effect->set_composite(options.get<KeyframeEffectOptions>().composite);

    // 7. Initialize the set of keyframes by performing the procedure defined for setKeyframes() passing keyframes as
    //    the input.
    TRY(effect->set_keyframes(keyframes));

    return effect;
}

WebIDL::ExceptionOr<JS::NonnullGCPtr<KeyframeEffect>> KeyframeEffect::construct_impl(JS::Realm& realm, JS::NonnullGCPtr<KeyframeEffect> source)
{
    auto& vm = realm.vm();

    // 1. Create a new KeyframeEffect object, effect.
    auto effect = vm.heap().allocate<KeyframeEffect>(realm, realm);

    // 2. Set the following properties of effect using the corresponding values of source:

    //   - effect target,
    effect->m_target_element = source->target();

    // FIXME:
    //   - keyframes,

    //   - composite operation, and
    effect->set_composite(source->composite());

    //   - all specified timing properties:

    //     - start delay,
    effect->m_start_delay = source->m_start_delay;

    //     - end delay,
    effect->m_end_delay = source->m_end_delay;

    //     - fill mode,
    effect->m_fill_mode = source->m_fill_mode;

    //     - iteration start,
    effect->m_iteration_start = source->m_iteration_start;

    //     - iteration count,
    effect->m_iteration_count = source->m_iteration_count;

    //     - iteration duration,
    effect->m_iteration_duration = source->m_iteration_duration;

    //     - playback direction, and
    effect->m_playback_direction = source->m_playback_direction;

    //     - timing function.
    effect->m_easing_function = source->m_easing_function;

    return effect;
}

void KeyframeEffect::set_pseudo_element(Optional<String> pseudo_element)
{
    // On setting, sets the target pseudo-selector of the animation effect to the provided value after applying the
    // following exceptions:

    // FIXME:
    // - If the provided value is not null and is an invalid <pseudo-element-selector>, the user agent must throw a
    //   DOMException with error name SyntaxError and leave the target pseudo-selector of this animation effect
    //   unchanged.

    // - If one of the legacy Selectors Level 2 single-colon selectors (':before', ':after', ':first-letter', or
    //   ':first-line') is specified, the target pseudo-selector must be set to the equivalent two-colon selector
    //   (e.g. '::before').
    if (pseudo_element.has_value()) {
        auto value = pseudo_element.value();

        if (value == ":before" || value == ":after" || value == ":first-letter" || value == ":first-line") {
            m_target_pseudo_selector = MUST(String::formatted(":{}", value));
            return;
        }
    }

    m_target_pseudo_selector = pseudo_element;
}

// https://www.w3.org/TR/web-animations-1/#dom-keyframeeffect-getkeyframes
WebIDL::ExceptionOr<Vector<JS::Object*>> KeyframeEffect::get_keyframes() const
{
    // FIXME: Implement this
    return Vector<JS::Object*> {};
}

// https://www.w3.org/TR/web-animations-1/#dom-keyframeeffect-setkeyframes
WebIDL::ExceptionOr<void> KeyframeEffect::set_keyframes(Optional<JS::Handle<JS::Object>> const&)
{
    // FIXME: Implement this
    return {};
}

KeyframeEffect::KeyframeEffect(JS::Realm& realm)
    : AnimationEffect(realm)
{
}

void KeyframeEffect::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    set_prototype(&Bindings::ensure_web_prototype<Bindings::KeyframeEffectPrototype>(realm, "KeyframeEffect"_fly_string));
}

void KeyframeEffect::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(m_target_element);
}

}
