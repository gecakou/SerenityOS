/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Badge.h>
#include <AK/Function.h>
#include <AK/HashMap.h>
#include <AK/HashTable.h>
#include <AK/RefCounted.h>
#include <AK/String.h>
#include <AK/WeakPtr.h>
#include <LibCore/MimeData.h>
#include <LibGUI/Forward.h>
#include <LibGUI/ModelIndex.h>
#include <LibGUI/ModelRole.h>
#include <LibGUI/Variant.h>
#include <LibGfx/Forward.h>
#include <LibGfx/TextAlignment.h>

namespace GUI {

enum class SortOrder {
    None,
    Ascending,
    Descending
};

class ModelClient {
public:
    virtual ~ModelClient() { }

    virtual void model_did_update(unsigned flags) = 0;

    virtual void model_did_insert_rows([[maybe_unused]] ModelIndex const& parent, [[maybe_unused]] int first, [[maybe_unused]] int last) { }
    virtual void model_did_insert_columns([[maybe_unused]] ModelIndex const& parent, [[maybe_unused]] int first, [[maybe_unused]] int last) { }
    virtual void model_did_move_rows([[maybe_unused]] ModelIndex const& source_parent, [[maybe_unused]] int first, [[maybe_unused]] int last, [[maybe_unused]] ModelIndex const& target_parent, [[maybe_unused]] int target_index) { }
    virtual void model_did_move_columns([[maybe_unused]] ModelIndex const& source_parent, [[maybe_unused]] int first, [[maybe_unused]] int last, [[maybe_unused]] ModelIndex const& target_parent, [[maybe_unused]] int target_index) { }
    virtual void model_did_delete_rows([[maybe_unused]] ModelIndex const& parent, [[maybe_unused]] int first, [[maybe_unused]] int last) { }
    virtual void model_did_delete_columns([[maybe_unused]] ModelIndex const& parent, [[maybe_unused]] int first, [[maybe_unused]] int last) { }
};

class Model : public RefCounted<Model> {
public:
    enum UpdateFlag {
        DontInvalidateIndices = 0,
        InvalidateAllIndices = 1 << 0,
    };

    enum MatchesFlag {
        AllMatching = 0,
        FirstMatchOnly = 1 << 0,
        CaseInsensitive = 1 << 1,
        MatchAtStart = 1 << 2,
        MatchFull = 1 << 3,
    };

    virtual ~Model();

    virtual int row_count(const ModelIndex& = ModelIndex()) const = 0;
    virtual int column_count(const ModelIndex& = ModelIndex()) const = 0;
    virtual String column_name(int) const { return {}; }
    virtual Variant data(const ModelIndex&, ModelRole = ModelRole::Display) const = 0;
    virtual TriState data_matches(const ModelIndex&, const Variant&) const { return TriState::Unknown; }
    virtual void invalidate();
    virtual ModelIndex parent_index(const ModelIndex&) const { return {}; }
    virtual ModelIndex index(int row, int column = 0, const ModelIndex& parent = ModelIndex()) const;
    virtual bool is_editable(const ModelIndex&) const { return false; }
    virtual bool is_searchable() const { return false; }
    virtual void set_data(const ModelIndex&, const Variant&) { }
    virtual int tree_column() const { return 0; }
    virtual bool accepts_drag(const ModelIndex&, const Vector<String>& mime_types) const;
    virtual Vector<ModelIndex, 1> matches(const StringView&, unsigned = MatchesFlag::AllMatching, const ModelIndex& = ModelIndex()) { return {}; }

    virtual bool is_column_sortable([[maybe_unused]] int column_index) const { return true; }
    virtual void sort([[maybe_unused]] int column, SortOrder) { }

    bool is_within_range(const ModelIndex& index) const
    {
        auto parent_index = this->parent_index(index);
        return index.row() >= 0 && index.row() < row_count(parent_index) && index.column() >= 0 && index.column() < column_count(parent_index);
    }

