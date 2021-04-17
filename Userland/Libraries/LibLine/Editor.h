/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, the SerenityOS developers.
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

#pragma once

#include <AK/BinarySearch.h>
#include <AK/ByteBuffer.h>
#include <AK/Function.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/QuickSort.h>
#include <AK/Result.h>
#include <AK/String.h>
#include <AK/Traits.h>
#include <AK/Utf32View.h>
#include <AK/Utf8View.h>
#include <AK/Vector.h>
#include <LibCore/DirIterator.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Notifier.h>
#include <LibCore/Object.h>
#include <LibLine/KeyCallbackMachine.h>
#include <LibLine/Span.h>
#include <LibLine/StringMetrics.h>
#include <LibLine/Style.h>
#include <LibLine/SuggestionDisplay.h>
#include <LibLine/SuggestionManager.h>
#include <LibLine/VT.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>

namespace Line {

struct KeyBinding {
    Vector<Key> keys;
    enum class Kind {
        InternalFunction,
        Insertion,
    } kind { Kind::InternalFunction };
    String binding;
};

struct Configuration {
    enum RefreshBehaviour {
        Lazy,
        Eager,
    };
    enum OperationMode {
        Unset,
        Full,
        NoEscapeSequences,
        NonInteractive,
    };
    enum SignalHandler {
        WithSignalHandlers,
        NoSignalHandlers,
    };

    struct DefaultTextEditor {
        String command;
    };

    Configuration()
    {
    }

    template<typename Arg, typename... Rest>
    Configuration(Arg arg, Rest... rest)
        : Configuration(rest...)
    {
        set(arg);
    }

    void set(RefreshBehaviour refresh) { refresh_behaviour = refresh; }
    void set(OperationMode mode) { operation_mode = mode; }
    void set(SignalHandler mode) { m_signal_mode = mode; }
    void set(const KeyBinding& binding) { keybindings.append(binding); }
    void set(DefaultTextEditor editor) { m_default_text_editor = move(editor.command); }

    static Configuration from_config(const StringView& libname = "line");

    RefreshBehaviour refresh_behaviour { RefreshBehaviour::Lazy };
    SignalHandler m_signal_mode { SignalHandler::WithSignalHandlers };
    OperationMode operation_mode { OperationMode::Unset };
    Vector<KeyBinding> keybindings;
    String m_default_text_editor {};
};

#define ENUMERATE_EDITOR_INTERNAL_FUNCTIONS(M) \
    M(clear_screen)                            \
    M(cursor_left_character)                   \
    M(cursor_left_word)                        \
    M(cursor_right_character)                  \
    M(cursor_right_word)                       \
    M(enter_search)                            \
    M(erase_character_backwards)               \
    M(erase_character_forwards)                \
    M(erase_to_beginning)                      \
    M(erase_to_end)                            \
    M(erase_word_backwards)                    \
    M(finish_edit)                             \
    M(go_end)                                  \
    M(go_home)                                 \
    M(kill_line)                               \
    M(search_backwards)                        \
    M(search_forwards)                         \
    M(transpose_characters)                    \
    M(transpose_words)                         \
    M(insert_last_words)                       \
    M(erase_alnum_word_backwards)              \
    M(erase_alnum_word_forwards)               \
    M(capitalize_word)                         \
    M(lowercase_word)                          \
    M(uppercase_word)                          \
    M(edit_in_external_editor)

#define EDITOR_INTERNAL_FUNCTION(name) \
    [](auto& editor) { editor.name();  return false; }

class Editor : public Core::Object {
    C_OBJECT(Editor);

public:
    enum class Error {
        ReadFailure,
        Empty,
        Eof,
    };

    ~Editor();

    Result<String, Error> get_line(const String& prompt);

    void initialize();

    void add_to_history(const String& line);
    bool load_history(const String& path);
    bool save_history(const String& path);
    const auto& history() const { return m_history; }

    void register_key_input_callback(const KeyBinding&);
    void register_key_input_callback(Vector<Key> keys, Function<bool(Editor&)> callback) { m_callback_machine.register_key_input_callback(move(keys), move(callback)); }
    void register_key_input_callback(Key key, Function<bool(Editor&)> callback) { register_key_input_callback(Vector<Key> { key }, move(callback)); }

    static StringMetrics actual_rendered_string_metrics(const StringView&);
    static StringMetrics actual_rendered_string_metrics(const Utf32View&);

    Function<Vector<CompletionSuggestion>(const Editor&)> on_tab_complete;
    Function<void()> on_interrupt_handled;
    Function<void(Editor&)> on_display_refresh;

    static Function<bool(Editor&)> find_internal_function(const StringView& name);
    enum class CaseChangeOp {
        Lowercase,
        Uppercase,
        Capital,
    };
    void case_change_word(CaseChangeOp);
#define __ENUMERATE_EDITOR_INTERNAL_FUNCTION(name) \
    void name();

