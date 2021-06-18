/*
 * Copyright (c) 2020-2021, Linus Groh <linusg@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/TypedArrayPrototype.h>

namespace JS {

TypedArrayPrototype::TypedArrayPrototype(GlobalObject& global_object)
    : Object(*global_object.object_prototype())
{
}

void TypedArrayPrototype::initialize(GlobalObject& object)
{
    auto& vm = this->vm();
    Object::initialize(object);
    u8 attr = Attribute::Writable | Attribute::Configurable;

    define_native_accessor(vm.names.length, length_getter, nullptr, Attribute::Configurable);
    define_native_accessor(vm.names.buffer, buffer_getter, nullptr, Attribute::Configurable);
    define_native_accessor(vm.names.byteLength, byte_length_getter, nullptr, Attribute::Configurable);
    define_native_accessor(vm.names.byteOffset, byte_offset_getter, nullptr, Attribute::Configurable);
    define_native_function(vm.names.at, at, 1, attr);
    define_native_function(vm.names.every, every, 1, attr);
    define_native_function(vm.names.find, find, 1, attr);
    define_native_function(vm.names.findIndex, find_index, 1, attr);
    define_native_function(vm.names.forEach, for_each, 1, attr);
}

TypedArrayPrototype::~TypedArrayPrototype()
{
}

static TypedArrayBase* typed_array_from(VM& vm, GlobalObject& global_object)
{
    auto* this_object = vm.this_value(global_object).to_object(global_object);
    if (!this_object)
        return nullptr;
    if (!this_object->is_typed_array()) {
        vm.throw_exception<TypeError>(global_object, ErrorType::NotA, "TypedArray");
        return nullptr;
    }
    return static_cast<TypedArrayBase*>(this_object);
}

static Function* callback_from_args(GlobalObject& global_object, const String& name)
{
    auto& vm = global_object.vm();
    if (vm.argument_count() < 1) {
        vm.throw_exception<TypeError>(global_object, ErrorType::TypedArrayPrototypeOneArg, name);
        return nullptr;
    }
    auto callback = vm.argument(0);
    if (!callback.is_function()) {
        vm.throw_exception<TypeError>(global_object, ErrorType::NotAFunction, callback.to_string_without_side_effects());
        return nullptr;
    }
    return &callback.as_function();
}

static void for_each_item(VM& vm, GlobalObject& global_object, const String& name, AK::Function<IterationDecision(size_t index, Value value, Value callback_result)> callback)
{
    auto* typed_array = typed_array_from(vm, global_object);
    if (!typed_array)
        return;

    auto initial_length = typed_array->array_length();

    auto* callback_function = callback_from_args(global_object, name);
    if (!callback_function)
        return;

    auto this_value = vm.argument(1);

    for (size_t i = 0; i < initial_length; ++i) {
        auto value = typed_array->get(i);
        if (vm.exception())
            return;

        auto callback_result = vm.call(*callback_function, this_value, value, Value((i32)i), typed_array);
        if (vm.exception())
            return;

        if (callback(i, value, callback_result) == IterationDecision::Break)
            break;
    }
}

// 23.2.3.18 get %TypedArray%.prototype.length, https://tc39.es/ecma262/#sec-get-%typedarray%.prototype.length
JS_DEFINE_NATIVE_GETTER(TypedArrayPrototype::length_getter)
{
    auto typed_array = typed_array_from(vm, global_object);
    if (!typed_array)
        return {};
    auto* array_buffer = typed_array->viewed_array_buffer();
    VERIFY(array_buffer);
    if (array_buffer->is_detached())
        return Value(0);
    return Value(typed_array->array_length());
}

// 4.1 %TypedArray%.prototype.at ( index ), https://tc39.es/proposal-relative-indexing-method/#sec-%typedarray%.prototype.at
JS_DEFINE_NATIVE_FUNCTION(TypedArrayPrototype::at)
{
    auto typed_array = typed_array_from(vm, global_object);
    if (!typed_array)
        return {};
    auto length = typed_array->array_length();
    auto relative_index = vm.argument(0).to_integer_or_infinity(global_object);
    if (vm.exception())
        return {};
    if (Value(relative_index).is_infinity())
        return js_undefined();
    Checked<size_t> index { 0 };
    if (relative_index >= 0) {
        index += relative_index;
    } else {
        index += length;
        index -= -relative_index;
    }
    if (index.has_overflow() || index.value() >= length)
        return js_undefined();
    return typed_array->get(index.value());
}

// 23.2.3.7 %TypedArray%.prototype.every ( callbackfn [ , thisArg ] ), https://tc39.es/ecma262/#sec-%typedarray%.prototype.every
JS_DEFINE_NATIVE_FUNCTION(TypedArrayPrototype::every)
{
    auto result = true;
    for_each_item(vm, global_object, "every", [&](auto, auto, auto callback_result) {
        if (!callback_result.to_boolean()) {
            result = false;
            return IterationDecision::Break;
        }
        return IterationDecision::Continue;
    });
    return Value(result);
}

// 23.2.3.10 %TypedArray%.prototype.find ( predicate [ , thisArg ] ), https://tc39.es/ecma262/#sec-%typedarray%.prototype.find
JS_DEFINE_NATIVE_FUNCTION(TypedArrayPrototype::find)
{
    auto result = js_undefined();
    for_each_item(vm, global_object, "find", [&](auto, auto value, auto callback_result) {
        if (callback_result.to_boolean()) {
            result = value;
            return IterationDecision::Break;
        }
        return IterationDecision::Continue;
    });
    return result;
}

// 23.2.3.11 %TypedArray%.prototype.findIndex ( predicate [ , thisArg ] ), https://tc39.es/ecma262/#sec-%typedarray%.prototype.findindex
JS_DEFINE_NATIVE_FUNCTION(TypedArrayPrototype::find_index)
{
    auto result_index = -1;
    for_each_item(vm, global_object, "findIndex", [&](auto index, auto, auto callback_result) {
        if (callback_result.to_boolean()) {
            result_index = index;
            return IterationDecision::Break;
        }
        return IterationDecision::Continue;
    });
    return Value(result_index);
}

// 23.2.3.12 %TypedArray%.prototype.forEach ( callbackfn [ , thisArg ] ), https://tc39.es/ecma262/#sec-%typedarray%.prototype.foreach
JS_DEFINE_NATIVE_FUNCTION(TypedArrayPrototype::for_each)
{
    for_each_item(vm, global_object, "forEach", [](auto, auto, auto) {
        return IterationDecision::Continue;
    });
    return js_undefined();
}

// 23.2.3.1 get %TypedArray%.prototype.buffer, https://tc39.es/ecma262/#sec-get-%typedarray%.prototype.buffer
JS_DEFINE_NATIVE_GETTER(TypedArrayPrototype::buffer_getter)
{
    auto typed_array = typed_array_from(vm, global_object);
    if (!typed_array)
        return {};
    auto* array_buffer = typed_array->viewed_array_buffer();
    VERIFY(array_buffer);
    return Value(array_buffer);
}

// 23.2.3.2 get %TypedArray%.prototype.byteLength, https://tc39.es/ecma262/#sec-get-%typedarray%.prototype.bytelength
JS_DEFINE_NATIVE_GETTER(TypedArrayPrototype::byte_length_getter)
{
    auto typed_array = typed_array_from(vm, global_object);
    if (!typed_array)
        return {};
    auto* array_buffer = typed_array->viewed_array_buffer();
    VERIFY(array_buffer);
    if (array_buffer->is_detached())
        return Value(0);
    return Value(typed_array->byte_length());
}

// 23.2.3.3 get %TypedArray%.prototype.byteOffset, https://tc39.es/ecma262/#sec-get-%typedarray%.prototype.byteoffset
JS_DEFINE_NATIVE_GETTER(TypedArrayPrototype::byte_offset_getter)
{
    auto typed_array = typed_array_from(vm, global_object);
    if (!typed_array)
        return {};
    auto* array_buffer = typed_array->viewed_array_buffer();
    VERIFY(array_buffer);
    if (array_buffer->is_detached())
        return Value(0);
    return Value(typed_array->byte_offset());
}

}
