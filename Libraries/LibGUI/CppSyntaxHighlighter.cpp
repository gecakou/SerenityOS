/*
 * Copyright (c) 2020, the SerenityOS developers.
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

#include <LibGUI/CppLexer.h>
#include <LibGUI/CppSyntaxHighlighter.h>
#include <LibGUI/TextEditor.h>
#include <LibGfx/Font.h>
#include <LibGfx/Palette.h>

namespace GUI {

static TextStyle style_for_token_type(Gfx::Palette palette, CppToken::Type type)
{
    switch (type) {
    case CppToken::Type::Keyword:
        return { palette.syntax_keyword(), &Gfx::Font::default_bold_fixed_width_font() };
    case CppToken::Type::KnownType:
        return { palette.syntax_type(), &Gfx::Font::default_bold_fixed_width_font() };
    case CppToken::Type::Identifier:
        return { palette.syntax_identifier() };
    case CppToken::Type::DoubleQuotedString:
    case CppToken::Type::SingleQuotedString:
    case CppToken::Type::RawString:
        return { palette.syntax_string() };
    case CppToken::Type::Integer:
    case CppToken::Type::Float:
        return { palette.syntax_number() };
    case CppToken::Type::IncludePath:
        return { palette.syntax_preprocessor_value() };
    case CppToken::Type::EscapeSequence:
        return { palette.syntax_keyword(), &Gfx::Font::default_bold_fixed_width_font() };
    case CppToken::Type::PreprocessorStatement:
    case CppToken::Type::IncludeStatement:
        return { palette.syntax_preprocessor_statement() };
    case CppToken::Type::Comment:
        return { palette.syntax_comment() };
    default:
        return { palette.base_text() };
    }
}

bool CppSyntaxHighlighter::is_identifier(void* token) const
{
    auto cpp_token = static_cast<GUI::CppToken::Type>(reinterpret_cast<size_t>(token));
    return cpp_token == GUI::CppToken::Type::Identifier;
}

bool CppSyntaxHighlighter::is_navigatable(void* token) const
{
    auto cpp_token = static_cast<GUI::CppToken::Type>(reinterpret_cast<size_t>(token));
    return cpp_token == GUI::CppToken::Type::IncludePath;
}

void CppSyntaxHighlighter::rehighlight(Gfx::Palette palette)
{
    ASSERT(m_editor);
    auto text = m_editor->text();
    CppLexer lexer(text);
    auto tokens = lexer.lex();

    Vector<GUI::TextDocumentSpan> spans;
    for (auto& token : tokens) {
#ifdef DEBUG_SYNTAX_HIGHLIGHTING
        dbg() << token.to_string() << " @ " << token.m_start.line << ":" << token.m_start.column << " - " << token.m_end.line << ":" << token.m_end.column;
#endif
        GUI::TextDocumentSpan span;
        span.range.set_start({ token.m_start.line, token.m_start.column });
        span.range.set_end({ token.m_end.line, token.m_end.column });
        auto style = style_for_token_type(palette, token.m_type);
        span.color = style.color;
        span.font = style.font;
        span.is_skippable = token.m_type == CppToken::Type::Whitespace;
        span.data = reinterpret_cast<void*>(token.m_type);
        spans.append(span);
    }
    m_editor->document().set_spans(spans);

    m_has_brace_buddies = false;
    highlight_matching_token_pair();

    m_editor->update();
}

Vector<SyntaxHighlighter::MatchingTokenPair> CppSyntaxHighlighter::matching_token_pairs() const
{
    static Vector<SyntaxHighlighter::MatchingTokenPair> pairs;
    if (pairs.is_empty()) {
        pairs.append({ reinterpret_cast<void*>(CppToken::Type::LeftCurly), reinterpret_cast<void*>(CppToken::Type::RightCurly) });
        pairs.append({ reinterpret_cast<void*>(CppToken::Type::LeftParen), reinterpret_cast<void*>(CppToken::Type::RightParen) });
        pairs.append({ reinterpret_cast<void*>(CppToken::Type::LeftBracket), reinterpret_cast<void*>(CppToken::Type::RightBracket) });
    }
    return pairs;
}

bool CppSyntaxHighlighter::token_types_equal(void* token1, void* token2) const
{
    return static_cast<GUI::CppToken::Type>(reinterpret_cast<size_t>(token1)) == static_cast<GUI::CppToken::Type>(reinterpret_cast<size_t>(token2));
}

CppSyntaxHighlighter::~CppSyntaxHighlighter()
{
}

}
