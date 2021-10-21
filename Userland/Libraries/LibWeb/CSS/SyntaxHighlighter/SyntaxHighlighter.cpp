/*
 * Copyright (c) 2021, Sam Atkins <atkinssj@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Debug.h>
#include <LibWeb/CSS/Parser/Tokenizer.h>
#include <LibWeb/CSS/SyntaxHighlighter/SyntaxHighlighter.h>

namespace Web::CSS {

bool SyntaxHighlighter::is_identifier(u64 token) const
{
    return static_cast<CSS::Token::Type>(token) == CSS::Token::Type::Ident;
}

bool SyntaxHighlighter::is_navigatable(u64) const
{
    return false;
}

void SyntaxHighlighter::rehighlight(Palette const& palette)
{
    dbgln_if(SYNTAX_HIGHLIGHTING_DEBUG, "(CSS::SyntaxHighlighter) starting rehighlight");
    auto text = m_client->get_text();

    Vector<GUI::TextDocumentSpan> spans;

    auto highlight = [&](auto start_line, auto start_column, auto end_line, auto end_column, Gfx::TextAttributes attributes, CSS::Token::Type type) {
        if (start_line > end_line || (start_line == end_line && start_column >= end_column)) {
            dbgln_if(SYNTAX_HIGHLIGHTING_DEBUG, "(CSS::SyntaxHighlighter) discarding ({}-{}) to ({}-{}) because it has zero or negative length", start_line, start_column, end_line, end_column);
            return;
        }
        dbgln_if(SYNTAX_HIGHLIGHTING_DEBUG, "(CSS::SyntaxHighlighter) highlighting ({}-{}) to ({}-{}) with color {}", start_line, start_column, end_line, end_column, attributes.color);
        spans.empend(
            GUI::TextRange {
                { start_line, start_column },
                { end_line, end_column },
            },
            move(attributes),
            static_cast<u64>(type),
            false);
    };

    CSS::Tokenizer tokenizer { text, "utf-8" };
    auto tokens = tokenizer.parse();
    for (auto const& token : tokens) {
        if (token.is(Token::Type::EndOfFile))
            break;

        switch (token.type()) {
        case Token::Type::Ident:
            highlight(token.start_position().line, token.start_position().column, token.end_position().line, token.end_position().column, { palette.syntax_identifier(), {} }, token.type());
            break;

        case Token::Type::String:
            highlight(token.start_position().line, token.start_position().column, token.end_position().line, token.end_position().column, { palette.syntax_string(), {} }, token.type());
            break;

        case Token::Type::Whitespace:
            // CSS doesn't produce comment tokens, they're just included as part of whitespace.
            highlight(token.start_position().line, token.start_position().column, token.end_position().line, token.end_position().column, { palette.syntax_comment(), {} }, token.type());
            break;

        case Token::Type::AtKeyword:
            highlight(token.start_position().line, token.start_position().column, token.end_position().line, token.end_position().column, { palette.syntax_keyword(), {} }, token.type());
            break;

        case Token::Type::Function:
            // Function tokens include the opening '(', so we split that into two tokens for highlighting purposes.
            highlight(token.start_position().line, token.start_position().column, token.end_position().line, token.end_position().column - 1, { palette.syntax_keyword(), {} }, token.type());
            highlight(token.end_position().line, token.end_position().column - 1, token.end_position().line, token.end_position().column, { palette.syntax_punctuation(), {} }, Token::Type::OpenParen);
            break;

        case Token::Type::Url:
            // An Url token is a `url()` function with its parameter string unquoted.
            // url
            highlight(token.start_position().line, token.start_position().column, token.start_position().line, token.start_position().column + 3, { palette.syntax_keyword(), {} }, token.type());
            // (
            highlight(token.start_position().line, token.start_position().column + 3, token.start_position().line, token.start_position().column + 4, { palette.syntax_punctuation(), {} }, Token::Type::OpenParen);
            // <string>
            highlight(token.start_position().line, token.start_position().column + 4, token.end_position().line, token.end_position().column - 1, { palette.syntax_string(), {} }, Token::Type::String);
            // )
            highlight(token.end_position().line, token.end_position().column - 1, token.end_position().line, token.end_position().column, { palette.syntax_punctuation(), {} }, Token::Type::CloseParen);
            break;

        case Token::Type::Number:
        case Token::Type::Dimension:
        case Token::Type::Percentage:
            highlight(token.start_position().line, token.start_position().column, token.end_position().line, token.end_position().column, { palette.syntax_number(), {} }, token.type());
            break;

        case Token::Type::Delim:
        case Token::Type::Colon:
        case Token::Type::Comma:
        case Token::Type::Semicolon:
        case Token::Type::OpenCurly:
        case Token::Type::OpenParen:
        case Token::Type::OpenSquare:
        case Token::Type::CloseCurly:
        case Token::Type::CloseParen:
        case Token::Type::CloseSquare:
            highlight(token.start_position().line, token.start_position().column, token.end_position().line, token.end_position().column, { palette.syntax_punctuation(), {} }, token.type());
            break;

        case Token::Type::CDO:
        case Token::Type::CDC:
            highlight(token.start_position().line, token.start_position().column, token.end_position().line, token.end_position().column, { palette.syntax_comment(), {} }, token.type());
            break;

        case Token::Type::Hash:
            // FIXME: Hash tokens can be ID selectors or colors, we don't know which without parsing properly.
            highlight(token.start_position().line, token.start_position().column, token.end_position().line, token.end_position().column, { palette.syntax_number(), {} }, token.type());
            break;

        case Token::Type::Invalid:
        case Token::Type::BadUrl:
        case Token::Type::BadString:
            // FIXME: Error highlighting color in palette?
            highlight(token.start_position().line, token.start_position().column, token.end_position().line, token.end_position().column, { Color(Color::NamedColor::Red), {}, false, true }, token.type());
            break;

        case Token::Type::EndOfFile:
        default:
            break;
        }
    }

    if constexpr (SYNTAX_HIGHLIGHTING_DEBUG) {
        dbgln("(CSS::SyntaxHighlighter) list of all spans:");
        for (auto& span : spans)
            dbgln("{}, {} - {}", span.range, span.attributes.color, span.data);
        dbgln("(CSS::SyntaxHighlighter) end of list");
    }

    m_client->do_set_spans(move(spans));
    m_has_brace_buddies = false;
    highlight_matching_token_pair();
    m_client->do_update();
}

Vector<Syntax::Highlighter::MatchingTokenPair> SyntaxHighlighter::matching_token_pairs_impl() const
{
    static Vector<Syntax::Highlighter::MatchingTokenPair> pairs;
    if (pairs.is_empty()) {
        pairs.append({ static_cast<u64>(CSS::Token::Type::OpenCurly), static_cast<u64>(CSS::Token::Type::CloseCurly) });
        pairs.append({ static_cast<u64>(CSS::Token::Type::OpenParen), static_cast<u64>(CSS::Token::Type::CloseParen) });
        pairs.append({ static_cast<u64>(CSS::Token::Type::OpenSquare), static_cast<u64>(CSS::Token::Type::CloseSquare) });
        pairs.append({ static_cast<u64>(CSS::Token::Type::CDO), static_cast<u64>(CSS::Token::Type::CDC) });
    }
    return pairs;
}

bool SyntaxHighlighter::token_types_equal(u64 token0, u64 token1) const
{
    return token0 == token1;
}

}
