/*
 * Copyright (c) 2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Debug.h>
#include <AK/SourceLocation.h>
#include <AK/Utf32View.h>
#include <LibTextCodec/Decoder.h>
#include <LibWeb/DOM/Comment.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/DocumentType.h>
#include <LibWeb/DOM/ElementFactory.h>
#include <LibWeb/DOM/Event.h>
#include <LibWeb/DOM/Text.h>
#include <LibWeb/DOM/Window.h>
#include <LibWeb/HTML/EventNames.h>
#include <LibWeb/HTML/HTMLFormElement.h>
#include <LibWeb/HTML/HTMLHeadElement.h>
#include <LibWeb/HTML/HTMLScriptElement.h>
#include <LibWeb/HTML/HTMLTableElement.h>
#include <LibWeb/HTML/HTMLTemplateElement.h>
#include <LibWeb/HTML/Parser/HTMLDocumentParser.h>
#include <LibWeb/HTML/Parser/HTMLEncodingDetection.h>
#include <LibWeb/HTML/Parser/HTMLToken.h>
#include <LibWeb/Namespace.h>
#include <LibWeb/SVG/TagNames.h>

namespace Web::HTML {

static inline void log_parse_error(const SourceLocation& location = SourceLocation::current())
{
    dbgln("Parse error! {}", location);
}

static Vector<FlyString> s_quirks_public_ids = {
    "+//Silmaril//dtd html Pro v0r11 19970101//",
    "-//AS//DTD HTML 3.0 asWedit + extensions//",
    "-//AdvaSoft Ltd//DTD HTML 3.0 asWedit + extensions//",
    "-//IETF//DTD HTML 2.0 Level 1//",
    "-//IETF//DTD HTML 2.0 Level 2//",
    "-//IETF//DTD HTML 2.0 Strict Level 1//",
    "-//IETF//DTD HTML 2.0 Strict Level 2//",
    "-//IETF//DTD HTML 2.0 Strict//",
    "-//IETF//DTD HTML 2.0//",
    "-//IETF//DTD HTML 2.1E//",
    "-//IETF//DTD HTML 3.0//",
    "-//IETF//DTD HTML 3.2 Final//",
    "-//IETF//DTD HTML 3.2//",
    "-//IETF//DTD HTML 3//",
    "-//IETF//DTD HTML Level 0//",
    "-//IETF//DTD HTML Level 1//",
    "-//IETF//DTD HTML Level 2//",
    "-//IETF//DTD HTML Level 3//",
    "-//IETF//DTD HTML Strict Level 0//",
    "-//IETF//DTD HTML Strict Level 1//",
    "-//IETF//DTD HTML Strict Level 2//",
    "-//IETF//DTD HTML Strict Level 3//",
    "-//IETF//DTD HTML Strict//",
    "-//IETF//DTD HTML//",
    "-//Metrius//DTD Metrius Presentational//",
    "-//Microsoft//DTD Internet Explorer 2.0 HTML Strict//",
    "-//Microsoft//DTD Internet Explorer 2.0 HTML//",
    "-//Microsoft//DTD Internet Explorer 2.0 Tables//",
    "-//Microsoft//DTD Internet Explorer 3.0 HTML Strict//",
    "-//Microsoft//DTD Internet Explorer 3.0 HTML//",
    "-//Microsoft//DTD Internet Explorer 3.0 Tables//",
    "-//Netscape Comm. Corp.//DTD HTML//",
    "-//Netscape Comm. Corp.//DTD Strict HTML//",
    "-//O'Reilly and Associates//DTD HTML 2.0//",
    "-//O'Reilly and Associates//DTD HTML Extended 1.0//",
    "-//O'Reilly and Associates//DTD HTML Extended Relaxed 1.0//",
    "-//SQ//DTD HTML 2.0 HoTMetaL + extensions//",
    "-//SoftQuad Software//DTD HoTMetaL PRO 6.0::19990601::extensions to HTML 4.0//",
    "-//SoftQuad//DTD HoTMetaL PRO 4.0::19971010::extensions to HTML 4.0//",
    "-//Spyglass//DTD HTML 2.0 Extended//",
    "-//Sun Microsystems Corp.//DTD HotJava HTML//",
    "-//Sun Microsystems Corp.//DTD HotJava Strict HTML//",
    "-//W3C//DTD HTML 3 1995-03-24//",
    "-//W3C//DTD HTML 3.2 Draft//",
    "-//W3C//DTD HTML 3.2 Final//",
    "-//W3C//DTD HTML 3.2//",
    "-//W3C//DTD HTML 3.2S Draft//",
    "-//W3C//DTD HTML 4.0 Frameset//",
    "-//W3C//DTD HTML 4.0 Transitional//",
    "-//W3C//DTD HTML Experimental 19960712//",
    "-//W3C//DTD HTML Experimental 970421//",
    "-//W3C//DTD W3 HTML//",
    "-//W3O//DTD W3 HTML 3.0//",
    "-//WebTechs//DTD Mozilla HTML 2.0//",
    "-//WebTechs//DTD Mozilla HTML//"
};

RefPtr<DOM::Document> parse_html_document(const StringView& data, const URL& url, const String& encoding)
{
    auto document = DOM::Document::create(url);
    HTMLDocumentParser parser(document, data, encoding);
    parser.run(url);
    return document;
}

HTMLDocumentParser::HTMLDocumentParser(DOM::Document& document, const StringView& input, const String& encoding)
    : m_tokenizer(input, encoding)
    , m_document(document)
{
    m_document->set_should_invalidate_styles_on_attribute_changes(false);
    auto standardized_encoding = TextCodec::get_standardized_encoding(encoding);
    VERIFY(standardized_encoding.has_value());
    m_document->set_encoding(standardized_encoding.value());
}

HTMLDocumentParser::~HTMLDocumentParser()
{
    m_document->set_should_invalidate_styles_on_attribute_changes(true);
}

void HTMLDocumentParser::run(const URL& url)
{
    m_document->set_url(url);
    m_document->set_source(m_tokenizer.source());

    for (;;) {
        auto optional_token = m_tokenizer.next_token();
        if (!optional_token.has_value())
            break;
        auto& token = optional_token.value();

        dbgln_if(PARSER_DEBUG, "[{}] {}", insertion_mode_name(), token.to_string());

        // FIXME: If the adjusted current node is a MathML text integration point and the token is a start tag whose tag name is neither "mglyph" nor "malignmark"
        // FIXME: If the adjusted current node is a MathML text integration point and the token is a character token
        // FIXME: If the adjusted current node is a MathML annotation-xml element and the token is a start tag whose tag name is "svg"
        // FIXME: If the adjusted current node is an HTML integration point and the token is a start tag
        // FIXME: If the adjusted current node is an HTML integration point and the token is a character token
        if (m_stack_of_open_elements.is_empty()
            || adjusted_current_node().namespace_() == Namespace::HTML
            || token.is_end_of_file()) {
            process_using_the_rules_for(m_insertion_mode, token);
        } else {
            process_using_the_rules_for_foreign_content(token);
        }

        if (m_stop_parsing) {
            dbgln_if(PARSER_DEBUG, "Stop parsing{}! :^)", m_parsing_fragment ? " fragment" : "");
            break;
        }
    }

    flush_character_insertions();

    // "The end"

    m_document->set_ready_state("interactive");

    auto scripts_to_execute_when_parsing_has_finished = m_document->take_scripts_to_execute_when_parsing_has_finished({});
    for (auto& script : scripts_to_execute_when_parsing_has_finished) {
        // FIXME: Spin the event loop until the script is ready to be parser executed and there's no style sheets blocking scripts.
        script.execute_script();
    }

    auto content_loaded_event = DOM::Event::create(HTML::EventNames::DOMContentLoaded);
    content_loaded_event->set_bubbles(true);
    m_document->dispatch_event(content_loaded_event);

    // FIXME: The document parser shouldn't execute these, it should just spin the event loop until the list becomes empty.
    // FIXME: Once the set has been added, also spin the event loop until the set becomes empty.
    auto scripts_to_execute_as_soon_as_possible = m_document->take_scripts_to_execute_as_soon_as_possible({});
    for (auto& script : scripts_to_execute_as_soon_as_possible) {
        script.execute_script();
    }

    // FIXME: Spin the event loop until there is nothing that delays the load event in the Document.

    m_document->set_ready_state("complete");
    m_document->window().dispatch_event(DOM::Event::create(HTML::EventNames::load));

    m_document->set_ready_for_post_load_tasks(true);
    m_document->completely_finish_loading();
}

void HTMLDocumentParser::process_using_the_rules_for(InsertionMode mode, HTMLToken& token)
{
    switch (mode) {
    case InsertionMode::Initial:
        handle_initial(token);
        break;
    case InsertionMode::BeforeHTML:
        handle_before_html(token);
        break;
    case InsertionMode::BeforeHead:
        handle_before_head(token);
        break;
    case InsertionMode::InHead:
        handle_in_head(token);
        break;
    case InsertionMode::InHeadNoscript:
        handle_in_head_noscript(token);
        break;
    case InsertionMode::AfterHead:
        handle_after_head(token);
        break;
    case InsertionMode::InBody:
        handle_in_body(token);
        break;
    case InsertionMode::AfterBody:
        handle_after_body(token);
        break;
    case InsertionMode::AfterAfterBody:
        handle_after_after_body(token);
        break;
    case InsertionMode::Text:
        handle_text(token);
        break;
    case InsertionMode::InTable:
        handle_in_table(token);
        break;
    case InsertionMode::InTableBody:
        handle_in_table_body(token);
        break;
    case InsertionMode::InRow:
        handle_in_row(token);
        break;
    case InsertionMode::InCell:
        handle_in_cell(token);
        break;
    case InsertionMode::InTableText:
        handle_in_table_text(token);
        break;
    case InsertionMode::InSelectInTable:
        handle_in_select_in_table(token);
        break;
    case InsertionMode::InSelect:
        handle_in_select(token);
        break;
    case InsertionMode::InCaption:
        handle_in_caption(token);
        break;
    case InsertionMode::InColumnGroup:
        handle_in_column_group(token);
        break;
    case InsertionMode::InTemplate:
        handle_in_template(token);
        break;
    case InsertionMode::InFrameset:
        handle_in_frameset(token);
        break;
    case InsertionMode::AfterFrameset:
        handle_after_frameset(token);
        break;
    case InsertionMode::AfterAfterFrameset:
        handle_after_after_frameset(token);
        break;
    default:
        VERIFY_NOT_REACHED();
    }
}

DOM::QuirksMode HTMLDocumentParser::which_quirks_mode(const HTMLToken& doctype_token) const
{
    if (doctype_token.doctype_data().force_quirks)
        return DOM::QuirksMode::Yes;

    // NOTE: The tokenizer puts the name into lower case for us.
    if (doctype_token.doctype_data().name != "html")
        return DOM::QuirksMode::Yes;

    auto const& public_identifier = doctype_token.doctype_data().public_identifier;
    auto const& system_identifier = doctype_token.doctype_data().system_identifier;

    if (public_identifier.equals_ignoring_case("-//W3O//DTD W3 HTML Strict 3.0//EN//"))
        return DOM::QuirksMode::Yes;

    if (public_identifier.equals_ignoring_case("-/W3C/DTD HTML 4.0 Transitional/EN"))
        return DOM::QuirksMode::Yes;

    if (public_identifier.equals_ignoring_case("HTML"))
        return DOM::QuirksMode::Yes;

    if (system_identifier.equals_ignoring_case("http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd"))
        return DOM::QuirksMode::Yes;

    for (auto& public_id : s_quirks_public_ids) {
        if (public_identifier.starts_with(public_id, CaseSensitivity::CaseInsensitive))
            return DOM::QuirksMode::Yes;
    }

    if (doctype_token.doctype_data().missing_system_identifier) {
        if (public_identifier.starts_with("-//W3C//DTD HTML 4.01 Frameset//", CaseSensitivity::CaseInsensitive))
            return DOM::QuirksMode::Yes;

        if (public_identifier.starts_with("-//W3C//DTD HTML 4.01 Transitional//", CaseSensitivity::CaseInsensitive))
            return DOM::QuirksMode::Yes;
    }

    if (public_identifier.starts_with("-//W3C//DTD XHTML 1.0 Frameset//", CaseSensitivity::CaseInsensitive))
        return DOM::QuirksMode::Limited;

    if (public_identifier.starts_with("-//W3C//DTD XHTML 1.0 Transitional//", CaseSensitivity::CaseInsensitive))
        return DOM::QuirksMode::Limited;

    if (!doctype_token.doctype_data().missing_system_identifier) {
        if (public_identifier.starts_with("-//W3C//DTD HTML 4.01 Frameset//", CaseSensitivity::CaseInsensitive))
            return DOM::QuirksMode::Limited;

        if (public_identifier.starts_with("-//W3C//DTD HTML 4.01 Transitional//", CaseSensitivity::CaseInsensitive))
            return DOM::QuirksMode::Limited;
    }

    return DOM::QuirksMode::No;
}

void HTMLDocumentParser::handle_initial(HTMLToken& token)
{
    if (token.is_character() && token.is_parser_whitespace()) {
        return;
    }

    if (token.is_comment()) {
        auto comment = adopt_ref(*new DOM::Comment(document(), token.comment()));
        document().append_child(move(comment));
        return;
    }

    if (token.is_doctype()) {
        auto doctype = adopt_ref(*new DOM::DocumentType(document()));
        doctype->set_name(token.doctype_data().name);
        doctype->set_public_id(token.doctype_data().public_identifier);
        doctype->set_system_id(token.doctype_data().system_identifier);
        document().append_child(move(doctype));
        document().set_quirks_mode(which_quirks_mode(token));
        m_insertion_mode = InsertionMode::BeforeHTML;
        return;
    }

    log_parse_error();
    document().set_quirks_mode(DOM::QuirksMode::Yes);
    m_insertion_mode = InsertionMode::BeforeHTML;
    process_using_the_rules_for(InsertionMode::BeforeHTML, token);
}

void HTMLDocumentParser::handle_before_html(HTMLToken& token)
{
    if (token.is_doctype()) {
        log_parse_error();
        return;
    }

    if (token.is_comment()) {
        auto comment = adopt_ref(*new DOM::Comment(document(), token.comment()));
        document().append_child(move(comment));
        return;
    }

    if (token.is_character() && token.is_parser_whitespace()) {
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::html) {
        auto element = create_element_for(token, Namespace::HTML);
        document().append_child(element);
        m_stack_of_open_elements.push(move(element));
        m_insertion_mode = InsertionMode::BeforeHead;
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::head, HTML::TagNames::body, HTML::TagNames::html, HTML::TagNames::br)) {
        goto AnythingElse;
    }

    if (token.is_end_tag()) {
        log_parse_error();
        return;
    }

AnythingElse:
    auto element = create_element(document(), HTML::TagNames::html, Namespace::HTML);
    document().append_child(element);
    m_stack_of_open_elements.push(element);
    // FIXME: If the Document is being loaded as part of navigation of a browsing context, then: run the application cache selection algorithm with no manifest, passing it the Document object.
    m_insertion_mode = InsertionMode::BeforeHead;
    process_using_the_rules_for(InsertionMode::BeforeHead, token);
    return;
}

DOM::Element& HTMLDocumentParser::current_node()
{
    return m_stack_of_open_elements.current_node();
}

DOM::Element& HTMLDocumentParser::adjusted_current_node()
{
    if (m_parsing_fragment && m_stack_of_open_elements.elements().size() == 1)
        return *m_context_element;

    return current_node();
}

DOM::Element& HTMLDocumentParser::node_before_current_node()
{
    return m_stack_of_open_elements.elements().at(m_stack_of_open_elements.elements().size() - 2);
}

HTMLDocumentParser::AdjustedInsertionLocation HTMLDocumentParser::find_appropriate_place_for_inserting_node()
{
    auto& target = current_node();
    HTMLDocumentParser::AdjustedInsertionLocation adjusted_insertion_location;

    if (m_foster_parenting && target.local_name().is_one_of(HTML::TagNames::table, HTML::TagNames::tbody, HTML::TagNames::tfoot, HTML::TagNames::thead, HTML::TagNames::tr)) {
        auto last_template = m_stack_of_open_elements.last_element_with_tag_name(HTML::TagNames::template_);
        auto last_table = m_stack_of_open_elements.last_element_with_tag_name(HTML::TagNames::table);
        if (last_template.element && (!last_table.element || last_template.index > last_table.index)) {
            // This returns the template content, so no need to check the parent is a template.
            return { verify_cast<HTMLTemplateElement>(last_template.element)->content(), nullptr };
        }
        if (!last_table.element) {
            VERIFY(m_parsing_fragment);
            // Guaranteed not to be a template element (it will be the html element),
            // so no need to check the parent is a template.
            return { m_stack_of_open_elements.elements().first(), nullptr };
        }
        if (last_table.element->parent_node())
            adjusted_insertion_location = { last_table.element->parent_node(), last_table.element };
        else
            adjusted_insertion_location = { m_stack_of_open_elements.element_before(*last_table.element), nullptr };
    } else {
        adjusted_insertion_location = { target, nullptr };
    }

    if (is<HTMLTemplateElement>(*adjusted_insertion_location.parent))
        return { verify_cast<HTMLTemplateElement>(*adjusted_insertion_location.parent).content(), nullptr };

    return adjusted_insertion_location;
}

NonnullRefPtr<DOM::Element> HTMLDocumentParser::create_element_for(const HTMLToken& token, const FlyString& namespace_)
{
    auto element = create_element(document(), token.tag_name(), namespace_);
    token.for_each_attribute([&](auto& attribute) {
        element->set_attribute(attribute.local_name, attribute.value);
        return IterationDecision::Continue;
    });
    return element;
}

// https://html.spec.whatwg.org/multipage/parsing.html#insert-a-foreign-element
NonnullRefPtr<DOM::Element> HTMLDocumentParser::insert_foreign_element(const HTMLToken& token, const FlyString& namespace_)
{
    auto adjusted_insertion_location = find_appropriate_place_for_inserting_node();

    // FIXME: Pass in adjusted_insertion_location.parent as the intended parent.
    auto element = create_element_for(token, namespace_);

    auto pre_insertion_validity = adjusted_insertion_location.parent->ensure_pre_insertion_validity(element, adjusted_insertion_location.insert_before_sibling);

    // NOTE: If it's not possible to insert the element at the adjusted insertion location, the element is simply dropped.
    if (!pre_insertion_validity.is_exception()) {
        if (!m_parsing_fragment) {
            // FIXME: push a new element queue onto element's relevant agent's custom element reactions stack.
        }

        adjusted_insertion_location.parent->insert_before(element, adjusted_insertion_location.insert_before_sibling);

        if (!m_parsing_fragment) {
            // FIXME: pop the element queue from element's relevant agent's custom element reactions stack, and invoke custom element reactions in that queue.
        }
    }

    m_stack_of_open_elements.push(element);
    return element;
}

NonnullRefPtr<DOM::Element> HTMLDocumentParser::insert_html_element(const HTMLToken& token)
{
    return insert_foreign_element(token, Namespace::HTML);
}

void HTMLDocumentParser::handle_before_head(HTMLToken& token)
{
    if (token.is_character() && token.is_parser_whitespace()) {
        return;
    }

    if (token.is_comment()) {
        insert_comment(token);
        return;
    }

    if (token.is_doctype()) {
        log_parse_error();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::html) {
        process_using_the_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::head) {
        auto element = insert_html_element(token);
        m_head_element = verify_cast<HTMLHeadElement>(*element);
        m_insertion_mode = InsertionMode::InHead;
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::head, HTML::TagNames::body, HTML::TagNames::html, HTML::TagNames::br)) {
        goto AnythingElse;
    }

    if (token.is_end_tag()) {
        log_parse_error();
        return;
    }

AnythingElse:
    m_head_element = verify_cast<HTMLHeadElement>(*insert_html_element(HTMLToken::make_start_tag(HTML::TagNames::head)));
    m_insertion_mode = InsertionMode::InHead;
    process_using_the_rules_for(InsertionMode::InHead, token);
    return;
}

void HTMLDocumentParser::insert_comment(HTMLToken& token)
{
    auto adjusted_insertion_location = find_appropriate_place_for_inserting_node();
    adjusted_insertion_location.parent->insert_before(adopt_ref(*new DOM::Comment(document(), token.comment())), adjusted_insertion_location.insert_before_sibling);
}

void HTMLDocumentParser::handle_in_head(HTMLToken& token)
{
    if (token.is_parser_whitespace()) {
        insert_character(token.code_point());
        return;
    }

    if (token.is_comment()) {
        insert_comment(token);
        return;
    }

    if (token.is_doctype()) {
        log_parse_error();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::html) {
        process_using_the_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::base, HTML::TagNames::basefont, HTML::TagNames::bgsound, HTML::TagNames::link)) {
        insert_html_element(token);
        m_stack_of_open_elements.pop();
        token.acknowledge_self_closing_flag_if_set();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::meta) {
        auto element = insert_html_element(token);
        m_stack_of_open_elements.pop();
        token.acknowledge_self_closing_flag_if_set();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::title) {
        insert_html_element(token);
        m_tokenizer.switch_to({}, HTMLTokenizer::State::RCDATA);
        m_original_insertion_mode = m_insertion_mode;
        m_insertion_mode = InsertionMode::Text;
        return;
    }

    if (token.is_start_tag() && ((token.tag_name() == HTML::TagNames::noscript && m_scripting_enabled) || token.tag_name() == HTML::TagNames::noframes || token.tag_name() == HTML::TagNames::style)) {
        parse_generic_raw_text_element(token);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::noscript && !m_scripting_enabled) {
        insert_html_element(token);
        m_insertion_mode = InsertionMode::InHeadNoscript;
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::script) {
        auto adjusted_insertion_location = find_appropriate_place_for_inserting_node();
        auto element = create_element_for(token, Namespace::HTML);
        auto& script_element = verify_cast<HTMLScriptElement>(*element);
        script_element.set_parser_document({}, document());
        script_element.set_non_blocking({}, false);

        if (m_parsing_fragment) {
            script_element.set_already_started({}, true);
        }

        if (m_invoked_via_document_write) {
            TODO();
        }

        adjusted_insertion_location.parent->insert_before(element, adjusted_insertion_location.insert_before_sibling, false);
        m_stack_of_open_elements.push(element);
        m_tokenizer.switch_to({}, HTMLTokenizer::State::ScriptData);
        m_original_insertion_mode = m_insertion_mode;
        m_insertion_mode = InsertionMode::Text;
        return;
    }
    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::head) {
        m_stack_of_open_elements.pop();
        m_insertion_mode = InsertionMode::AfterHead;
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::body, HTML::TagNames::html, HTML::TagNames::br)) {
        goto AnythingElse;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::template_) {
        insert_html_element(token);
        m_list_of_active_formatting_elements.add_marker();
        m_frameset_ok = false;
        m_insertion_mode = InsertionMode::InTemplate;
        m_stack_of_template_insertion_modes.append(InsertionMode::InTemplate);
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::template_) {
        if (!m_stack_of_open_elements.contains(HTML::TagNames::template_)) {
            log_parse_error();
            return;
        }

        generate_all_implied_end_tags_thoroughly();

        if (current_node().local_name() != HTML::TagNames::template_)
            log_parse_error();

        m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::template_);
        m_list_of_active_formatting_elements.clear_up_to_the_last_marker();
        m_stack_of_template_insertion_modes.take_last();
        reset_the_insertion_mode_appropriately();
        return;
    }

    if ((token.is_start_tag() && token.tag_name() == HTML::TagNames::head) || token.is_end_tag()) {
        log_parse_error();
        return;
    }

AnythingElse:
    m_stack_of_open_elements.pop();
    m_insertion_mode = InsertionMode::AfterHead;
    process_using_the_rules_for(m_insertion_mode, token);
}

void HTMLDocumentParser::handle_in_head_noscript(HTMLToken& token)
{
    if (token.is_doctype()) {
        log_parse_error();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::html) {
        process_using_the_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::noscript) {
        m_stack_of_open_elements.pop();
        m_insertion_mode = InsertionMode::InHead;
        return;
    }

    if (token.is_parser_whitespace() || token.is_comment() || (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::basefont, HTML::TagNames::bgsound, HTML::TagNames::link, HTML::TagNames::meta, HTML::TagNames::noframes, HTML::TagNames::style))) {
        process_using_the_rules_for(InsertionMode::InHead, token);
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::br) {
        goto AnythingElse;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::head, HTML::TagNames::noscript)) {
        log_parse_error();
        return;
    }

AnythingElse:
    log_parse_error();
    m_stack_of_open_elements.pop();
    m_insertion_mode = InsertionMode::InHead;
    process_using_the_rules_for(m_insertion_mode, token);
}

void HTMLDocumentParser::parse_generic_raw_text_element(HTMLToken& token)
{
    insert_html_element(token);
    m_tokenizer.switch_to({}, HTMLTokenizer::State::RAWTEXT);
    m_original_insertion_mode = m_insertion_mode;
    m_insertion_mode = InsertionMode::Text;
}

DOM::Text* HTMLDocumentParser::find_character_insertion_node()
{
    auto adjusted_insertion_location = find_appropriate_place_for_inserting_node();
    if (adjusted_insertion_location.insert_before_sibling) {
        TODO();
    }
    if (adjusted_insertion_location.parent->is_document())
        return nullptr;
    if (adjusted_insertion_location.parent->last_child() && adjusted_insertion_location.parent->last_child()->is_text())
        return verify_cast<DOM::Text>(adjusted_insertion_location.parent->last_child());
    auto new_text_node = adopt_ref(*new DOM::Text(document(), ""));
    adjusted_insertion_location.parent->append_child(new_text_node);
    return new_text_node;
}

void HTMLDocumentParser::flush_character_insertions()
{
    if (m_character_insertion_builder.is_empty())
        return;
    m_character_insertion_node->set_data(m_character_insertion_builder.to_string());
    m_character_insertion_node->parent()->children_changed();
    m_character_insertion_builder.clear();
}

void HTMLDocumentParser::insert_character(u32 data)
{
    auto node = find_character_insertion_node();
    if (node == m_character_insertion_node) {
        m_character_insertion_builder.append(Utf32View { &data, 1 });
        return;
    }
    if (!m_character_insertion_node) {
        m_character_insertion_node = node;
        m_character_insertion_builder.append(Utf32View { &data, 1 });
        return;
    }
    flush_character_insertions();
    m_character_insertion_node = node;
    m_character_insertion_builder.append(Utf32View { &data, 1 });
}

void HTMLDocumentParser::handle_after_head(HTMLToken& token)
{
    if (token.is_character() && token.is_parser_whitespace()) {
        insert_character(token.code_point());
        return;
    }

    if (token.is_comment()) {
        insert_comment(token);
        return;
    }

    if (token.is_doctype()) {
        log_parse_error();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::html) {
        process_using_the_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::body) {
        insert_html_element(token);
        m_frameset_ok = false;
        m_insertion_mode = InsertionMode::InBody;
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::frameset) {
        insert_html_element(token);
        m_insertion_mode = InsertionMode::InFrameset;
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::base, HTML::TagNames::basefont, HTML::TagNames::bgsound, HTML::TagNames::link, HTML::TagNames::meta, HTML::TagNames::noframes, HTML::TagNames::script, HTML::TagNames::style, HTML::TagNames::template_, HTML::TagNames::title)) {
        log_parse_error();
        m_stack_of_open_elements.push(*m_head_element);
        process_using_the_rules_for(InsertionMode::InHead, token);
        m_stack_of_open_elements.elements().remove_first_matching([&](auto& entry) {
            return entry.ptr() == m_head_element;
        });
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::template_) {
        process_using_the_rules_for(InsertionMode::InHead, token);
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::body, HTML::TagNames::html, HTML::TagNames::br)) {
        goto AnythingElse;
    }

    if ((token.is_start_tag() && token.tag_name() == HTML::TagNames::head) || token.is_end_tag()) {
        log_parse_error();
        return;
    }

AnythingElse:
    insert_html_element(HTMLToken::make_start_tag(HTML::TagNames::body));
    m_insertion_mode = InsertionMode::InBody;
    process_using_the_rules_for(m_insertion_mode, token);
}

void HTMLDocumentParser::generate_implied_end_tags(const FlyString& exception)
{
    while (current_node().local_name() != exception && current_node().local_name().is_one_of(HTML::TagNames::dd, HTML::TagNames::dt, HTML::TagNames::li, HTML::TagNames::optgroup, HTML::TagNames::option, HTML::TagNames::p, HTML::TagNames::rb, HTML::TagNames::rp, HTML::TagNames::rt, HTML::TagNames::rtc))
        m_stack_of_open_elements.pop();
}

void HTMLDocumentParser::generate_all_implied_end_tags_thoroughly()
{
    while (current_node().local_name().is_one_of(HTML::TagNames::caption, HTML::TagNames::colgroup, HTML::TagNames::dd, HTML::TagNames::dt, HTML::TagNames::li, HTML::TagNames::optgroup, HTML::TagNames::option, HTML::TagNames::p, HTML::TagNames::rb, HTML::TagNames::rp, HTML::TagNames::rt, HTML::TagNames::rtc, HTML::TagNames::tbody, HTML::TagNames::td, HTML::TagNames::tfoot, HTML::TagNames::th, HTML::TagNames::thead, HTML::TagNames::tr))
        m_stack_of_open_elements.pop();
}

void HTMLDocumentParser::close_a_p_element()
{
    generate_implied_end_tags(HTML::TagNames::p);
    if (current_node().local_name() != HTML::TagNames::p) {
        log_parse_error();
    }
    m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::p);
}

void HTMLDocumentParser::handle_after_body(HTMLToken& token)
{
    if (token.is_character() && token.is_parser_whitespace()) {
        process_using_the_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (token.is_comment()) {
        auto& insertion_location = m_stack_of_open_elements.first();
        insertion_location.append_child(adopt_ref(*new DOM::Comment(document(), token.comment())));
        return;
    }

    if (token.is_doctype()) {
        log_parse_error();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::html) {
        process_using_the_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::html) {
        if (m_parsing_fragment) {
            log_parse_error();
            return;
        }
        m_insertion_mode = InsertionMode::AfterAfterBody;
        return;
    }

    if (token.is_end_of_file()) {
        stop_parsing();
        return;
    }

    log_parse_error();
    m_insertion_mode = InsertionMode::InBody;
    process_using_the_rules_for(InsertionMode::InBody, token);
}

void HTMLDocumentParser::handle_after_after_body(HTMLToken& token)
{
    if (token.is_comment()) {
        auto comment = adopt_ref(*new DOM::Comment(document(), token.comment()));
        document().append_child(move(comment));
        return;
    }

    if (token.is_doctype() || token.is_parser_whitespace() || (token.is_start_tag() && token.tag_name() == HTML::TagNames::html)) {
        process_using_the_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (token.is_end_of_file()) {
        stop_parsing();
        return;
    }

    log_parse_error();
    m_insertion_mode = InsertionMode::InBody;
    process_using_the_rules_for(m_insertion_mode, token);
}

void HTMLDocumentParser::reconstruct_the_active_formatting_elements()
{
    // FIXME: This needs to care about "markers"

    if (m_list_of_active_formatting_elements.is_empty())
        return;

    if (m_list_of_active_formatting_elements.entries().last().is_marker())
        return;

    if (m_stack_of_open_elements.contains(*m_list_of_active_formatting_elements.entries().last().element))
        return;

    ssize_t index = m_list_of_active_formatting_elements.entries().size() - 1;
    RefPtr<DOM::Element> entry = m_list_of_active_formatting_elements.entries().at(index).element;
    VERIFY(entry);

Rewind:
    if (index == 0) {
        goto Create;
    }

    --index;
    entry = m_list_of_active_formatting_elements.entries().at(index).element;
    VERIFY(entry);

    if (!m_stack_of_open_elements.contains(*entry))
        goto Rewind;

Advance:
    ++index;
    entry = m_list_of_active_formatting_elements.entries().at(index).element;
    VERIFY(entry);

Create:
    // FIXME: Hold on to the real token!
    auto new_element = insert_html_element(HTMLToken::make_start_tag(entry->local_name()));

    m_list_of_active_formatting_elements.entries().at(index).element = *new_element;

    if (index != (ssize_t)m_list_of_active_formatting_elements.entries().size() - 1)
        goto Advance;
}

HTMLDocumentParser::AdoptionAgencyAlgorithmOutcome HTMLDocumentParser::run_the_adoption_agency_algorithm(HTMLToken& token)
{
    auto subject = token.tag_name();

    // If the current node is an HTML element whose tag name is subject,
    // and the current node is not in the list of active formatting elements,
    // then pop the current node off the stack of open elements, and return.
    if (current_node().local_name() == subject && !m_list_of_active_formatting_elements.contains(current_node())) {
        m_stack_of_open_elements.pop();
        return AdoptionAgencyAlgorithmOutcome::DoNothing;
    }

    size_t outer_loop_counter = 0;

    //OuterLoop:
    if (outer_loop_counter >= 8)
        return AdoptionAgencyAlgorithmOutcome::DoNothing;

    ++outer_loop_counter;

    auto formatting_element = m_list_of_active_formatting_elements.last_element_with_tag_name_before_marker(subject);
    if (!formatting_element)
        return AdoptionAgencyAlgorithmOutcome::RunAnyOtherEndTagSteps;

    if (!m_stack_of_open_elements.contains(*formatting_element)) {
        log_parse_error();
        m_list_of_active_formatting_elements.remove(*formatting_element);
        return AdoptionAgencyAlgorithmOutcome::DoNothing;
    }

    if (!m_stack_of_open_elements.has_in_scope(*formatting_element)) {
        log_parse_error();
        return AdoptionAgencyAlgorithmOutcome::DoNothing;
    }

    if (formatting_element != &current_node()) {
        log_parse_error();
    }

    RefPtr<DOM::Element> furthest_block = m_stack_of_open_elements.topmost_special_node_below(*formatting_element);

    if (!furthest_block) {
        while (&current_node() != formatting_element)
            m_stack_of_open_elements.pop();
        m_stack_of_open_elements.pop();

        m_list_of_active_formatting_elements.remove(*formatting_element);
        return AdoptionAgencyAlgorithmOutcome::DoNothing;
    }

    // FIXME: Implement the rest of the AAA :^)

    TODO();
}

bool HTMLDocumentParser::is_special_tag(const FlyString& tag_name, const FlyString& namespace_)
{
    if (namespace_ == Namespace::HTML) {
        return tag_name.is_one_of(
            HTML::TagNames::address,
            HTML::TagNames::applet,
            HTML::TagNames::area,
            HTML::TagNames::article,
            HTML::TagNames::aside,
            HTML::TagNames::base,
            HTML::TagNames::basefont,
            HTML::TagNames::bgsound,
            HTML::TagNames::blockquote,
            HTML::TagNames::body,
            HTML::TagNames::br,
            HTML::TagNames::button,
            HTML::TagNames::caption,
            HTML::TagNames::center,
            HTML::TagNames::col,
            HTML::TagNames::colgroup,
            HTML::TagNames::dd,
            HTML::TagNames::details,
            HTML::TagNames::dir,
            HTML::TagNames::div,
            HTML::TagNames::dl,
            HTML::TagNames::dt,
            HTML::TagNames::embed,
            HTML::TagNames::fieldset,
            HTML::TagNames::figcaption,
            HTML::TagNames::figure,
            HTML::TagNames::footer,
            HTML::TagNames::form,
            HTML::TagNames::frame,
            HTML::TagNames::frameset,
            HTML::TagNames::h1,
            HTML::TagNames::h2,
            HTML::TagNames::h3,
            HTML::TagNames::h4,
            HTML::TagNames::h5,
            HTML::TagNames::h6,
            HTML::TagNames::head,
            HTML::TagNames::header,
            HTML::TagNames::hgroup,
            HTML::TagNames::hr,
            HTML::TagNames::html,
            HTML::TagNames::iframe,
            HTML::TagNames::img,
            HTML::TagNames::input,
            HTML::TagNames::keygen,
            HTML::TagNames::li,
            HTML::TagNames::link,
            HTML::TagNames::listing,
            HTML::TagNames::main,
            HTML::TagNames::marquee,
            HTML::TagNames::menu,
            HTML::TagNames::meta,
            HTML::TagNames::nav,
            HTML::TagNames::noembed,
            HTML::TagNames::noframes,
            HTML::TagNames::noscript,
            HTML::TagNames::object,
            HTML::TagNames::ol,
            HTML::TagNames::p,
            HTML::TagNames::param,
            HTML::TagNames::plaintext,
            HTML::TagNames::pre,
            HTML::TagNames::script,
            HTML::TagNames::section,
            HTML::TagNames::select,
            HTML::TagNames::source,
            HTML::TagNames::style,
            HTML::TagNames::summary,
            HTML::TagNames::table,
            HTML::TagNames::tbody,
            HTML::TagNames::td,
            HTML::TagNames::template_,
            HTML::TagNames::textarea,
            HTML::TagNames::tfoot,
            HTML::TagNames::th,
            HTML::TagNames::thead,
            HTML::TagNames::title,
            HTML::TagNames::tr,
            HTML::TagNames::track,
            HTML::TagNames::ul,
            HTML::TagNames::wbr,
            HTML::TagNames::xmp);
    } else if (namespace_ == Namespace::SVG) {
        return tag_name.is_one_of(
            SVG::TagNames::desc,
            SVG::TagNames::foreignObject,
            SVG::TagNames::title);
    } else if (namespace_ == Namespace::MathML) {
        TODO();
    }

    return false;
}

void HTMLDocumentParser::handle_in_body(HTMLToken& token)
{
    if (token.is_character()) {
        if (token.code_point() == 0) {
            log_parse_error();
            return;
        }
        if (token.is_parser_whitespace()) {
            reconstruct_the_active_formatting_elements();
            insert_character(token.code_point());
            return;
        }
        reconstruct_the_active_formatting_elements();
        insert_character(token.code_point());
        m_frameset_ok = false;
        return;
    }

    if (token.is_comment()) {
        insert_comment(token);
        return;
    }

    if (token.is_doctype()) {
        log_parse_error();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::html) {
        log_parse_error();
        if (m_stack_of_open_elements.contains(HTML::TagNames::template_))
            return;
        token.for_each_attribute([&](auto& attribute) {
            if (!current_node().has_attribute(attribute.local_name))
                current_node().set_attribute(attribute.local_name, attribute.value);
            return IterationDecision::Continue;
        });
        return;
    }
    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::base, HTML::TagNames::basefont, HTML::TagNames::bgsound, HTML::TagNames::link, HTML::TagNames::meta, HTML::TagNames::noframes, HTML::TagNames::script, HTML::TagNames::style, HTML::TagNames::template_, HTML::TagNames::title)) {
        process_using_the_rules_for(InsertionMode::InHead, token);
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::template_) {
        process_using_the_rules_for(InsertionMode::InHead, token);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::body) {
        log_parse_error();
        if (m_stack_of_open_elements.elements().size() == 1
            || m_stack_of_open_elements.elements().at(1).local_name() != HTML::TagNames::body
            || m_stack_of_open_elements.contains(HTML::TagNames::template_)) {
            VERIFY(m_parsing_fragment);
            return;
        }
        m_frameset_ok = false;
        auto& body_element = m_stack_of_open_elements.elements().at(1);
        token.for_each_attribute([&](auto& attribute) {
            if (!body_element.has_attribute(attribute.local_name))
                body_element.set_attribute(attribute.local_name, attribute.value);
            return IterationDecision::Continue;
        });
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::frameset) {
        log_parse_error();

        if (m_stack_of_open_elements.elements().size() == 1
            || m_stack_of_open_elements.elements().at(1).local_name() != HTML::TagNames::body) {
            VERIFY(m_parsing_fragment);
            return;
        }

        if (!m_frameset_ok)
            return;

        TODO();
    }

    if (token.is_end_of_file()) {
        if (!m_stack_of_template_insertion_modes.is_empty()) {
            process_using_the_rules_for(InsertionMode::InTemplate, token);
            return;
        }

        for (auto& node : m_stack_of_open_elements.elements()) {
            if (!node.local_name().is_one_of(HTML::TagNames::dd, HTML::TagNames::dt, HTML::TagNames::li, HTML::TagNames::optgroup, HTML::TagNames::option, HTML::TagNames::p, HTML::TagNames::rb, HTML::TagNames::rp, HTML::TagNames::rt, HTML::TagNames::rtc, HTML::TagNames::tbody, HTML::TagNames::td, HTML::TagNames::tfoot, HTML::TagNames::th, HTML::TagNames::thead, HTML::TagNames::tr, HTML::TagNames::body, HTML::TagNames::html)) {
                log_parse_error();
                break;
            }
        }

        stop_parsing();
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::body) {
        if (!m_stack_of_open_elements.has_in_scope(HTML::TagNames::body)) {
            log_parse_error();
            return;
        }

        for (auto& node : m_stack_of_open_elements.elements()) {
            if (!node.local_name().is_one_of(HTML::TagNames::dd, HTML::TagNames::dt, HTML::TagNames::li, HTML::TagNames::optgroup, HTML::TagNames::option, HTML::TagNames::p, HTML::TagNames::rb, HTML::TagNames::rp, HTML::TagNames::rt, HTML::TagNames::rtc, HTML::TagNames::tbody, HTML::TagNames::td, HTML::TagNames::tfoot, HTML::TagNames::th, HTML::TagNames::thead, HTML::TagNames::tr, HTML::TagNames::body, HTML::TagNames::html)) {
                log_parse_error();
                break;
            }
        }

        m_insertion_mode = InsertionMode::AfterBody;
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::html) {
        if (!m_stack_of_open_elements.has_in_scope(HTML::TagNames::body)) {
            log_parse_error();
            return;
        }

        for (auto& node : m_stack_of_open_elements.elements()) {
            if (!node.local_name().is_one_of(HTML::TagNames::dd, HTML::TagNames::dt, HTML::TagNames::li, HTML::TagNames::optgroup, HTML::TagNames::option, HTML::TagNames::p, HTML::TagNames::rb, HTML::TagNames::rp, HTML::TagNames::rt, HTML::TagNames::rtc, HTML::TagNames::tbody, HTML::TagNames::td, HTML::TagNames::tfoot, HTML::TagNames::th, HTML::TagNames::thead, HTML::TagNames::tr, HTML::TagNames::body, HTML::TagNames::html)) {
                log_parse_error();
                break;
            }
        }

        m_insertion_mode = InsertionMode::AfterBody;
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::address, HTML::TagNames::article, HTML::TagNames::aside, HTML::TagNames::blockquote, HTML::TagNames::center, HTML::TagNames::details, HTML::TagNames::dialog, HTML::TagNames::dir, HTML::TagNames::div, HTML::TagNames::dl, HTML::TagNames::fieldset, HTML::TagNames::figcaption, HTML::TagNames::figure, HTML::TagNames::footer, HTML::TagNames::header, HTML::TagNames::hgroup, HTML::TagNames::main, HTML::TagNames::menu, HTML::TagNames::nav, HTML::TagNames::ol, HTML::TagNames::p, HTML::TagNames::section, HTML::TagNames::summary, HTML::TagNames::ul)) {
        if (m_stack_of_open_elements.has_in_button_scope(HTML::TagNames::p))
            close_a_p_element();
        insert_html_element(token);
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::h1, HTML::TagNames::h2, HTML::TagNames::h3, HTML::TagNames::h4, HTML::TagNames::h5, HTML::TagNames::h6)) {
        if (m_stack_of_open_elements.has_in_button_scope(HTML::TagNames::p))
            close_a_p_element();
        if (current_node().local_name().is_one_of(HTML::TagNames::h1, HTML::TagNames::h2, HTML::TagNames::h3, HTML::TagNames::h4, HTML::TagNames::h5, HTML::TagNames::h6)) {
            log_parse_error();
            m_stack_of_open_elements.pop();
        }
        insert_html_element(token);
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::pre, HTML::TagNames::listing)) {
        if (m_stack_of_open_elements.has_in_button_scope(HTML::TagNames::p))
            close_a_p_element();

        insert_html_element(token);

        m_frameset_ok = false;

        // If the next token is a U+000A LINE FEED (LF) character token,
        // then ignore that token and move on to the next one.
        // (Newlines at the start of pre blocks are ignored as an authoring convenience.)
        auto next_token = m_tokenizer.next_token();
        if (next_token.has_value() && next_token.value().is_character() && next_token.value().code_point() == '\n') {
            // Ignore it.
        } else {
            process_using_the_rules_for(m_insertion_mode, next_token.value());
        }
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::form) {
        if (m_form_element && !m_stack_of_open_elements.contains(HTML::TagNames::template_)) {
            log_parse_error();
            return;
        }
        if (m_stack_of_open_elements.has_in_button_scope(HTML::TagNames::p))
            close_a_p_element();
        auto element = insert_html_element(token);
        if (!m_stack_of_open_elements.contains(HTML::TagNames::template_))
            m_form_element = verify_cast<HTMLFormElement>(*element);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::li) {
        m_frameset_ok = false;

        for (ssize_t i = m_stack_of_open_elements.elements().size() - 1; i >= 0; --i) {
            RefPtr<DOM::Element> node = m_stack_of_open_elements.elements()[i];

            if (node->local_name() == HTML::TagNames::li) {
                generate_implied_end_tags(HTML::TagNames::li);
                if (current_node().local_name() != HTML::TagNames::li) {
                    log_parse_error();
                }
                m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::li);
                break;
            }

            if (is_special_tag(node->local_name(), node->namespace_()) && !node->local_name().is_one_of(HTML::TagNames::address, HTML::TagNames::div, HTML::TagNames::p))
                break;
        }

        if (m_stack_of_open_elements.has_in_button_scope(HTML::TagNames::p))
            close_a_p_element();

        insert_html_element(token);
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::dd, HTML::TagNames::dt)) {
        m_frameset_ok = false;
        for (ssize_t i = m_stack_of_open_elements.elements().size() - 1; i >= 0; --i) {
            RefPtr<DOM::Element> node = m_stack_of_open_elements.elements()[i];
            if (node->local_name() == HTML::TagNames::dd) {
                generate_implied_end_tags(HTML::TagNames::dd);
                if (current_node().local_name() != HTML::TagNames::dd) {
                    log_parse_error();
                }
                m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::dd);
                break;
            }
            if (node->local_name() == HTML::TagNames::dt) {
                generate_implied_end_tags(HTML::TagNames::dt);
                if (current_node().local_name() != HTML::TagNames::dt) {
                    log_parse_error();
                }
                m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::dt);
                break;
            }
            if (is_special_tag(node->local_name(), node->namespace_()) && !node->local_name().is_one_of(HTML::TagNames::address, HTML::TagNames::div, HTML::TagNames::p))
                break;
        }
        if (m_stack_of_open_elements.has_in_button_scope(HTML::TagNames::p))
            close_a_p_element();
        insert_html_element(token);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::plaintext) {
        if (m_stack_of_open_elements.has_in_button_scope(HTML::TagNames::p))
            close_a_p_element();
        insert_html_element(token);
        m_tokenizer.switch_to({}, HTMLTokenizer::State::PLAINTEXT);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::button) {
        if (m_stack_of_open_elements.has_in_button_scope(HTML::TagNames::button)) {
            log_parse_error();
            generate_implied_end_tags();
            m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::button);
        }
        reconstruct_the_active_formatting_elements();
        insert_html_element(token);
        m_frameset_ok = false;
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::address, HTML::TagNames::article, HTML::TagNames::aside, HTML::TagNames::blockquote, HTML::TagNames::button, HTML::TagNames::center, HTML::TagNames::details, HTML::TagNames::dialog, HTML::TagNames::dir, HTML::TagNames::div, HTML::TagNames::dl, HTML::TagNames::fieldset, HTML::TagNames::figcaption, HTML::TagNames::figure, HTML::TagNames::footer, HTML::TagNames::header, HTML::TagNames::hgroup, HTML::TagNames::listing, HTML::TagNames::main, HTML::TagNames::menu, HTML::TagNames::nav, HTML::TagNames::ol, HTML::TagNames::pre, HTML::TagNames::section, HTML::TagNames::summary, HTML::TagNames::ul)) {
        if (!m_stack_of_open_elements.has_in_scope(token.tag_name())) {
            log_parse_error();
            return;
        }

        generate_implied_end_tags();

        if (current_node().local_name() != token.tag_name()) {
            log_parse_error();
        }

        m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(token.tag_name());
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::form) {
        if (!m_stack_of_open_elements.contains(HTML::TagNames::template_)) {
            auto node = m_form_element;
            m_form_element = nullptr;
            if (!node || !m_stack_of_open_elements.has_in_scope(*node)) {
                log_parse_error();
                return;
            }
            generate_implied_end_tags();
            if (&current_node() != node) {
                log_parse_error();
            }
            m_stack_of_open_elements.elements().remove_first_matching([&](auto& entry) { return entry.ptr() == node.ptr(); });
        } else {
            if (!m_stack_of_open_elements.has_in_scope(HTML::TagNames::form)) {
                log_parse_error();
                return;
            }
            generate_implied_end_tags();
            if (current_node().local_name() != HTML::TagNames::form) {
                log_parse_error();
            }
            m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::form);
        }
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::p) {
        if (!m_stack_of_open_elements.has_in_button_scope(HTML::TagNames::p)) {
            log_parse_error();
            insert_html_element(HTMLToken::make_start_tag(HTML::TagNames::p));
        }
        close_a_p_element();
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::li) {
        if (!m_stack_of_open_elements.has_in_list_item_scope(HTML::TagNames::li)) {
            log_parse_error();
            return;
        }
        generate_implied_end_tags(HTML::TagNames::li);
        if (current_node().local_name() != HTML::TagNames::li) {
            log_parse_error();
            dbgln("Expected <li> current node, but had <{}>", current_node().local_name());
        }
        m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::li);
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::dd, HTML::TagNames::dt)) {
        if (!m_stack_of_open_elements.has_in_scope(token.tag_name())) {
            log_parse_error();
            return;
        }
        generate_implied_end_tags(token.tag_name());
        if (current_node().local_name() != token.tag_name()) {
            log_parse_error();
        }
        m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(token.tag_name());
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::h1, HTML::TagNames::h2, HTML::TagNames::h3, HTML::TagNames::h4, HTML::TagNames::h5, HTML::TagNames::h6)) {
        if (!m_stack_of_open_elements.has_in_scope(HTML::TagNames::h1)
            && !m_stack_of_open_elements.has_in_scope(HTML::TagNames::h2)
            && !m_stack_of_open_elements.has_in_scope(HTML::TagNames::h3)
            && !m_stack_of_open_elements.has_in_scope(HTML::TagNames::h4)
            && !m_stack_of_open_elements.has_in_scope(HTML::TagNames::h5)
            && !m_stack_of_open_elements.has_in_scope(HTML::TagNames::h6)) {
            log_parse_error();
            return;
        }

        generate_implied_end_tags();
        if (current_node().local_name() != token.tag_name()) {
            log_parse_error();
        }

        for (;;) {
            auto popped_element = m_stack_of_open_elements.pop();
            if (popped_element->local_name().is_one_of(HTML::TagNames::h1, HTML::TagNames::h2, HTML::TagNames::h3, HTML::TagNames::h4, HTML::TagNames::h5, HTML::TagNames::h6))
                break;
        }
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::a) {
        if (auto* element = m_list_of_active_formatting_elements.last_element_with_tag_name_before_marker(HTML::TagNames::a)) {
            log_parse_error();
            if (run_the_adoption_agency_algorithm(token) == AdoptionAgencyAlgorithmOutcome::RunAnyOtherEndTagSteps)
                goto AnyOtherEndTag;
            m_list_of_active_formatting_elements.remove(*element);
            m_stack_of_open_elements.elements().remove_first_matching([&](auto& entry) {
                return entry.ptr() == element;
            });
        }
        reconstruct_the_active_formatting_elements();
        auto element = insert_html_element(token);
        m_list_of_active_formatting_elements.add(*element);
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::b, HTML::TagNames::big, HTML::TagNames::code, HTML::TagNames::em, HTML::TagNames::font, HTML::TagNames::i, HTML::TagNames::s, HTML::TagNames::small, HTML::TagNames::strike, HTML::TagNames::strong, HTML::TagNames::tt, HTML::TagNames::u)) {
        reconstruct_the_active_formatting_elements();
        auto element = insert_html_element(token);
        m_list_of_active_formatting_elements.add(*element);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::nobr) {
        reconstruct_the_active_formatting_elements();
        if (m_stack_of_open_elements.has_in_scope(HTML::TagNames::nobr)) {
            log_parse_error();
            run_the_adoption_agency_algorithm(token);
            reconstruct_the_active_formatting_elements();
        }
        auto element = insert_html_element(token);
        m_list_of_active_formatting_elements.add(*element);
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::a, HTML::TagNames::b, HTML::TagNames::big, HTML::TagNames::code, HTML::TagNames::em, HTML::TagNames::font, HTML::TagNames::i, HTML::TagNames::nobr, HTML::TagNames::s, HTML::TagNames::small, HTML::TagNames::strike, HTML::TagNames::strong, HTML::TagNames::tt, HTML::TagNames::u)) {
        if (run_the_adoption_agency_algorithm(token) == AdoptionAgencyAlgorithmOutcome::RunAnyOtherEndTagSteps)
            goto AnyOtherEndTag;
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::applet, HTML::TagNames::marquee, HTML::TagNames::object)) {
        reconstruct_the_active_formatting_elements();
        insert_html_element(token);
        m_list_of_active_formatting_elements.add_marker();
        m_frameset_ok = false;
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::applet, HTML::TagNames::marquee, HTML::TagNames::object)) {
        if (!m_stack_of_open_elements.has_in_scope(token.tag_name())) {
            log_parse_error();
            return;
        }

        generate_implied_end_tags();
        if (current_node().local_name() != token.tag_name()) {
            log_parse_error();
        }
        m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(token.tag_name());
        m_list_of_active_formatting_elements.clear_up_to_the_last_marker();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::table) {
        if (!document().in_quirks_mode()) {
            if (m_stack_of_open_elements.has_in_button_scope(HTML::TagNames::p))
                close_a_p_element();
        }
        insert_html_element(token);
        m_frameset_ok = false;
        m_insertion_mode = InsertionMode::InTable;
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::br) {
        token.drop_attributes();
        goto BRStartTag;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::area, HTML::TagNames::br, HTML::TagNames::embed, HTML::TagNames::img, HTML::TagNames::keygen, HTML::TagNames::wbr)) {
    BRStartTag:
        reconstruct_the_active_formatting_elements();
        insert_html_element(token);
        m_stack_of_open_elements.pop();
        token.acknowledge_self_closing_flag_if_set();
        m_frameset_ok = false;
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::input) {
        reconstruct_the_active_formatting_elements();
        insert_html_element(token);
        m_stack_of_open_elements.pop();
        token.acknowledge_self_closing_flag_if_set();
        auto type_attribute = token.attribute(HTML::AttributeNames::type);
        if (type_attribute.is_null() || !type_attribute.equals_ignoring_case("hidden")) {
            m_frameset_ok = false;
        }
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::param, HTML::TagNames::source, HTML::TagNames::track)) {
        insert_html_element(token);
        m_stack_of_open_elements.pop();
        token.acknowledge_self_closing_flag_if_set();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::hr) {
        if (m_stack_of_open_elements.has_in_button_scope(HTML::TagNames::p))
            close_a_p_element();
        insert_html_element(token);
        m_stack_of_open_elements.pop();
        token.acknowledge_self_closing_flag_if_set();
        m_frameset_ok = false;
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::image) {
        // Parse error. Change the token's tag name to HTML::TagNames::img and reprocess it. (Don't ask.)
        log_parse_error();
        token.set_tag_name("img");
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::textarea) {
        insert_html_element(token);

        m_tokenizer.switch_to({}, HTMLTokenizer::State::RCDATA);

        // If the next token is a U+000A LINE FEED (LF) character token,
        // then ignore that token and move on to the next one.
        // (Newlines at the start of pre blocks are ignored as an authoring convenience.)
        auto next_token = m_tokenizer.next_token();

        m_original_insertion_mode = m_insertion_mode;
        m_frameset_ok = false;
        m_insertion_mode = InsertionMode::Text;

        if (next_token.has_value() && next_token.value().is_character() && next_token.value().code_point() == '\n') {
            // Ignore it.
        } else {
            process_using_the_rules_for(m_insertion_mode, next_token.value());
        }
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::xmp) {
        if (m_stack_of_open_elements.has_in_button_scope(HTML::TagNames::p)) {
            close_a_p_element();
        }
        reconstruct_the_active_formatting_elements();
        m_frameset_ok = false;
        parse_generic_raw_text_element(token);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::iframe) {
        m_frameset_ok = false;
        parse_generic_raw_text_element(token);
        return;
    }

    if (token.is_start_tag() && ((token.tag_name() == HTML::TagNames::noembed) || (token.tag_name() == HTML::TagNames::noscript && m_scripting_enabled))) {
        parse_generic_raw_text_element(token);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::select) {
        reconstruct_the_active_formatting_elements();
        insert_html_element(token);
        m_frameset_ok = false;
        switch (m_insertion_mode) {
        case InsertionMode::InTable:
        case InsertionMode::InCaption:
        case InsertionMode::InTableBody:
        case InsertionMode::InRow:
        case InsertionMode::InCell:
            m_insertion_mode = InsertionMode::InSelectInTable;
            break;
        default:
            m_insertion_mode = InsertionMode::InSelect;
            break;
        }
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::optgroup, HTML::TagNames::option)) {
        if (current_node().local_name() == HTML::TagNames::option)
            m_stack_of_open_elements.pop();
        reconstruct_the_active_formatting_elements();
        insert_html_element(token);
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::rb, HTML::TagNames::rtc)) {
        if (m_stack_of_open_elements.has_in_scope(HTML::TagNames::ruby))
            generate_implied_end_tags();

        if (current_node().local_name() != HTML::TagNames::ruby)
            log_parse_error();

        insert_html_element(token);
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::rp, HTML::TagNames::rt)) {
        if (m_stack_of_open_elements.has_in_scope(HTML::TagNames::ruby))
            generate_implied_end_tags(HTML::TagNames::rtc);

        if (current_node().local_name() != HTML::TagNames::rtc || current_node().local_name() != HTML::TagNames::ruby)
            log_parse_error();

        insert_html_element(token);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::math) {
        reconstruct_the_active_formatting_elements();
        adjust_mathml_attributes(token);
        adjust_foreign_attributes(token);

        insert_foreign_element(token, Namespace::MathML);

        if (token.is_self_closing()) {
            m_stack_of_open_elements.pop();
            token.acknowledge_self_closing_flag_if_set();
        }
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::svg) {
        reconstruct_the_active_formatting_elements();
        adjust_svg_attributes(token);
        adjust_foreign_attributes(token);

        insert_foreign_element(token, Namespace::SVG);

        if (token.is_self_closing()) {
            m_stack_of_open_elements.pop();
            token.acknowledge_self_closing_flag_if_set();
        }
        return;
    }

    if ((token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::caption, HTML::TagNames::col, HTML::TagNames::colgroup, HTML::TagNames::frame, HTML::TagNames::head, HTML::TagNames::tbody, HTML::TagNames::td, HTML::TagNames::tfoot, HTML::TagNames::th, HTML::TagNames::thead, HTML::TagNames::tr))) {
        log_parse_error();
        return;
    }

    // Any other start tag
    if (token.is_start_tag()) {
        reconstruct_the_active_formatting_elements();
        insert_html_element(token);
        return;
    }

    if (token.is_end_tag()) {
    AnyOtherEndTag:
        RefPtr<DOM::Element> node;
        for (ssize_t i = m_stack_of_open_elements.elements().size() - 1; i >= 0; --i) {
            node = m_stack_of_open_elements.elements()[i];
            if (node->local_name() == token.tag_name()) {
                generate_implied_end_tags(token.tag_name());
                if (node != current_node()) {
                    log_parse_error();
                }
                while (&current_node() != node) {
                    m_stack_of_open_elements.pop();
                }
                m_stack_of_open_elements.pop();
                break;
            }
            if (is_special_tag(node->local_name(), node->namespace_())) {
                log_parse_error();
                return;
            }
        }
        return;
    }
}

void HTMLDocumentParser::adjust_mathml_attributes(HTMLToken& token)
{
    token.adjust_attribute_name("definitionurl", "definitionURL");
}

void HTMLDocumentParser::adjust_svg_tag_names(HTMLToken& token)
{
    token.adjust_tag_name("altglyph", "altGlyph");
    token.adjust_tag_name("altglyphdef", "altGlyphDef");
    token.adjust_tag_name("altglyphitem", "altGlyphItem");
    token.adjust_tag_name("animatecolor", "animateColor");
    token.adjust_tag_name("animatemotion", "animateMotion");
    token.adjust_tag_name("animatetransform", "animateTransform");
    token.adjust_tag_name("clippath", "clipPath");
    token.adjust_tag_name("feblend", "feBlend");
    token.adjust_tag_name("fecolormatrix", "feColorMatrix");
    token.adjust_tag_name("fecomponenttransfer", "feComponentTransfer");
    token.adjust_tag_name("fecomposite", "feComposite");
    token.adjust_tag_name("feconvolvematrix", "feConvolveMatrix");
    token.adjust_tag_name("fediffuselighting", "feDiffuseLighting");
    token.adjust_tag_name("fedisplacementmap", "feDisplacementMap");
    token.adjust_tag_name("fedistantlight", "feDistantLight");
    token.adjust_tag_name("fedropshadow", "feDropShadow");
    token.adjust_tag_name("feflood", "feFlood");
    token.adjust_tag_name("fefunca", "feFuncA");
    token.adjust_tag_name("fefuncb", "feFuncB");
    token.adjust_tag_name("fefuncg", "feFuncG");
    token.adjust_tag_name("fefuncr", "feFuncR");
    token.adjust_tag_name("fegaussianblur", "feGaussianBlur");
    token.adjust_tag_name("feimage", "feImage");
    token.adjust_tag_name("femerge", "feMerge");
    token.adjust_tag_name("femergenode", "feMergeNode");
    token.adjust_tag_name("femorphology", "feMorphology");
    token.adjust_tag_name("feoffset", "feOffset");
    token.adjust_tag_name("fepointlight", "fePointLight");
    token.adjust_tag_name("fespecularlighting", "feSpecularLighting");
    token.adjust_tag_name("fespotlight", "feSpotlight");
    token.adjust_tag_name("glyphref", "glyphRef");
    token.adjust_tag_name("lineargradient", "linearGradient");
    token.adjust_tag_name("radialgradient", "radialGradient");
    token.adjust_tag_name("textpath", "textPath");
}

void HTMLDocumentParser::adjust_svg_attributes(HTMLToken& token)
{
    token.adjust_attribute_name("attributename", "attributeName");
    token.adjust_attribute_name("attributetype", "attributeType");
    token.adjust_attribute_name("basefrequency", "baseFrequency");
    token.adjust_attribute_name("baseprofile", "baseProfile");
    token.adjust_attribute_name("calcmode", "calcMode");
    token.adjust_attribute_name("clippathunits", "clipPathUnits");
    token.adjust_attribute_name("diffuseconstant", "diffuseConstant");
    token.adjust_attribute_name("edgemode", "edgeMode");
    token.adjust_attribute_name("filterunits", "filterUnits");
    token.adjust_attribute_name("glyphref", "glyphRef");
    token.adjust_attribute_name("gradienttransform", "gradientTransform");
    token.adjust_attribute_name("gradientunits", "gradientUnits");
    token.adjust_attribute_name("kernelmatrix", "kernelMatrix");
    token.adjust_attribute_name("kernelunitlength", "kernelUnitLength");
    token.adjust_attribute_name("keypoints", "keyPoints");
    token.adjust_attribute_name("keysplines", "keySplines");
    token.adjust_attribute_name("keytimes", "keyTimes");
    token.adjust_attribute_name("lengthadjust", "lengthAdjust");
    token.adjust_attribute_name("limitingconeangle", "limitingConeAngle");
    token.adjust_attribute_name("markerheight", "markerHeight");
    token.adjust_attribute_name("markerunits", "markerUnits");
    token.adjust_attribute_name("markerwidth", "markerWidth");
    token.adjust_attribute_name("maskcontentunits", "maskContentUnits");
    token.adjust_attribute_name("maskunits", "maskUnits");
    token.adjust_attribute_name("numoctaves", "numOctaves");
    token.adjust_attribute_name("pathlength", "pathLength");
    token.adjust_attribute_name("patterncontentunits", "patternContentUnits");
    token.adjust_attribute_name("patterntransform", "patternTransform");
    token.adjust_attribute_name("patternunits", "patternUnits");
    token.adjust_attribute_name("pointsatx", "pointsAtX");
    token.adjust_attribute_name("pointsaty", "pointsAtY");
    token.adjust_attribute_name("pointsatz", "pointsAtZ");
    token.adjust_attribute_name("preservealpha", "preserveAlpha");
    token.adjust_attribute_name("preserveaspectratio", "preserveAspectRatio");
    token.adjust_attribute_name("primitiveunits", "primitiveUnits");
    token.adjust_attribute_name("refx", "refX");
    token.adjust_attribute_name("refy", "refY");
    token.adjust_attribute_name("repeatcount", "repeatCount");
    token.adjust_attribute_name("repeatdur", "repeatDur");
    token.adjust_attribute_name("requiredextensions", "requiredExtensions");
    token.adjust_attribute_name("requiredfeatures", "requiredFeatures");
    token.adjust_attribute_name("specularconstant", "specularConstant");
    token.adjust_attribute_name("specularexponent", "specularExponent");
    token.adjust_attribute_name("spreadmethod", "spreadMethod");
    token.adjust_attribute_name("startoffset", "startOffset");
    token.adjust_attribute_name("stddeviation", "stdDeviation");
    token.adjust_attribute_name("stitchtiles", "stitchTiles");
    token.adjust_attribute_name("surfacescale", "surfaceScale");
    token.adjust_attribute_name("systemlanguage", "systemLanguage");
    token.adjust_attribute_name("tablevalues", "tableValues");
    token.adjust_attribute_name("targetx", "targetX");
    token.adjust_attribute_name("targety", "targetY");
    token.adjust_attribute_name("textlength", "textLength");
    token.adjust_attribute_name("viewbox", "viewBox");
    token.adjust_attribute_name("viewtarget", "viewTarget");
    token.adjust_attribute_name("xchannelselector", "xChannelSelector");
    token.adjust_attribute_name("ychannelselector", "yChannelSelector");
    token.adjust_attribute_name("zoomandpan", "zoomAndPan");
}

void HTMLDocumentParser::adjust_foreign_attributes(HTMLToken& token)
{
    token.adjust_foreign_attribute("xlink:actuate", "xlink", "actuate", Namespace::XLink);
    token.adjust_foreign_attribute("xlink:arcrole", "xlink", "arcrole", Namespace::XLink);
    token.adjust_foreign_attribute("xlink:href", "xlink", "href", Namespace::XLink);
    token.adjust_foreign_attribute("xlink:role", "xlink", "role", Namespace::XLink);
    token.adjust_foreign_attribute("xlink:show", "xlink", "show", Namespace::XLink);
    token.adjust_foreign_attribute("xlink:title", "xlink", "title", Namespace::XLink);
    token.adjust_foreign_attribute("xlink:type", "xlink", "type", Namespace::XLink);

    token.adjust_foreign_attribute("xml:lang", "xml", "lang", Namespace::XML);
    token.adjust_foreign_attribute("xml:space", "xml", "space", Namespace::XML);

    token.adjust_foreign_attribute("xmlns", "", "xmlns", Namespace::XMLNS);
    token.adjust_foreign_attribute("xmlns:xlink", "xmlns", "xlink", Namespace::XMLNS);
}

void HTMLDocumentParser::increment_script_nesting_level()
{
    ++m_script_nesting_level;
}

void HTMLDocumentParser::decrement_script_nesting_level()
{
    VERIFY(m_script_nesting_level);
    --m_script_nesting_level;
}

void HTMLDocumentParser::handle_text(HTMLToken& token)
{
    if (token.is_character()) {
        insert_character(token.code_point());
        return;
    }
    if (token.is_end_of_file()) {
        log_parse_error();
        if (current_node().local_name() == HTML::TagNames::script)
            verify_cast<HTMLScriptElement>(current_node()).set_already_started({}, true);
        m_stack_of_open_elements.pop();
        m_insertion_mode = m_original_insertion_mode;
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }
    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::script) {
        // Make sure the <script> element has up-to-date text content before preparing the script.
        flush_character_insertions();

        NonnullRefPtr<HTMLScriptElement> script = verify_cast<HTMLScriptElement>(current_node());
        m_stack_of_open_elements.pop();
        m_insertion_mode = m_original_insertion_mode;
        // FIXME: Handle tokenizer insertion point stuff here.
        increment_script_nesting_level();
        script->prepare_script({});
        decrement_script_nesting_level();
        if (script_nesting_level() == 0)
            m_parser_pause_flag = false;
        // FIXME: Handle tokenizer insertion point stuff here too.

        while (document().pending_parsing_blocking_script()) {
            if (script_nesting_level() != 0) {
                m_parser_pause_flag = true;
                // FIXME: Abort the processing of any nested invocations of the tokenizer,
                //        yielding control back to the caller. (Tokenization will resume when
                //        the caller returns to the "outer" tree construction stage.)
                TODO();
            } else {
                auto the_script = document().take_pending_parsing_blocking_script({});
                m_tokenizer.set_blocked(true);

                // FIXME: If the parser's Document has a style sheet that is blocking scripts
                //        or the script's "ready to be parser-executed" flag is not set:
                //        spin the event loop until the parser's Document has no style sheet
                //        that is blocking scripts and the script's "ready to be parser-executed"
                //        flag is set.

                if (the_script->failed_to_load())
                    return;

                VERIFY(the_script->is_ready_to_be_parser_executed());

                if (m_aborted)
                    return;

                m_tokenizer.set_blocked(false);

                // FIXME: Handle tokenizer insertion point stuff here too.

                VERIFY(script_nesting_level() == 0);
                increment_script_nesting_level();

                the_script->execute_script();

                decrement_script_nesting_level();
                VERIFY(script_nesting_level() == 0);
                m_parser_pause_flag = false;

                // FIXME: Handle tokenizer insertion point stuff here too.
            }
        }
        return;
    }

    if (token.is_end_tag()) {
        m_stack_of_open_elements.pop();
        m_insertion_mode = m_original_insertion_mode;
        return;
    }
    TODO();
}

void HTMLDocumentParser::clear_the_stack_back_to_a_table_context()
{
    while (!current_node().local_name().is_one_of(HTML::TagNames::table, HTML::TagNames::template_, HTML::TagNames::html))
        m_stack_of_open_elements.pop();

    if (current_node().local_name() == HTML::TagNames::html)
        VERIFY(m_parsing_fragment);
}

void HTMLDocumentParser::clear_the_stack_back_to_a_table_row_context()
{
    while (!current_node().local_name().is_one_of(HTML::TagNames::tr, HTML::TagNames::template_, HTML::TagNames::html))
        m_stack_of_open_elements.pop();

    if (current_node().local_name() == HTML::TagNames::html)
        VERIFY(m_parsing_fragment);
}

void HTMLDocumentParser::clear_the_stack_back_to_a_table_body_context()
{
    while (!current_node().local_name().is_one_of(HTML::TagNames::tbody, HTML::TagNames::tfoot, HTML::TagNames::thead, HTML::TagNames::template_, HTML::TagNames::html))
        m_stack_of_open_elements.pop();

    if (current_node().local_name() == HTML::TagNames::html)
        VERIFY(m_parsing_fragment);
}

void HTMLDocumentParser::handle_in_row(HTMLToken& token)
{
    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::th, HTML::TagNames::td)) {
        clear_the_stack_back_to_a_table_row_context();
        insert_html_element(token);
        m_insertion_mode = InsertionMode::InCell;
        m_list_of_active_formatting_elements.add_marker();
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::tr) {
        if (!m_stack_of_open_elements.has_in_table_scope(HTML::TagNames::tr)) {
            log_parse_error();
            return;
        }
        clear_the_stack_back_to_a_table_row_context();
        m_stack_of_open_elements.pop();
        m_insertion_mode = InsertionMode::InTableBody;
        return;
    }

    if ((token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::caption, HTML::TagNames::col, HTML::TagNames::colgroup, HTML::TagNames::tbody, HTML::TagNames::tfoot, HTML::TagNames::thead, HTML::TagNames::tr))
        || (token.is_end_tag() && token.tag_name() == HTML::TagNames::table)) {
        if (!m_stack_of_open_elements.has_in_table_scope(HTML::TagNames::tr)) {
            log_parse_error();
            return;
        }
        clear_the_stack_back_to_a_table_row_context();
        m_stack_of_open_elements.pop();
        m_insertion_mode = InsertionMode::InTableBody;
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::tbody, HTML::TagNames::tfoot, HTML::TagNames::thead)) {
        if (!m_stack_of_open_elements.has_in_table_scope(token.tag_name())) {
            log_parse_error();
            return;
        }
        if (!m_stack_of_open_elements.has_in_table_scope(HTML::TagNames::tr)) {
            return;
        }
        clear_the_stack_back_to_a_table_row_context();
        m_stack_of_open_elements.pop();
        m_insertion_mode = InsertionMode::InTableBody;
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::body, HTML::TagNames::caption, HTML::TagNames::col, HTML::TagNames::colgroup, HTML::TagNames::html, HTML::TagNames::td, HTML::TagNames::th)) {
        log_parse_error();
        return;
    }

    process_using_the_rules_for(InsertionMode::InTable, token);
}

void HTMLDocumentParser::close_the_cell()
{
    generate_implied_end_tags();
    if (!current_node().local_name().is_one_of(HTML::TagNames::td, HTML::TagNames::th)) {
        log_parse_error();
    }
    while (!current_node().local_name().is_one_of(HTML::TagNames::td, HTML::TagNames::th))
        m_stack_of_open_elements.pop();
    m_stack_of_open_elements.pop();
    m_list_of_active_formatting_elements.clear_up_to_the_last_marker();
    m_insertion_mode = InsertionMode::InRow;
}

void HTMLDocumentParser::handle_in_cell(HTMLToken& token)
{
    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::td, HTML::TagNames::th)) {
        if (!m_stack_of_open_elements.has_in_table_scope(token.tag_name())) {
            log_parse_error();
            return;
        }
        generate_implied_end_tags();

        if (current_node().local_name() != token.tag_name()) {
            log_parse_error();
        }

        m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(token.tag_name());

        m_list_of_active_formatting_elements.clear_up_to_the_last_marker();

        m_insertion_mode = InsertionMode::InRow;
        return;
    }
    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::caption, HTML::TagNames::col, HTML::TagNames::colgroup, HTML::TagNames::tbody, HTML::TagNames::td, HTML::TagNames::tfoot, HTML::TagNames::th, HTML::TagNames::thead, HTML::TagNames::tr)) {
        if (!m_stack_of_open_elements.has_in_table_scope(HTML::TagNames::td) && !m_stack_of_open_elements.has_in_table_scope(HTML::TagNames::th)) {
            VERIFY(m_parsing_fragment);
            log_parse_error();
            return;
        }
        close_the_cell();
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::body, HTML::TagNames::caption, HTML::TagNames::col, HTML::TagNames::colgroup, HTML::TagNames::html)) {
        log_parse_error();
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::table, HTML::TagNames::tbody, HTML::TagNames::tfoot, HTML::TagNames::thead, HTML::TagNames::tr)) {
        if (!m_stack_of_open_elements.has_in_table_scope(token.tag_name())) {
            log_parse_error();
            return;
        }
        close_the_cell();
        // Reprocess the token.
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }

    process_using_the_rules_for(InsertionMode::InBody, token);
}

void HTMLDocumentParser::handle_in_table_text(HTMLToken& token)
{
    if (token.is_character()) {
        if (token.code_point() == 0) {
            log_parse_error();
            return;
        }

        m_pending_table_character_tokens.append(token);
        return;
    }

    for (auto& pending_token : m_pending_table_character_tokens) {
        VERIFY(pending_token.is_character());
        if (!pending_token.is_parser_whitespace()) {
            // If any of the tokens in the pending table character tokens list
            // are character tokens that are not ASCII whitespace, then this is a parse error:
            // reprocess the character tokens in the pending table character tokens list using
            // the rules given in the "anything else" entry in the "in table" insertion mode.
            log_parse_error();
            m_foster_parenting = true;
            process_using_the_rules_for(InsertionMode::InBody, token);
            m_foster_parenting = false;
            return;
        }
    }

    for (auto& pending_token : m_pending_table_character_tokens) {
        insert_character(pending_token.code_point());
    }

    m_insertion_mode = m_original_insertion_mode;
    process_using_the_rules_for(m_insertion_mode, token);
}

void HTMLDocumentParser::handle_in_table_body(HTMLToken& token)
{
    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::tr) {
        clear_the_stack_back_to_a_table_body_context();
        insert_html_element(token);
        m_insertion_mode = InsertionMode::InRow;
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::th, HTML::TagNames::td)) {
        log_parse_error();
        clear_the_stack_back_to_a_table_body_context();
        insert_html_element(HTMLToken::make_start_tag(HTML::TagNames::tr));
        m_insertion_mode = InsertionMode::InRow;
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::tbody, HTML::TagNames::tfoot, HTML::TagNames::thead)) {
        if (!m_stack_of_open_elements.has_in_table_scope(token.tag_name())) {
            log_parse_error();
            return;
        }
        clear_the_stack_back_to_a_table_body_context();
        m_stack_of_open_elements.pop();
        m_insertion_mode = InsertionMode::InTable;
        return;
    }

    if ((token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::caption, HTML::TagNames::col, HTML::TagNames::colgroup, HTML::TagNames::tbody, HTML::TagNames::tfoot, HTML::TagNames::thead))
        || (token.is_end_tag() && token.tag_name() == HTML::TagNames::table)) {

        if (!m_stack_of_open_elements.has_in_table_scope(HTML::TagNames::tbody)
            && !m_stack_of_open_elements.has_in_table_scope(HTML::TagNames::thead)
            && !m_stack_of_open_elements.has_in_table_scope(HTML::TagNames::tfoot)) {
            log_parse_error();
            return;
        }

        clear_the_stack_back_to_a_table_body_context();
        m_stack_of_open_elements.pop();
        m_insertion_mode = InsertionMode::InTable;
        process_using_the_rules_for(InsertionMode::InTable, token);
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::body, HTML::TagNames::caption, HTML::TagNames::col, HTML::TagNames::colgroup, HTML::TagNames::html, HTML::TagNames::td, HTML::TagNames::th, HTML::TagNames::tr)) {
        log_parse_error();
        return;
    }

    process_using_the_rules_for(InsertionMode::InTable, token);
}

void HTMLDocumentParser::handle_in_table(HTMLToken& token)
{
    if (token.is_character() && current_node().local_name().is_one_of(HTML::TagNames::table, HTML::TagNames::tbody, HTML::TagNames::tfoot, HTML::TagNames::thead, HTML::TagNames::tr)) {
        m_pending_table_character_tokens.clear();
        m_original_insertion_mode = m_insertion_mode;
        m_insertion_mode = InsertionMode::InTableText;
        process_using_the_rules_for(InsertionMode::InTableText, token);
        return;
    }
    if (token.is_comment()) {
        insert_comment(token);
        return;
    }
    if (token.is_doctype()) {
        log_parse_error();
        return;
    }
    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::caption) {
        clear_the_stack_back_to_a_table_context();
        m_list_of_active_formatting_elements.add_marker();
        insert_html_element(token);
        m_insertion_mode = InsertionMode::InCaption;
        return;
    }
    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::colgroup) {
        clear_the_stack_back_to_a_table_context();
        insert_html_element(token);
        m_insertion_mode = InsertionMode::InColumnGroup;
        return;
    }
    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::col) {
        clear_the_stack_back_to_a_table_context();
        insert_html_element(HTMLToken::make_start_tag(HTML::TagNames::colgroup));
        m_insertion_mode = InsertionMode::InColumnGroup;
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }
    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::tbody, HTML::TagNames::tfoot, HTML::TagNames::thead)) {
        clear_the_stack_back_to_a_table_context();
        insert_html_element(token);
        m_insertion_mode = InsertionMode::InTableBody;
        return;
    }
    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::td, HTML::TagNames::th, HTML::TagNames::tr)) {
        clear_the_stack_back_to_a_table_context();
        insert_html_element(HTMLToken::make_start_tag(HTML::TagNames::tbody));
        m_insertion_mode = InsertionMode::InTableBody;
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }
    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::table) {
        log_parse_error();
        if (!m_stack_of_open_elements.has_in_table_scope(HTML::TagNames::table))
            return;

        m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::table);

        reset_the_insertion_mode_appropriately();
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }
    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::table) {
        if (!m_stack_of_open_elements.has_in_table_scope(HTML::TagNames::table)) {
            log_parse_error();
            return;
        }

        m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::table);

        reset_the_insertion_mode_appropriately();
        return;
    }
    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::body, HTML::TagNames::caption, HTML::TagNames::col, HTML::TagNames::colgroup, HTML::TagNames::html, HTML::TagNames::tbody, HTML::TagNames::td, HTML::TagNames::tfoot, HTML::TagNames::th, HTML::TagNames::thead, HTML::TagNames::tr)) {
        log_parse_error();
        return;
    }
    if ((token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::style, HTML::TagNames::script, HTML::TagNames::template_))
        || (token.is_end_tag() && token.tag_name() == HTML::TagNames::template_)) {
        process_using_the_rules_for(InsertionMode::InHead, token);
        return;
    }
    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::input) {
        auto type_attribute = token.attribute(HTML::AttributeNames::type);
        if (type_attribute.is_null() || !type_attribute.equals_ignoring_case("hidden")) {
            goto AnythingElse;
        }

        log_parse_error();
        insert_html_element(token);

        // FIXME: Is this the correct interpretation of "Pop that input element off the stack of open elements."?
        //        Because this wording is the first time it's seen in the spec.
        //        Other times it's worded as: "Immediately pop the current node off the stack of open elements."
        m_stack_of_open_elements.pop();
        token.acknowledge_self_closing_flag_if_set();
        return;
    }
    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::form) {
        log_parse_error();
        if (m_form_element || m_stack_of_open_elements.contains(HTML::TagNames::template_)) {
            return;
        }

        m_form_element = verify_cast<HTMLFormElement>(*insert_html_element(token));

        // FIXME: See previous FIXME, as this is the same situation but for form.
        m_stack_of_open_elements.pop();
        return;
    }
    if (token.is_end_of_file()) {
        process_using_the_rules_for(InsertionMode::InBody, token);
        return;
    }

AnythingElse:
    log_parse_error();
    m_foster_parenting = true;
    process_using_the_rules_for(InsertionMode::InBody, token);
    m_foster_parenting = false;
}

void HTMLDocumentParser::handle_in_select_in_table(HTMLToken& token)
{
    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::caption, HTML::TagNames::table, HTML::TagNames::tbody, HTML::TagNames::tfoot, HTML::TagNames::thead, HTML::TagNames::tr, HTML::TagNames::td, HTML::TagNames::th)) {
        log_parse_error();
        m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::select);
        reset_the_insertion_mode_appropriately();
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::caption, HTML::TagNames::table, HTML::TagNames::tbody, HTML::TagNames::tfoot, HTML::TagNames::thead, HTML::TagNames::tr, HTML::TagNames::td, HTML::TagNames::th)) {
        log_parse_error();

        if (!m_stack_of_open_elements.has_in_table_scope(token.tag_name()))
            return;

        m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::select);
        reset_the_insertion_mode_appropriately();
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }

    process_using_the_rules_for(InsertionMode::InSelect, token);
}

void HTMLDocumentParser::handle_in_select(HTMLToken& token)
{
    if (token.is_character()) {
        if (token.code_point() == 0) {
            log_parse_error();
            return;
        }
        insert_character(token.code_point());
        return;
    }

    if (token.is_comment()) {
        insert_comment(token);
        return;
    }

    if (token.is_doctype()) {
        log_parse_error();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::html) {
        process_using_the_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::option) {
        if (current_node().local_name() == HTML::TagNames::option) {
            m_stack_of_open_elements.pop();
        }
        insert_html_element(token);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::optgroup) {
        if (current_node().local_name() == HTML::TagNames::option) {
            m_stack_of_open_elements.pop();
        }
        if (current_node().local_name() == HTML::TagNames::optgroup) {
            m_stack_of_open_elements.pop();
        }
        insert_html_element(token);
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::optgroup) {
        if (current_node().local_name() == HTML::TagNames::option && node_before_current_node().local_name() == HTML::TagNames::optgroup)
            m_stack_of_open_elements.pop();

        if (current_node().local_name() == HTML::TagNames::optgroup) {
            m_stack_of_open_elements.pop();
        } else {
            log_parse_error();
            return;
        }
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::option) {
        if (current_node().local_name() == HTML::TagNames::option) {
            m_stack_of_open_elements.pop();
        } else {
            log_parse_error();
            return;
        }
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::select) {
        if (!m_stack_of_open_elements.has_in_select_scope(HTML::TagNames::select)) {
            VERIFY(m_parsing_fragment);
            log_parse_error();
            return;
        }
        m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::select);
        reset_the_insertion_mode_appropriately();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::select) {
        log_parse_error();

        if (!m_stack_of_open_elements.has_in_select_scope(HTML::TagNames::select)) {
            VERIFY(m_parsing_fragment);
            return;
        }

        m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::select);
        reset_the_insertion_mode_appropriately();
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::input, HTML::TagNames::keygen, HTML::TagNames::textarea)) {
        log_parse_error();

        if (!m_stack_of_open_elements.has_in_select_scope(HTML::TagNames::select)) {
            VERIFY(m_parsing_fragment);
            return;
        }

        m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::select);
        reset_the_insertion_mode_appropriately();
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::script, HTML::TagNames::template_)) {
        process_using_the_rules_for(InsertionMode::InHead, token);
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::template_) {
        process_using_the_rules_for(InsertionMode::InHead, token);
        return;
    }

    if (token.is_end_of_file()) {
        process_using_the_rules_for(InsertionMode::InBody, token);
        return;
    }

    log_parse_error();
}

void HTMLDocumentParser::handle_in_caption(HTMLToken& token)
{
    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::caption) {
        if (!m_stack_of_open_elements.has_in_table_scope(HTML::TagNames::caption)) {
            VERIFY(m_parsing_fragment);
            log_parse_error();
            return;
        }

        generate_implied_end_tags();

        if (current_node().local_name() != HTML::TagNames::caption)
            log_parse_error();

        m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::caption);
        m_list_of_active_formatting_elements.clear_up_to_the_last_marker();

        m_insertion_mode = InsertionMode::InTable;
        return;
    }

    if ((token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::caption, HTML::TagNames::col, HTML::TagNames::colgroup, HTML::TagNames::tbody, HTML::TagNames::td, HTML::TagNames::tfoot, HTML::TagNames::th, HTML::TagNames::thead, HTML::TagNames::tr))
        || (token.is_end_tag() && token.tag_name() == HTML::TagNames::table)) {
        if (!m_stack_of_open_elements.has_in_table_scope(HTML::TagNames::caption)) {
            VERIFY(m_parsing_fragment);
            log_parse_error();
            return;
        }

        generate_implied_end_tags();

        if (current_node().local_name() != HTML::TagNames::caption)
            log_parse_error();

        m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::caption);
        m_list_of_active_formatting_elements.clear_up_to_the_last_marker();

        m_insertion_mode = InsertionMode::InTable;
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }

    if (token.is_end_tag() && token.tag_name().is_one_of(HTML::TagNames::body, HTML::TagNames::col, HTML::TagNames::colgroup, HTML::TagNames::html, HTML::TagNames::tbody, HTML::TagNames::td, HTML::TagNames::tfoot, HTML::TagNames::th, HTML::TagNames::thead, HTML::TagNames::tr)) {
        log_parse_error();
        return;
    }

    process_using_the_rules_for(InsertionMode::InBody, token);
}

void HTMLDocumentParser::handle_in_column_group(HTMLToken& token)
{
    if (token.is_character() && token.is_parser_whitespace()) {
        insert_character(token.code_point());
        return;
    }

    if (token.is_comment()) {
        insert_comment(token);
        return;
    }

    if (token.is_doctype()) {
        log_parse_error();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::html) {
        process_using_the_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::col) {
        insert_html_element(token);
        m_stack_of_open_elements.pop();
        token.acknowledge_self_closing_flag_if_set();
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::colgroup) {
        if (current_node().local_name() != HTML::TagNames::colgroup) {
            log_parse_error();
            return;
        }

        m_stack_of_open_elements.pop();
        m_insertion_mode = InsertionMode::InTable;
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::col) {
        log_parse_error();
        return;
    }

    if ((token.is_start_tag() || token.is_end_tag()) && token.tag_name() == HTML::TagNames::template_) {
        process_using_the_rules_for(InsertionMode::InHead, token);
        return;
    }

    if (token.is_end_of_file()) {
        process_using_the_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (current_node().local_name() != HTML::TagNames::colgroup) {
        log_parse_error();
        return;
    }

    m_stack_of_open_elements.pop();
    m_insertion_mode = InsertionMode::InTable;
    process_using_the_rules_for(m_insertion_mode, token);
}

void HTMLDocumentParser::handle_in_template(HTMLToken& token)
{
    if (token.is_character() || token.is_comment() || token.is_doctype()) {
        process_using_the_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::base, HTML::TagNames::basefont, HTML::TagNames::bgsound, HTML::TagNames::link, HTML::TagNames::meta, HTML::TagNames::noframes, HTML::TagNames::script, HTML::TagNames::style, HTML::TagNames::template_, HTML::TagNames::title)) {
        process_using_the_rules_for(InsertionMode::InHead, token);
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::template_) {
        process_using_the_rules_for(InsertionMode::InHead, token);
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::caption, HTML::TagNames::colgroup, HTML::TagNames::tbody, HTML::TagNames::tfoot, HTML::TagNames::thead)) {
        m_stack_of_template_insertion_modes.take_last();
        m_stack_of_template_insertion_modes.append(InsertionMode::InTable);
        m_insertion_mode = InsertionMode::InTable;
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::col) {
        m_stack_of_template_insertion_modes.take_last();
        m_stack_of_template_insertion_modes.append(InsertionMode::InColumnGroup);
        m_insertion_mode = InsertionMode::InColumnGroup;
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::tr) {
        m_stack_of_template_insertion_modes.take_last();
        m_stack_of_template_insertion_modes.append(InsertionMode::InTableBody);
        m_insertion_mode = InsertionMode::InTableBody;
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }

    if (token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::td, HTML::TagNames::th)) {
        m_stack_of_template_insertion_modes.take_last();
        m_stack_of_template_insertion_modes.append(InsertionMode::InRow);
        m_insertion_mode = InsertionMode::InRow;
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }

    if (token.is_start_tag()) {
        m_stack_of_template_insertion_modes.take_last();
        m_stack_of_template_insertion_modes.append(InsertionMode::InBody);
        m_insertion_mode = InsertionMode::InBody;
        process_using_the_rules_for(m_insertion_mode, token);
        return;
    }

    if (token.is_end_tag()) {
        log_parse_error();
        return;
    }

    if (token.is_end_of_file()) {
        if (!m_stack_of_open_elements.contains(HTML::TagNames::template_)) {
            VERIFY(m_parsing_fragment);
            stop_parsing();
            return;
        }

        log_parse_error();
        m_stack_of_open_elements.pop_until_an_element_with_tag_name_has_been_popped(HTML::TagNames::template_);
        m_list_of_active_formatting_elements.clear_up_to_the_last_marker();
        m_stack_of_template_insertion_modes.take_last();
        reset_the_insertion_mode_appropriately();
        process_using_the_rules_for(m_insertion_mode, token);
    }
}

void HTMLDocumentParser::handle_in_frameset(HTMLToken& token)
{
    if (token.is_character() && token.is_parser_whitespace()) {
        insert_character(token.code_point());
        return;
    }

    if (token.is_comment()) {
        insert_comment(token);
        return;
    }

    if (token.is_doctype()) {
        log_parse_error();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::html) {
        process_using_the_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::frameset) {
        insert_html_element(token);
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::frameset) {
        // FIXME: If the current node is the root html element, then this is a parse error; ignore the token. (fragment case)

        m_stack_of_open_elements.pop();

        if (!m_parsing_fragment && current_node().local_name() != HTML::TagNames::frameset) {
            m_insertion_mode = InsertionMode::AfterFrameset;
        }
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::frame) {
        insert_html_element(token);
        m_stack_of_open_elements.pop();
        token.acknowledge_self_closing_flag_if_set();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::noframes) {
        process_using_the_rules_for(InsertionMode::InHead, token);
        return;
    }

    if (token.is_end_of_file()) {
        //FIXME: If the current node is not the root html element, then this is a parse error.

        stop_parsing();
        return;
    }

    log_parse_error();
}

void HTMLDocumentParser::handle_after_frameset(HTMLToken& token)
{
    if (token.is_character() && token.is_parser_whitespace()) {
        insert_character(token.code_point());
        return;
    }

    if (token.is_comment()) {
        insert_comment(token);
        return;
    }

    if (token.is_doctype()) {
        log_parse_error();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::html) {
        process_using_the_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (token.is_end_tag() && token.tag_name() == HTML::TagNames::html) {
        m_insertion_mode = InsertionMode::AfterAfterFrameset;
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::noframes) {
        process_using_the_rules_for(InsertionMode::InHead, token);
        return;
    }

    if (token.is_end_of_file()) {
        stop_parsing();
        return;
    }

    log_parse_error();
}

void HTMLDocumentParser::handle_after_after_frameset(HTMLToken& token)
{
    if (token.is_comment()) {
        auto comment = adopt_ref(*new DOM::Comment(document(), token.comment()));
        document().append_child(move(comment));
        return;
    }

    if (token.is_doctype() || token.is_parser_whitespace() || (token.is_start_tag() && token.tag_name() == HTML::TagNames::html)) {
        process_using_the_rules_for(InsertionMode::InBody, token);
        return;
    }

    if (token.is_end_of_file()) {
        stop_parsing();
        return;
    }

    if (token.is_start_tag() && token.tag_name() == HTML::TagNames::noframes) {
        process_using_the_rules_for(InsertionMode::InHead, token);
        return;
    }

    log_parse_error();
}

void HTMLDocumentParser::process_using_the_rules_for_foreign_content(HTMLToken& token)
{
    if (token.is_character()) {
        if (token.code_point() == 0) {
            log_parse_error();
            insert_character(0xFFFD);
            return;
        }
        if (token.is_parser_whitespace()) {
            insert_character(token.code_point());
            return;
        }
        insert_character(token.code_point());
        m_frameset_ok = false;
        return;
    }

    if (token.is_comment()) {
        insert_comment(token);
        return;
    }

    if (token.is_doctype()) {
        log_parse_error();
        return;
    }

    if ((token.is_start_tag() && token.tag_name().is_one_of(HTML::TagNames::b, HTML::TagNames::big, HTML::TagNames::blockquote, HTML::TagNames::body, HTML::TagNames::br, HTML::TagNames::center, HTML::TagNames::code, HTML::TagNames::dd, HTML::TagNames::div, HTML::TagNames::dl, HTML::TagNames::dt, HTML::TagNames::em, HTML::TagNames::embed, HTML::TagNames::h1, HTML::TagNames::h2, HTML::TagNames::h3, HTML::TagNames::h4, HTML::TagNames::h5, HTML::TagNames::h6, HTML::TagNames::head, HTML::TagNames::hr, HTML::TagNames::i, HTML::TagNames::img, HTML::TagNames::li, HTML::TagNames::listing, HTML::TagNames::menu, HTML::TagNames::meta, HTML::TagNames::nobr, HTML::TagNames::ol, HTML::TagNames::p, HTML::TagNames::pre, HTML::TagNames::ruby, HTML::TagNames::s, HTML::TagNames::small, HTML::TagNames::span, HTML::TagNames::strong, HTML::TagNames::strike, HTML::TagNames::sub, HTML::TagNames::sup, HTML::TagNames::table, HTML::TagNames::tt, HTML::TagNames::u, HTML::TagNames::ul, HTML::TagNames::var))
        || (token.is_start_tag() && token.tag_name() == HTML::TagNames::font && (token.has_attribute(HTML::AttributeNames::color) || token.has_attribute(HTML::AttributeNames::face) || token.has_attribute(HTML::AttributeNames::size)))) {
        log_parse_error();
        if (m_parsing_fragment) {
            goto AnyOtherStartTag;
        }

        TODO();
    }

    if (token.is_start_tag()) {
    AnyOtherStartTag:
        if (adjusted_current_node().namespace_() == Namespace::MathML) {
            adjust_mathml_attributes(token);
        } else if (adjusted_current_node().namespace_() == Namespace::SVG) {
            adjust_svg_tag_names(token);
            adjust_svg_attributes(token);
        }

        adjust_foreign_attributes(token);
        insert_foreign_element(token, adjusted_current_node().namespace_());

        if (token.is_self_closing()) {
            if (token.tag_name() == SVG::TagNames::script && current_node().namespace_() == Namespace::SVG) {
                token.acknowledge_self_closing_flag_if_set();
                goto ScriptEndTag;
            }

            m_stack_of_open_elements.pop();
            token.acknowledge_self_closing_flag_if_set();
        }

        return;
    }

    if (token.is_end_tag() && current_node().namespace_() == Namespace::SVG && current_node().tag_name() == SVG::TagNames::script) {
    ScriptEndTag:
        m_stack_of_open_elements.pop();
        TODO();
    }

    if (token.is_end_tag()) {
        RefPtr<DOM::Element> node = current_node();
        // FIXME: Not sure if this is the correct to_lowercase, as the specification says "to ASCII lowercase"
        if (node->tag_name().to_lowercase() != token.tag_name())
            log_parse_error();
        for (ssize_t i = m_stack_of_open_elements.elements().size() - 1; i >= 0; --i) {
            if (node == m_stack_of_open_elements.first()) {
                VERIFY(m_parsing_fragment);
                return;
            }
            // FIXME: See the above FIXME
            if (node->tag_name().to_lowercase() == token.tag_name()) {
                while (current_node() != node)
                    m_stack_of_open_elements.pop();
                m_stack_of_open_elements.pop();
                return;
            }

            node = m_stack_of_open_elements.elements().at(i - 1);

            if (node->namespace_() != Namespace::HTML)
                continue;

            process_using_the_rules_for(m_insertion_mode, token);
            return;
        }
    }

    VERIFY_NOT_REACHED();
}

// https://html.spec.whatwg.org/multipage/parsing.html#reset-the-insertion-mode-appropriately
void HTMLDocumentParser::reset_the_insertion_mode_appropriately()
{
    for (ssize_t i = m_stack_of_open_elements.elements().size() - 1; i >= 0; --i) {
        bool last = i == 0;
        // NOTE: When parsing fragments, we substitute the context element for the root of the stack of open elements.
        RefPtr<DOM::Element> node;
        if (last && m_parsing_fragment) {
            node = m_context_element;
        } else {
            node = m_stack_of_open_elements.elements().at(i);
        }

        if (node->local_name() == HTML::TagNames::select) {
            if (!last) {
                for (ssize_t j = i; j > 0; --j) {
                    auto& ancestor = m_stack_of_open_elements.elements().at(j - 1);

                    if (is<HTMLTemplateElement>(ancestor))
                        break;

                    if (is<HTMLTableElement>(ancestor)) {
                        m_insertion_mode = InsertionMode::InSelectInTable;
                        return;
                    }
                }
            }

            m_insertion_mode = InsertionMode::InSelect;
            return;
        }

        if (!last && node->local_name().is_one_of(HTML::TagNames::td, HTML::TagNames::th)) {
            m_insertion_mode = InsertionMode::InCell;
            return;
        }

        if (node->local_name() == HTML::TagNames::tr) {
            m_insertion_mode = InsertionMode::InRow;
            return;
        }

        if (node->local_name().is_one_of(HTML::TagNames::tbody, HTML::TagNames::thead, HTML::TagNames::tfoot)) {
            m_insertion_mode = InsertionMode::InTableBody;
            return;
        }

        if (node->local_name() == HTML::TagNames::caption) {
            m_insertion_mode = InsertionMode::InCaption;
            return;
        }

        if (node->local_name() == HTML::TagNames::colgroup) {
            m_insertion_mode = InsertionMode::InColumnGroup;
            return;
        }

        if (node->local_name() == HTML::TagNames::table) {
            m_insertion_mode = InsertionMode::InTable;
            return;
        }

        if (node->local_name() == HTML::TagNames::template_) {
            m_insertion_mode = m_stack_of_template_insertion_modes.last();
            return;
        }

        if (!last && node->local_name() == HTML::TagNames::head) {
            m_insertion_mode = InsertionMode::InHead;
            return;
        }

        if (node->local_name() == HTML::TagNames::body) {
            m_insertion_mode = InsertionMode::InBody;
            return;
        }

        if (node->local_name() == HTML::TagNames::frameset) {
            VERIFY(m_parsing_fragment);
            m_insertion_mode = InsertionMode::InFrameset;
            return;
        }

        if (node->local_name() == HTML::TagNames::html) {
            if (!m_head_element) {
                VERIFY(m_parsing_fragment);
                m_insertion_mode = InsertionMode::BeforeHead;
                return;
            }

            m_insertion_mode = InsertionMode::AfterHead;
            return;
        }
    }

    VERIFY(m_parsing_fragment);
    m_insertion_mode = InsertionMode::InBody;
}

const char* HTMLDocumentParser::insertion_mode_name() const
{
    switch (m_insertion_mode) {
#define __ENUMERATE_INSERTION_MODE(mode) \
    case InsertionMode::mode:            \
        return #mode;
        ENUMERATE_INSERTION_MODES
#undef __ENUMERATE_INSERTION_MODE
    }
    VERIFY_NOT_REACHED();
}

DOM::Document& HTMLDocumentParser::document()
{
    return *m_document;
}

NonnullRefPtrVector<DOM::Node> HTMLDocumentParser::parse_html_fragment(DOM::Element& context_element, const StringView& markup)
{
    auto temp_document = DOM::Document::create();
    HTMLDocumentParser parser(*temp_document, markup, "utf-8");
    parser.m_context_element = context_element;
    parser.m_parsing_fragment = true;
    parser.document().set_quirks_mode(context_element.document().mode());

    if (context_element.local_name().is_one_of(HTML::TagNames::title, HTML::TagNames::textarea)) {
        parser.m_tokenizer.switch_to({}, HTMLTokenizer::State::RCDATA);
    } else if (context_element.local_name().is_one_of(HTML::TagNames::style, HTML::TagNames::xmp, HTML::TagNames::iframe, HTML::TagNames::noembed, HTML::TagNames::noframes)) {
        parser.m_tokenizer.switch_to({}, HTMLTokenizer::State::RAWTEXT);
    } else if (context_element.local_name().is_one_of(HTML::TagNames::script)) {
        parser.m_tokenizer.switch_to({}, HTMLTokenizer::State::ScriptData);
    } else if (context_element.local_name().is_one_of(HTML::TagNames::noscript)) {
        if (context_element.document().is_scripting_enabled())
            parser.m_tokenizer.switch_to({}, HTMLTokenizer::State::RAWTEXT);
    } else if (context_element.local_name().is_one_of(HTML::TagNames::plaintext)) {
        parser.m_tokenizer.switch_to({}, HTMLTokenizer::State::PLAINTEXT);
    }

    auto root = create_element(context_element.document(), HTML::TagNames::html, Namespace::HTML);
    parser.document().append_child(root);
    parser.m_stack_of_open_elements.push(root);

    if (context_element.local_name() == HTML::TagNames::template_) {
        parser.m_stack_of_template_insertion_modes.append(InsertionMode::InTemplate);
    }

    // FIXME: Create a start tag token whose name is the local name of context and whose attributes are the attributes of context.

    parser.reset_the_insertion_mode_appropriately();

    for (auto* form_candidate = &context_element; form_candidate; form_candidate = form_candidate->parent_element()) {
        if (is<HTMLFormElement>(*form_candidate)) {
            parser.m_form_element = verify_cast<HTMLFormElement>(*form_candidate);
            break;
        }
    }

    parser.run(context_element.document().url());

    NonnullRefPtrVector<DOM::Node> children;
    while (RefPtr<DOM::Node> child = root->first_child()) {
        root->remove_child(*child);
        context_element.document().adopt_node(*child);
        children.append(*child);
    }
    return children;
}

NonnullOwnPtr<HTMLDocumentParser> HTMLDocumentParser::create_with_uncertain_encoding(DOM::Document& document, const ByteBuffer& input)
{
    if (document.has_encoding())
        return make<HTMLDocumentParser>(document, input, document.encoding().value());
    auto encoding = run_encoding_sniffing_algorithm(input);
    dbgln("The encoding sniffing algorithm returned encoding '{}'", encoding);
    return make<HTMLDocumentParser>(document, input, encoding);
}

}