    ENUMERATE_EDITOR_INTERNAL_FUNCTIONS(__ENUMERATE_EDITOR_INTERNAL_FUNCTION)

#undef __ENUMERATE_EDITOR_INTERNAL_FUNCTION

    void interrupted();
    void resized()
    {
        m_was_resized = true;
        m_previous_num_columns = m_num_columns;
        get_terminal_size();
        m_suggestion_display->set_vt_size(m_num_lines, m_num_columns);
        if (m_is_searching)
            m_search_editor->resized();
    }

    size_t cursor() const { return m_cursor; }
    void set_cursor(size_t cursor)
    {
        if (cursor > m_buffer.size())
            cursor = m_buffer.size();
        m_cursor = cursor;
    }
    const Vector<u32, 1024>& buffer() const { return m_buffer; }
    u32 buffer_at(size_t pos) const { return m_buffer.at(pos); }
    String line() const { return line(m_buffer.size()); }
    String line(size_t up_to_index) const;

    // Only makes sense inside a character_input callback or on_* callback.
    void set_prompt(const String& prompt)
    {
        if (m_cached_prompt_valid)
            m_old_prompt_metrics = m_cached_prompt_metrics;
        m_cached_prompt_valid = false;
        m_cached_prompt_metrics = actual_rendered_string_metrics(prompt);
        m_new_prompt = prompt;
    }

    void clear_line();
    void insert(const String&);
    void insert(const StringView&);
    void insert(const Utf32View&);
    void insert(const u32);
    void stylize(const Span&, const Style&);
    void strip_styles(bool strip_anchored = false);

    // Invariant Offset is an offset into the suggested data, hinting the editor what parts of the suggestion will not change
    // Static Offset is an offset into the token, signifying where the suggestions start
    // e.g.
    //    foobar<suggestion initiated>, on_tab_complete returns "barx", "bary", "barz"
    //       ^ ^
    //       +-|- static offset: the suggestions start here
    //         +- invariant offset: the suggestions do not change up to here
    //
    void suggest(size_t invariant_offset = 0, size_t static_offset = 0, Span::Mode offset_mode = Span::ByteOriented) const;

    const struct termios& termios() const { return m_termios; }
    const struct termios& default_termios() const { return m_default_termios; }
    struct winsize terminal_size() const
    {
        winsize ws { (u16)m_num_lines, (u16)m_num_columns, 0, 0 };
        return ws;
    }

    void finish()
    {
        m_finish = true;
    }

    bool is_editing() const { return m_is_editing; }

    const Utf32View buffer_view() const { return { m_buffer.data(), m_buffer.size() }; }

private:
    explicit Editor(Configuration configuration = Configuration::from_config());

    void set_default_keybinds();

    enum VTState {
        Free = 1,
        Escape = 3,
        Bracket = 5,
        BracketArgsSemi = 7,
        Title = 9,
    };

    static VTState actual_rendered_string_length_step(StringMetrics&, size_t, StringMetrics::LineMetrics& current_line, u32, u32, VTState);

    enum LoopExitCode {
        Exit = 0,
        Retry
    };

    // FIXME: Port to Core::Property
    void save_to(JsonObject&);

    void try_update_once();
    void handle_interrupt_event();
    void handle_read_event();

    Vector<size_t, 2> vt_dsr();
    void remove_at_index(size_t);

    enum class ModificationKind {
        Insertion,
        Removal,
        ForcedOverlapRemoval,
    };
    void readjust_anchored_styles(size_t hint_index, ModificationKind);

    Style find_applicable_style(size_t offset) const;

    bool search(const StringView&, bool allow_empty = false, bool from_beginning = true);
    inline void end_search()
    {
        m_is_searching = false;
        m_refresh_needed = true;
        m_search_offset = 0;
        if (m_reset_buffer_on_search_end) {
            m_buffer.clear();
            for (auto ch : m_pre_search_buffer)
                m_buffer.append(ch);
            m_cursor = m_pre_search_cursor;
        }
        m_reset_buffer_on_search_end = true;
        m_search_editor = nullptr;
    }

    void reset()
    {
        m_cached_buffer_metrics.reset();
        m_cached_prompt_valid = false;
        m_cursor = 0;
        m_drawn_cursor = 0;
        m_inline_search_cursor = 0;
        m_search_offset = 0;
        m_search_offset_state = SearchOffsetState::Unbiased;
        m_old_prompt_metrics = m_cached_prompt_metrics;
        set_origin(0, 0);
        m_prompt_lines_at_suggestion_initiation = 0;
        m_refresh_needed = true;
        m_input_error.clear();
        m_returned_line = String::empty();
        m_chars_touched_in_the_middle = 0;
        m_drawn_end_of_line_offset = 0;
        m_drawn_spans = {};
    }

    void refresh_display();
    void cleanup();
    void cleanup_suggestions();
    void really_quit_event_loop();

    void restore()
    {
        VERIFY(m_initialized);
        tcsetattr(0, TCSANOW, &m_default_termios);
        m_initialized = false;
        for (auto id : m_signal_handlers)
            Core::EventLoop::unregister_signal(id);
    }

