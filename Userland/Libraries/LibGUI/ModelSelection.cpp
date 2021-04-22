/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Badge.h>
#include <LibGUI/AbstractView.h>
#include <LibGUI/Model.h>
#include <LibGUI/ModelSelection.h>

namespace GUI {

void ModelSelection::remove_matching(Function<bool(const ModelIndex&)> filter)
{
    Vector<ModelIndex> to_remove;
    for (auto& index : m_indexes) {
        if (filter(index))
            to_remove.append(index);
    }
    if (!to_remove.is_empty()) {
        for (auto& index : to_remove)
            m_indexes.remove(index);
        notify_selection_changed();
    }
}

void ModelSelection::set(const ModelIndex& index)
{
    VERIFY(index.is_valid());
    if (m_indexes.size() == 1 && m_indexes.contains(index))
        return;
    m_indexes.clear();
    m_indexes.set(index);
    notify_selection_changed();
}

void ModelSelection::add(const ModelIndex& index)
{
    VERIFY(index.is_valid());
    if (m_indexes.contains(index))
        return;
    m_indexes.set(index);
    notify_selection_changed();
}

void ModelSelection::add_all(const Vector<ModelIndex>& indices)
{
    {
        TemporaryChange notify_change { m_disable_notify, true };
        for (auto& index : indices)
            add(index);
    }

    if (m_notify_pending)
        notify_selection_changed();
}

void ModelSelection::toggle(const ModelIndex& index)
{
    VERIFY(index.is_valid());
    if (m_indexes.contains(index))
        m_indexes.remove(index);
    else
        m_indexes.set(index);
    notify_selection_changed();
}

bool ModelSelection::remove(const ModelIndex& index)
{
    VERIFY(index.is_valid());
    if (!m_indexes.contains(index))
        return false;
    m_indexes.remove(index);
    notify_selection_changed();
    return true;
}

void ModelSelection::clear()
{
    if (m_indexes.is_empty())
        return;
    m_indexes.clear();
    notify_selection_changed();
}

void ModelSelection::notify_selection_changed()
{
    if (!m_disable_notify) {
        m_view.notify_selection_changed({});
        m_notify_pending = false;
    } else {
        m_notify_pending = true;
    }
}

}
