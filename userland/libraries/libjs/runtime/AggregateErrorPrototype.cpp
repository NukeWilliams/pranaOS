/*
 * Copyright (c) 2021, Krisna Pranav
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// includes
#include <libjs/runtime/AggregateErrorPrototype.h>
#include <libjs/runtime/GlobalObject.h>
#include <libjs/runtime/PrimitiveString.h>

namespace JS {

AggregateErrorPrototype::AggregateErrorPrototype(GlobalObject& global_object)
    : Object(*global_object.error_prototype())
{
}

void AggregateErrorPrototype::initialize(GlobalObject& global_object)
{
    auto& vm = this->vm();
    Object::initialize(global_object);
    u8 attr = Attribute::Writable | Attribute::Configurable;
    define_direct_property(vm.names.name, js_string(vm, "AggregateError"), attr);
    define_direct_property(vm.names.message, js_string(vm, ""), attr);
}

}
