#include "SampleWidget.h"
#include <LibAudio/ABuffer.h>
#include <LibGUI/GPainter.h>
#include <LibM/math.h>

SampleWidget::SampleWidget(GWidget* parent)
    : GFrame(parent)
{
    set_frame_shape(FrameShape::Container);
    set_frame_shadow(FrameShadow::Sunken);
    set_frame_thickness(2);
}

SampleWidget::~SampleWidget()
{
}

void SampleWidget::paint_event(GPaintEvent& event)
{
    GFrame::paint_event(event);
    GPainter painter(*this);

    painter.add_clip_rect(event.rect());
    painter.fill_rect(frame_inner_rect(), Color::Black);

    float sample_max = 0;
    int count = 0;
    int x_offset = frame_inner_rect().x();
    int x = x_offset;
    int y_offset = frame_inner_rect().center().y();

    if (m_buffer) {
        int samples_per_pixel = m_buffer->sample_count() / frame_inner_rect().width();
        for (int sample_index = 0; sample_index < m_buffer->sample_count() && (x - x_offset) < frame_inner_rect().width(); ++sample_index) {
            float sample = fabsf(m_buffer->samples()[sample_index].left);

            sample_max = max(sample, sample_max);
            ++count;

            if (count >= samples_per_pixel) {
                Point min_point = { x, y_offset + static_cast<int>(-sample_max * frame_inner_rect().height() / 2) };
                Point max_point = { x++, y_offset + static_cast<int>(sample_max * frame_inner_rect().height() / 2) };
                painter.draw_line(min_point, max_point, Color::Green);

                count = 0;
                sample_max = 0;
            }
        }
    } else {
        painter.draw_line({ x, y_offset }, { frame_inner_rect().width(), y_offset }, Color::Green);
    }
}

void SampleWidget::set_buffer(ABuffer* buffer)
{
    if (m_buffer == buffer)
        return;
    m_buffer = buffer;
    update();
}
