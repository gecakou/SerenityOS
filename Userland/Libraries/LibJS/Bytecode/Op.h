/*
 * Copyright (c) 2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/FlyString.h>
#include <LibJS/Bytecode/Instruction.h>
#include <LibJS/Bytecode/Label.h>
#include <LibJS/Bytecode/Register.h>
#include <LibJS/Heap/Cell.h>
#include <LibJS/Runtime/Value.h>

namespace JS::Bytecode::Op {

class Load final : public Instruction {
public:
    Load(Register dst, Value value)
        : m_dst(dst)
        , m_value(value)
    {
    }

    virtual ~Load() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    Register m_dst;
    Value m_value;
};

class Add final : public Instruction {
public:
    Add(Register dst, Register src1, Register src2)
        : m_dst(dst)
        , m_src1(src1)
        , m_src2(src2)
    {
    }

    virtual ~Add() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    Register m_dst;
    Register m_src1;
    Register m_src2;
};

class Sub final : public Instruction {
public:
    Sub(Register dst, Register src1, Register src2)
        : m_dst(dst)
        , m_src1(src1)
        , m_src2(src2)
    {
    }

    virtual ~Sub() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    Register m_dst;
    Register m_src1;
    Register m_src2;
};

class LessThan final : public Instruction {
public:
    LessThan(Register dst, Register src1, Register src2)
        : m_dst(dst)
        , m_src1(src1)
        , m_src2(src2)
    {
    }

    virtual ~LessThan() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    Register m_dst;
    Register m_src1;
    Register m_src2;
};

class AbstractInequals final : public Instruction {
public:
    AbstractInequals(Register dst, Register src1, Register src2)
        : m_dst(dst)
        , m_src1(src1)
        , m_src2(src2)
    {
    }

    virtual ~AbstractInequals() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    Register m_dst;
    Register m_src1;
    Register m_src2;
};

class NewString final : public Instruction {
public:
    NewString(Register dst, String string)
        : m_dst(dst)
        , m_string(move(string))
    {
    }

    virtual ~NewString() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    Register m_dst;
    String m_string;
};

class NewObject final : public Instruction {
public:
    explicit NewObject(Register dst)
        : m_dst(dst)
    {
    }

    virtual ~NewObject() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    Register m_dst;
};

class SetVariable final : public Instruction {
public:
    SetVariable(FlyString identifier, Register src)
        : m_identifier(move(identifier))
        , m_src(src)
    {
    }

    virtual ~SetVariable() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    FlyString m_identifier;
    Register m_src;
};

class GetVariable final : public Instruction {
public:
    GetVariable(Register dst, FlyString identifier)
        : m_dst(dst)
        , m_identifier(move(identifier))
    {
    }

    virtual ~GetVariable() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    Register m_dst;
    FlyString m_identifier;
};

class GetById final : public Instruction {
public:
    GetById(Register dst, Register base, FlyString property)
        : m_dst(dst)
        , m_base(base)
        , m_property(move(property))
    {
    }

    virtual ~GetById() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    Register m_dst;
    Register m_base;
    FlyString m_property;
};

class PutById final : public Instruction {
public:
    PutById(Register base, FlyString property, Register src)
        : m_base(base)
        , m_property(move(property))
        , m_src(src)
    {
    }

    virtual ~PutById() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    Register m_base;
    FlyString m_property;
    Register m_src;
};

class Jump final : public Instruction {
public:
    explicit Jump(Optional<Label> target = {})
        : m_target(move(target))
    {
    }

    void set_target(Optional<Label> target) { m_target = move(target); }

    virtual ~Jump() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    Optional<Label> m_target;
};

class JumpIfFalse final : public Instruction {
public:
    explicit JumpIfFalse(Register result, Optional<Label> target = {})
        : m_result(result)
        , m_target(move(target))
    {
    }

    void set_target(Optional<Label> target) { m_target = move(target); }

    virtual ~JumpIfFalse() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    Register m_result;
    Optional<Label> m_target;
};

class JumpIfTrue final : public Instruction {
public:
    explicit JumpIfTrue(Register result, Optional<Label> target = {})
        : m_result(result)
        , m_target(move(target))
    {
    }

    void set_target(Optional<Label> target) { m_target = move(target); }

    virtual ~JumpIfTrue() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    Register m_result;
    Optional<Label> m_target;
};

class Call final : public Instruction {
public:
    Call(Register dst, Register callee, Register this_value, Vector<Register> arguments)
        : m_dst(dst)
        , m_callee(callee)
        , m_this_value(this_value)
        , m_arguments(move(arguments))
    {
    }

    virtual ~Call() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    Register m_dst;
    Register m_callee;
    Register m_this_value;
    Vector<Register> m_arguments;
};

class EnterScope final : public Instruction {
public:
    explicit EnterScope(ScopeNode const& scope_node)
        : m_scope_node(scope_node)
    {
    }

    virtual ~EnterScope() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    ScopeNode const& m_scope_node;
};

class Return final : public Instruction {
public:
    explicit Return(Optional<Register> argument)
        : m_argument(move(argument))
    {
    }

    virtual ~Return() override { }
    virtual void execute(Bytecode::Interpreter&) const override;
    virtual String to_string() const override;

private:
    Optional<Register> m_argument;
};

}
