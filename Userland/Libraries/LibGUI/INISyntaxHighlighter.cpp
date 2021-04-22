/*
 * Copyright (c) 2020, Hüseyin Aslıtürk <asliturk@hotmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGUI/INILexer.h>
#include <LibGUI/INISyntaxHighlighter.h>
#include <LibGUI/TextEditor.h>
#include <LibGfx/Font.h>
#include <LibGfx/Palette.h>

namespace GUI {

static Syntax::TextStyle style_for_token_type(const Gfx::Palette& palette, IniToken::Type type)
{
    switch (type) {
    case IniToken::Type::LeftBracket:
    case IniToken::Type::RightBracket:
    case IniToken::Type::section:
        return { palette.syntax_keyword(), true };
    case IniToken::Type::Name:
        return { palette.syntax_identifier() };
    case IniToken::Type::Value:
        return { palette.syntax_string() };
    case IniToken::Type::Comment:
        return { palette.syntax_comment() };
    case IniToken::Type::Equal:
        return { palette.syntax_operator(), true };
    default:
        return { palette.base_text() };
    }
}

bool IniSyntaxHighlighter::is_identifier(void* token) const
{
    auto ini_token = static_cast<GUI::IniToken::Type>(reinterpret_cast<size_t>(token));
    return ini_token == GUI::IniToken::Type::Name;
}

void IniSyntaxHighlighter::rehighlight(const Palette& palette)
{
    auto text = m_client->get_text();
    IniLexer lexer(text);
    auto tokens = lexer.lex();

    Vector<GUI::TextDocumentSpan> spans;
    for (auto& token : tokens) {
        GUI::TextDocumentSpan span;
        span.range.set_start({ token.m_start.line, token.m_start.column });
        span.range.set_end({ token.m_end.line, token.m_end.column });
        auto style = style_for_token_type(palette, token.m_type);
        span.attributes.color = style.color;
        span.attributes.bold = style.bold;
        span.is_skippable = token.m_type == IniToken::Type::Whitespace;
        span.data = reinterpret_cast<void*>(token.m_type);
        spans.append(span);
    }
    m_client->do_set_spans(move(spans));

    m_has_brace_buddies = false;
    highlight_matching_token_pair();

    m_client->do_update();
}

Vector<IniSyntaxHighlighter::MatchingTokenPair> IniSyntaxHighlighter::matching_token_pairs() const
{
    static Vector<MatchingTokenPair> pairs;
    if (pairs.is_empty()) {
        pairs.append({ reinterpret_cast<void*>(IniToken::Type::LeftBracket), reinterpret_cast<void*>(IniToken::Type::RightBracket) });
    }
    return pairs;
}

bool IniSyntaxHighlighter::token_types_equal(void* token1, void* token2) const
{
    return static_cast<GUI::IniToken::Type>(reinterpret_cast<size_t>(token1)) == static_cast<GUI::IniToken::Type>(reinterpret_cast<size_t>(token2));
}

IniSyntaxHighlighter::~IniSyntaxHighlighter()
{
}

}
