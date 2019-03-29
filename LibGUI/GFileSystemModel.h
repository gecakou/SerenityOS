#pragma once

#include <LibGUI/GModel.h>

class GFileSystemModel : public GModel {
    friend class Node;
public:
    static Retained<GFileSystemModel> create(const String& root_path = "/")
    {
        return adopt(*new GFileSystemModel(root_path));
    }
    virtual ~GFileSystemModel() override;

    String root_path() const { return m_root_path; }

    virtual int row_count(const GModelIndex& = GModelIndex()) const override;
    virtual int column_count(const GModelIndex& = GModelIndex()) const override;
    virtual GVariant data(const GModelIndex&, Role = Role::Display) const override;
    virtual void update() override;
    virtual GModelIndex parent_index(const GModelIndex&) const override;
    virtual GModelIndex index(int row, int column = 0, const GModelIndex& = GModelIndex()) const override;
    virtual void activate(const GModelIndex&) override;

private:
    explicit GFileSystemModel(const String& root_path);

    String m_root_path;

    struct Node;
    Node* m_root { nullptr };
};