    virtual StringView drag_data_type() const { return {}; }
    virtual RefPtr<Core::MimeData> mime_data(const ModelSelection&) const;

    void register_view(Badge<AbstractView>, AbstractView&);
    void unregister_view(Badge<AbstractView>, AbstractView&);

    void register_client(ModelClient&);
    void unregister_client(ModelClient&);

    WeakPtr<PersistentHandle> register_persistent_index(Badge<PersistentModelIndex>, ModelIndex const&);

protected:
    Model();

    void for_each_view(Function<void(AbstractView&)>);
    void for_each_client(Function<void(ModelClient&)>);
    void did_update(unsigned flags = UpdateFlag::InvalidateAllIndices);

    static bool string_matches(const StringView& str, const StringView& needle, unsigned flags)
    {
        auto case_sensitivity = (flags & CaseInsensitive) ? CaseSensitivity::CaseInsensitive : CaseSensitivity::CaseSensitive;
        if (flags & MatchFull)
            return str.length() == needle.length() && str.starts_with(needle, case_sensitivity);
        if (flags & MatchAtStart)
            return str.starts_with(needle, case_sensitivity);
        return str.contains(needle, case_sensitivity);
    }

    ModelIndex create_index(int row, int column, const void* data = nullptr) const;

    void begin_insert_rows(ModelIndex const& parent, int first, int last);
    void begin_insert_columns(ModelIndex const& parent, int first, int last);
    void begin_move_rows(ModelIndex const& source_parent, int first, int last, ModelIndex const& target_parent, int target_index);
    void begin_move_columns(ModelIndex const& source_parent, int first, int last, ModelIndex const& target_parent, int target_index);
    void begin_delete_rows(ModelIndex const& parent, int first, int last);
    void begin_delete_columns(ModelIndex const& parent, int first, int last);

    void end_insert_rows();
    void end_insert_columns();
    void end_move_rows();
    void end_move_columns();
    void end_delete_rows();
    void end_delete_columns();

    void change_persistent_index_list(Vector<ModelIndex> const& old_indices, Vector<ModelIndex> const& new_indices);

private:
    enum class OperationType {
        Invalid = 0,
        Insert,
        Move,
        Delete,
        Reset
    };
    enum class Direction {
        Row,
        Column
    };

    struct Operation {
        OperationType type { OperationType::Invalid };
        Direction direction { Direction::Row };
        ModelIndex source_parent;
        int first { 0 };
        int last { 0 };
        ModelIndex target_parent;
        int target { 0 };

        Operation(OperationType type)
            : type(type)
        {
        }

        Operation(OperationType type, Direction direction, ModelIndex const& parent, int first, int last)
            : type(type)
            , direction(direction)
            , source_parent(parent)
            , first(first)
            , last(last)
        {
        }

        Operation(OperationType type, Direction direction, ModelIndex const& source_parent, int first, int last, ModelIndex const& target_parent, int target)
            : type(type)
            , direction(direction)
            , source_parent(source_parent)
            , first(first)
            , last(last)
            , target_parent(target_parent)
            , target(target)
        {
        }
    };

    void handle_insert(Operation const&);
    void handle_move(Operation const&);
    void handle_delete(Operation const&);

    template<bool IsRow>
    void save_deleted_indices(ModelIndex const& parent, int first, int last);

    HashMap<ModelIndex, OwnPtr<PersistentHandle>> m_persistent_handles;
    Vector<Operation> m_operation_stack;
    // NOTE: We need to save which indices have been deleted before the delete
    // actually happens, because we can't figure out which persistent handles
    // belong to us in end_delete_rows/columns (because accessing the parents of
    // the indices might be impossible).
    Vector<Vector<ModelIndex>> m_deleted_indices_stack;

    HashTable<AbstractView*> m_views;
    HashTable<ModelClient*> m_clients;
};

inline ModelIndex ModelIndex::parent() const
{
    return m_model ? m_model->parent_index(*this) : ModelIndex();
}

}
