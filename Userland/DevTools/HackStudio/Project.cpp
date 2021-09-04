/*
 * Copyright (c) 2018-2020, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Project.h"
#include "HackStudio.h"
#include <LibCore/File.h>

namespace HackStudio {

Project::Project(String const& root_path)
    : m_root_path(root_path)
{
    m_model = GUI::FileSystemModel::create(root_path, GUI::FileSystemModel::Mode::FilesAndDirectories);
}

OwnPtr<Project> Project::open_with_root_path(String const& root_path)
{
    if (!Core::File::is_directory(root_path))
        return {};
    return adopt_own(*new Project(root_path));
}

template<typename Callback>
static void traverse_model(const GUI::FileSystemModel& model, const GUI::ModelIndex& index, Callback callback)
{
    if (index.is_valid())
        callback(index);
    auto row_count = model.row_count(index);
    if (!row_count)
        return;
    for (int row = 0; row < row_count; ++row) {
        auto child_index = model.index(row, GUI::FileSystemModel::Column::Name, index);
        traverse_model(model, child_index, callback);
    }
}

void Project::for_each_text_file(Function<void(ProjectFile const&)> callback) const
{
    traverse_model(model(), {}, [&](auto& index) {
        auto file = get_file(model().full_path(index));
        callback(*file);
    });
}

NonnullRefPtr<ProjectFile> Project::get_file(String const& path) const
{
    auto full_path = to_absolute_path(path);
    for (auto& file : m_files) {
        if (file.name() == full_path)
            return file;
    }
    auto file = ProjectFile::construct_with_name(full_path);
    m_files.append(file);
    return file;
}

String Project::to_absolute_path(String const& path) const
{
    if (LexicalPath { path }.is_absolute()) {
        return path;
    }
    return LexicalPath { String::formatted("{}/{}", m_root_path, path) }.string();
}

}
