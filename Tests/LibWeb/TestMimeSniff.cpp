/*
 * Copyright (c) 2023, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>

#include <LibWeb/MimeSniff/Resource.h>

TEST_CASE(compute_unknown_mime_type)
{
    HashMap<StringView, Vector<StringView>> mime_type_to_headers_map;

    mime_type_to_headers_map.set("application/octet-stream"sv, { "\x00"sv });
    mime_type_to_headers_map.set("text/html"sv, {
                                                    "\x09\x09<!DOCTYPE HTML\x20"sv,
                                                    "\x0A<HTML\x3E"sv,
                                                    "\x0C<HEAD\x20"sv,
                                                    "\x0D<SCRIPT>"sv,
                                                    "\x20<IFRAME>"sv,
                                                    "<H1>"sv,
                                                    "<DIV>"sv,
                                                    "<FONT>"sv,
                                                    "<TABLE>"sv,
                                                    "<A>"sv,
                                                    "<STYLE>"sv,
                                                    "<TITLE>"sv,
                                                    "<B>"sv,
                                                    "<BODY>"sv,
                                                    "<BR>"sv,
                                                    "<P>"sv,
                                                    "<!-->"sv,
                                                });
    mime_type_to_headers_map.set("text/xml"sv, { "<?xml"sv });
    mime_type_to_headers_map.set("application/pdf"sv, { "%PDF-"sv });
    mime_type_to_headers_map.set("application/postscript"sv, { "%!PS-Adobe-"sv });
    mime_type_to_headers_map.set("text/plain"sv, {
                                                     "\xFE\xFF\x00\x00"sv,
                                                     "\xFF\xFE\x00\x00"sv,
                                                     "\xEF\xBB\xBF\x00"sv,
                                                     "Hello world!"sv,
                                                 });

    for (auto const& mime_type_to_headers : mime_type_to_headers_map) {
        auto mime_type = mime_type_to_headers.key;

        for (auto const& header : mime_type_to_headers.value) {
            auto resource = MUST(Web::MimeSniff::Resource::create(header.bytes()));
            EXPECT_EQ(mime_type, resource.computed_mime_type().essence());
        }
    }
}
