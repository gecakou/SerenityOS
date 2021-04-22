/*
 * Copyright (c) 2020, Linus Groh <mail@linusgroh.de>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibJS/Runtime/NativeFunction.h>

namespace JS {

class TypedArrayConstructor : public NativeFunction {
    JS_OBJECT(TypedArrayConstructor, NativeFunction);

public:
    TypedArrayConstructor(const FlyString& name, Object& prototype);
    explicit TypedArrayConstructor(GlobalObject&);
    virtual void initialize(GlobalObject&) override;
    virtual ~TypedArrayConstructor() override;

    virtual Value call() override;
    virtual Value construct(Function& new_target) override;

private:
    virtual bool has_constructor() const override { return true; }
};

}