    const StringMetrics& current_prompt_metrics() const
    {
        return m_cached_prompt_valid ? m_cached_prompt_metrics : m_old_prompt_metrics;
    }

    size_t num_lines() const
    {
        return current_prompt_metrics().lines_with_addition(m_cached_buffer_metrics, m_num_columns);
    }

    size_t cursor_line() const
    {
        auto cursor = m_drawn_cursor;
        if (cursor > m_cursor)
            cursor = m_cursor;
        return current_prompt_metrics().lines_with_addition(
            actual_rendered_string_metrics(buffer_view().substring_view(0, cursor)),
            m_num_columns);
    }

    size_t offset_in_line() const
    {
        auto cursor = m_drawn_cursor;
        if (cursor > m_cursor)
            cursor = m_cursor;
        auto buffer_metrics = actual_rendered_string_metrics(buffer_view().substring_view(0, cursor));
        return current_prompt_metrics().offset_with_addition(buffer_metrics, m_num_columns);
    }

    void set_origin()
    {
        auto position = vt_dsr();
        set_origin(position[0], position[1]);
    }

    void set_origin(int row, int col)
    {
        m_origin_row = row;
        m_origin_column = col;
        m_suggestion_display->set_origin(row, col, {});
    }

    void recalculate_origin();
    void reposition_cursor(bool to_end = false);

    struct CodepointRange {
        size_t start { 0 };
        size_t end { 0 };
    };
    CodepointRange byte_offset_range_to_code_point_offset_range(size_t byte_start, size_t byte_end, size_t code_point_scan_offset, bool reverse = false) const;

    void get_terminal_size();

    bool m_finish { false };

    RefPtr<Editor> m_search_editor;
    bool m_is_searching { false };
    bool m_reset_buffer_on_search_end { true };
    size_t m_search_offset { 0 };
    enum class SearchOffsetState {
        Unbiased,
        Backwards,
        Forwards,
    } m_search_offset_state { SearchOffsetState::Unbiased };
    size_t m_pre_search_cursor { 0 };
    Vector<u32, 1024> m_pre_search_buffer;

    Vector<u32, 1024> m_buffer;
    ByteBuffer m_pending_chars;
    Vector<char, 512> m_incomplete_data;
    Optional<Error> m_input_error;
    String m_returned_line;

    size_t m_cursor { 0 };
    size_t m_drawn_cursor { 0 };
    size_t m_drawn_end_of_line_offset { 0 };
    size_t m_inline_search_cursor { 0 };
    size_t m_chars_touched_in_the_middle { 0 };
    size_t m_times_tab_pressed { 0 };
    size_t m_num_columns { 0 };
    size_t m_num_lines { 1 };
    size_t m_previous_num_columns { 0 };
    size_t m_extra_forward_lines { 0 };
    StringMetrics m_cached_prompt_metrics;
    StringMetrics m_old_prompt_metrics;
    StringMetrics m_cached_buffer_metrics;
    size_t m_prompt_lines_at_suggestion_initiation { 0 };
    bool m_cached_prompt_valid { false };

    // Exact position before our prompt in the terminal.
    size_t m_origin_row { 0 };
    size_t m_origin_column { 0 };

    OwnPtr<SuggestionDisplay> m_suggestion_display;

    String m_new_prompt;

    SuggestionManager m_suggestion_manager;

    bool m_always_refresh { false };

    enum class TabDirection {
        Forward,
        Backward,
    };
    TabDirection m_tab_direction { TabDirection::Forward };

    KeyCallbackMachine m_callback_machine;

    struct termios m_termios {
    };
    struct termios m_default_termios {
    };
    bool m_was_interrupted { false };
    bool m_previous_interrupt_was_handled_as_interrupt { true };
    bool m_was_resized { false };

    // FIXME: This should be something more take_first()-friendly.
    struct HistoryEntry {
        String entry;
        time_t timestamp;
    };
    Vector<HistoryEntry> m_history;
    size_t m_history_cursor { 0 };
    size_t m_history_capacity { 1024 };

    enum class InputState {
        Free,
        Verbatim,
        GotEscape,
        CSIExpectParameter,
        CSIExpectIntermediate,
        CSIExpectFinal,
    };
    InputState m_state { InputState::Free };

    struct Spans {
        HashMap<u32, HashMap<u32, Style>> m_spans_starting;
        HashMap<u32, HashMap<u32, Style>> m_spans_ending;
        HashMap<u32, HashMap<u32, Style>> m_anchored_spans_starting;
        HashMap<u32, HashMap<u32, Style>> m_anchored_spans_ending;

        bool contains_up_to_offset(const Spans& other, size_t offset) const;
    } m_drawn_spans, m_current_spans;

    RefPtr<Core::Notifier> m_notifier;

    bool m_initialized { false };
    bool m_refresh_needed { false };
    Vector<int, 2> m_signal_handlers;

    bool m_is_editing { false };

    Configuration m_configuration;
};

}
