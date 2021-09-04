/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGUI/AbstractView.h>
#include <LibGUI/Model.h>
#include <LibGUI/ModelSelection.h>

namespace GUI {

void ModelSelection::remove_matching(Function<bool(ModelIndex const&)> filter)
{
    Vector<ModelIndex> to_remove;
    for (auto& index : m_indices) {
        if (filter(index))
            to_remove.append(index);
    }
    if (!to_remove.is_empty()) {
        for (auto& index : to_remove)
            m_indices.remove(index);
        notify_selection_changed();
    }
}

void ModelSelection::set(ModelIndex const& index)
{
    VERIFY(index.is_valid());
    if (m_indices.size() == 1 && m_indices.contains(index))
        return;
    m_indices.clear();
    m_indices.set(index);
    notify_selection_changed();
}

void ModelSelection::add(ModelIndex const& index)
{
    VERIFY(index.is_valid());
    if (m_indices.contains(index))
        return;
    m_indices.set(index);
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

void ModelSelection::toggle(ModelIndex const& index)
{
    VERIFY(index.is_valid());
    if (m_indices.contains(index))
        m_indices.remove(index);
    else
        m_indices.set(index);
    notify_selection_changed();
}

bool ModelSelection::remove(ModelIndex const& index)
{
    VERIFY(index.is_valid());
    if (!m_indices.contains(index))
        return false;
    m_indices.remove(index);
    notify_selection_changed();
    return true;
}

void ModelSelection::clear()
{
    if (m_indices.is_empty())
        return;
    m_indices.clear();
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
