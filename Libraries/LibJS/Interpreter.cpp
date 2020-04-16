/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <AK/Badge.h>
#include <LibJS/AST.h>
#include <LibJS/Interpreter.h>
#include <LibJS/Runtime/ArrayPrototype.h>
#include <LibJS/Runtime/BooleanPrototype.h>
#include <LibJS/Runtime/DatePrototype.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/ErrorPrototype.h>
#include <LibJS/Runtime/FunctionPrototype.h>
#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/LexicalEnvironment.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/NumberPrototype.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/ObjectPrototype.h>
#include <LibJS/Runtime/Shape.h>
#include <LibJS/Runtime/StringPrototype.h>
#include <LibJS/Runtime/Value.h>

namespace JS {

Interpreter::Interpreter()
    : m_heap(*this)
{
    m_empty_object_shape = heap().allocate<Shape>();

    // These are done first since other prototypes depend on their presence.
    m_object_prototype = heap().allocate<ObjectPrototype>();
    m_function_prototype = heap().allocate<FunctionPrototype>();

#define __JS_ENUMERATE(ClassName, snake_name, PrototypeName, ConstructorName) \
    if (!m_##snake_name##_prototype)                                          \
        m_##snake_name##_prototype = heap().allocate<PrototypeName>();
    JS_ENUMERATE_BUILTIN_TYPES
#undef __JS_ENUMERATE
}

Interpreter::~Interpreter()
{
}

Value Interpreter::run(const Statement& statement, ArgumentVector arguments, ScopeType scope_type)
{
    if (statement.is_program()) {
        if (m_call_stack.is_empty()) {
            CallFrame global_call_fram;
            global_call_fram.this_value = m_global_object;
            global_call_fram.function_name = "(global execution context)";
            global_call_fram.environment = heap().allocate<LexicalEnvironment>();
            m_call_stack.append(move(global_call_fram));
        }
    }

    if (!statement.is_scope_node())
        return statement.execute(*this);

    auto& block = static_cast<const ScopeNode&>(statement);
    enter_scope(block, move(arguments), scope_type);

    m_last_value = js_undefined();
    for (auto& node : block.children()) {
        m_last_value = node.execute(*this);
        if (m_unwind_until != ScopeType::None)
            break;
    }

    bool did_return = m_unwind_until == ScopeType::Function;

    if (m_unwind_until == scope_type)
        m_unwind_until = ScopeType::None;

    exit_scope(block);

    return did_return ? m_last_value : js_undefined();
}

void Interpreter::enter_scope(const ScopeNode& scope_node, ArgumentVector arguments, ScopeType scope_type)
{
    if (scope_type == ScopeType::Function) {
        m_scope_stack.append({ scope_type, scope_node, false });
        return;
    }

    HashMap<FlyString, Variable> scope_variables_with_declaration_kind;
    scope_variables_with_declaration_kind.ensure_capacity(16);

    for (auto& declaration : scope_node.variables()) {
        for (auto& declarator : declaration.declarations()) {
            if (scope_node.is_program())
                global_object().put(declarator.id().string(), js_undefined());
            else
                scope_variables_with_declaration_kind.set(declarator.id().string(), { js_undefined(), declaration.declaration_kind() });
        }
    }

    for (auto& argument : arguments) {
        scope_variables_with_declaration_kind.set(argument.name, { argument.value, DeclarationKind::Var });
    }

    bool pushed_lexical_environment = false;

    if (!scope_variables_with_declaration_kind.is_empty()) {
        auto* block_lexical_environment = heap().allocate<LexicalEnvironment>(move(scope_variables_with_declaration_kind), current_environment());
        m_call_stack.last().environment = block_lexical_environment;
        pushed_lexical_environment = true;
    }

    m_scope_stack.append({ scope_type, scope_node, pushed_lexical_environment });
}

void Interpreter::exit_scope(const ScopeNode& scope_node)
{
    while (!m_scope_stack.is_empty()) {
        auto popped_scope = m_scope_stack.take_last();
        if (popped_scope.pushed_environment)
            m_call_stack.last().environment = m_call_stack.last().environment->parent();
        if (popped_scope.scope_node.ptr() == &scope_node)
            break;
    }

    // If we unwind all the way, just reset m_unwind_until so that future "return" doesn't break.
    if (m_scope_stack.is_empty())
        m_unwind_until = ScopeType::None;
}

void Interpreter::set_variable(const FlyString& name, Value value, bool first_assignment)
{
    for (auto* environment = current_environment(); environment; environment = environment->parent()) {
        auto possible_match = environment->get(name);
        if (possible_match.has_value()) {
            if (!first_assignment && possible_match.value().declaration_kind == DeclarationKind::Const) {
                throw_exception<TypeError>("Assignment to constant variable");
                return;
            }

            environment->set(name, { value, possible_match.value().declaration_kind });
            return;
        }
    }

    global_object().put(move(name), move(value));
}

Optional<Value> Interpreter::get_variable(const FlyString& name)
{
    for (auto* environment = current_environment(); environment; environment = environment->parent()) {
        auto possible_match = environment->get(name);
        if (possible_match.has_value())
            return possible_match.value().value;
    }
    return global_object().get(name);
}

void Interpreter::gather_roots(Badge<Heap>, HashTable<Cell*>& roots)
{
    roots.set(m_empty_object_shape);
    roots.set(m_global_object);
    roots.set(m_exception);

#define __JS_ENUMERATE(ClassName, snake_name, PrototypeName, ConstructorName) \
    roots.set(m_##snake_name##_prototype);
    JS_ENUMERATE_BUILTIN_TYPES
#undef __JS_ENUMERATE

    if (m_last_value.is_cell())
        roots.set(m_last_value.as_cell());

    for (auto& call_frame : m_call_stack) {
        if (call_frame.this_value.is_cell())
            roots.set(call_frame.this_value.as_cell());
        for (auto& argument : call_frame.arguments) {
            if (argument.is_cell())
                roots.set(argument.as_cell());
        }
        roots.set(call_frame.environment);
    }
}

Value Interpreter::call(Function* function, Value this_value, const Vector<Value>& arguments)
{
    auto& call_frame = push_call_frame();
    call_frame.function_name = function->name();
    call_frame.this_value = this_value;
    call_frame.arguments = arguments;
    call_frame.environment = function->create_environment();
    auto result = function->call(*this);
    pop_call_frame();
    return result;
}

Value Interpreter::throw_exception(Exception* exception)
{
    if (exception->value().is_object() && exception->value().as_object().is_error()) {
        auto& error = static_cast<Error&>(exception->value().as_object());
        dbg() << "Throwing JavaScript Error: " << error.name() << ", " << error.message();

        for (ssize_t i = m_call_stack.size() - 1; i >= 0; --i) {
            auto function_name = m_call_stack[i].function_name;
            if (function_name.is_empty())
                function_name = "<anonymous>";
            dbg() << "  " << function_name;
        }
    }
    m_exception = exception;
    unwind(ScopeType::Try);
    return {};
}

GlobalObject& Interpreter::global_object()
{
    return static_cast<GlobalObject&>(*m_global_object);
}

const GlobalObject& Interpreter::global_object() const
{
    return static_cast<const GlobalObject&>(*m_global_object);
}

}
