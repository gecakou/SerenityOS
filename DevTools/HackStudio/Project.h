#pragma once

#include "TextDocument.h"
#include <AK/Noncopyable.h>
#include <AK/NonnullRefPtrVector.h>
#include <AK/OwnPtr.h>
#include <LibGUI/GModel.h>

class Project {
    AK_MAKE_NONCOPYABLE(Project)
    AK_MAKE_NONMOVABLE(Project)
public:
    static OwnPtr<Project> load_from_file(const String& path);

    GModel& model() { return *m_model; }

    template<typename Callback>
    void for_each_text_file(Callback callback) const
    {
        for (auto& file : m_files) {
            callback(file);
        }
    }

private:
    friend class ProjectModel;
    explicit Project(Vector<String>&& files);

    RefPtr<GModel> m_model;
    NonnullRefPtrVector<TextDocument> m_files;
};
