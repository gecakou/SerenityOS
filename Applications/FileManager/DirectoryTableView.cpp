#include "DirectoryTableView.h"

DirectoryTableView::DirectoryTableView(GWidget* parent)
    : GTableView(parent)
{
    set_model(make<DirectoryTableModel>());
}

DirectoryTableView::~DirectoryTableView()
{
}

void DirectoryTableView::open(const String& path)
{
    model().open(path);
}

void DirectoryTableView::model_notification(const GModelNotification& notification)
{
    if (notification.type() == GModelNotification::Type::ModelUpdated) {
        set_status_message(String::format("%d item%s (%u byte%s)",
        model().row_count(),
        model().row_count() != 1 ? "s" : "",
        model().bytes_in_files(),
        model().bytes_in_files() != 1 ? "s" : ""));
    }
}

void DirectoryTableView::set_status_message(const String& message)
{
    if (on_status_message)
        on_status_message(message);
}
