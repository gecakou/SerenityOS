@GUI::Frame {
    fill_with_background_color: true
    layout: @GUI::VerticalBoxLayout {
        margins: [8]
    }

    @GUI::GroupBox {
        title: "Cursor Speed"
        fixed_height: 106
        layout: @GUI::VerticalBoxLayout {
            margins: [6]
            spacing: 2
        }

        @GUI::Widget {
            layout: @GUI::HorizontalBoxLayout {
                spacing: 16
            }

            @GUI::Label {
                fixed_width: 32
                fixed_height: 32
                icon: "/res/graphics/mouse-cursor-speed.png"
            }

            @GUI::Label {
                text: "The relative speed of the mouse cursor."
                text_alignment: "CenterLeft"
            }
        }

        @GUI::Widget {
            layout: @GUI::HorizontalBoxLayout {
                spacing: 16
            }

            @GUI::Widget {
                fixed_width: 32
            }

            @GUI::HorizontalSlider {
                name: "speed_slider"
                min: 0
                max: 100
                value: 50
            }

            @GUI::Label {
                fixed_width: 40
                name: "speed_label"
            }
        }
    }

    @GUI::GroupBox {
        title: "Scroll Wheel Step Size"
        fixed_height: 106
        layout: @GUI::VerticalBoxLayout {
            margins: [6]
            spacing: 2
        }

        @GUI::Widget {
            layout: @GUI::HorizontalBoxLayout {
                spacing: 16
            }

            @GUI::Label {
                fixed_width: 32
                fixed_height: 32
                icon: "/res/graphics/scroll-wheel-step-size.png"
            }

            @GUI::Label {
                text: "The number of steps taken when the scroll wheel is\nmoved a single notch."
                text_alignment: "CenterLeft"
            }
        }

        @GUI::Widget {
            layout: @GUI::HorizontalBoxLayout {
                margins: [8]
                spacing: 8
            }

            @GUI::Widget {
                fixed_width: 32
            }

            @GUI::Label {
                autosize: true
                text: "Step size:"
            }

            @GUI::SpinBox {
                name: "scroll_length_spinbox"
                min: 0
                max: 100
                value: 50
                fixed_width: 100
            }

            @GUI::Layout::Spacer {}
        }
    }

    @GUI::GroupBox {
        title: "Double-click Speed"
        fixed_height: 106
        layout: @GUI::VerticalBoxLayout {
            margins: [6]
            spacing: 2
        }

        @GUI::Widget {
            layout: @GUI::HorizontalBoxLayout {
                spacing: 16
            }

            @MouseSettings::DoubleClickArrowWidget {
                fixed_width: 32
                fixed_height: 32
                name: "double_click_arrow_widget"
            }

            @GUI::Label {
                text: "The maximum time that may pass between two clicks\nin order for them to become a double-click."
                text_alignment: "CenterLeft"
            }
        }

        @GUI::Widget {
            layout: @GUI::HorizontalBoxLayout {
                margins: [8]
                spacing: 8
            }

            @GUI::Widget {
                fixed_width: 32
            }

            @GUI::HorizontalSlider {
                name: "double_click_speed_slider"
                min: 0
                max: 100
                value: 50
            }

            @GUI::Label {
                fixed_width: 40
                name: "double_click_speed_label"
            }
        }
    }

    @GUI::GroupBox {
        title: "Button Configuration"
        fixed_height: 136
        layout: @GUI::VerticalBoxLayout {
            margins: [16, 8, 8]
            spacing: 2
        }

        @GUI::Widget {
            layout: @GUI::HorizontalBoxLayout {
                spacing: 16
            }

            @GUI::Label {
                fixed_width: 32
                fixed_height: 32
                icon: "/res/graphics/switch-mouse-buttons.png"
            }

            @GUI::CheckBox {
                name: "switch_buttons_input"
                text: "Switch primary and secondary buttons"
            }
        }

        @GUI::Widget {
            layout: @GUI::HorizontalBoxLayout {
                spacing: 16
            }

            @GUI::Label {
                fixed_width: 32
                fixed_height: 32
                icon: "/res/graphics/emulate-middle-mouse.png"
            }

            @GUI::Widget {
                layout: @GUI::HorizontalBoxLayout {
                    spacing: 4
                }

                @GUI::CheckBox {
                    name: "emulate_middle_mouse"
                    fixed_width: 16
                }

                @GUI::Label {
                    text: "Simulate middle mouse button press with Alt+[secondary mouse button]"
                    text_alignment: "CenterLeft"
                }
            }
        }
    }
}
