#pragma once

#include <LibHTML/Layout/LayoutNode.h>

class Element;

class LayoutBlock : public LayoutNode {
public:
    explicit LayoutBlock(const Node&);
    virtual ~LayoutBlock() override;

    virtual const char* class_name() const override { return "LayoutBlock"; }

    virtual void layout() override;

private:
    virtual bool is_block() const override { return true; }

    void compute_width();
    void compute_height();
};
