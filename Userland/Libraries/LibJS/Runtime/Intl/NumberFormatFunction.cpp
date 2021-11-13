/*
 * Copyright (c) 2021, Tim Flynn <trflynn89@pm.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/Intl/NumberFormat.h>
#include <LibJS/Runtime/Intl/NumberFormatFunction.h>

namespace JS::Intl {

// 15.1.4 Number Format Functions
NumberFormatFunction* NumberFormatFunction::create(GlobalObject& global_object, NumberFormat& number_format)
{
    return global_object.heap().allocate<NumberFormatFunction>(global_object, number_format, *global_object.function_prototype());
}

NumberFormatFunction::NumberFormatFunction(NumberFormat& number_format, Object& prototype)
    : NativeFunction(prototype)
    , m_number_format(number_format)
{
}

void NumberFormatFunction::initialize(GlobalObject& global_object)
{
    Base::initialize(global_object);
    define_direct_property(vm().names.length, Value(1), Attribute::Configurable);
}

ThrowCompletionOr<Value> NumberFormatFunction::call()
{
    auto& global_object = this->global_object();
    auto& vm = global_object.vm();

    // 1. Let nf be F.[[NumberFormat]].
    // 2. Assert: Type(nf) is Object and nf has an [[InitializedNumberFormat]] internal slot.
    // 3. If value is not provided, let value be undefined.
    auto value = vm.argument(0);

    // 4. Let x be ? ToNumeric(value).
    value = TRY(value.to_numeric(global_object));

    // FIXME: Support BigInt number formatting.
    if (value.is_bigint())
        return vm.throw_completion<InternalError>(global_object, ErrorType::NotImplemented, "BigInt number formatting");

    // 5. Return ? FormatNumeric(nf, x).
    // Note: Our implementation of FormatNumeric does not throw.
    auto formatted = format_numeric(m_number_format, value.as_double());

    return js_string(vm, move(formatted));
}

void NumberFormatFunction::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(&m_number_format);
}

}
