/*
 * Copyright (c) 2021, Hunter Salyer <thefalsehonesty@gmail.com>
 * Copyright (c) 2022, Gregory Bertilson <zaggy1024@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Function.h>

#include "Enums.h"
#include "LookupTables.h"
#include "Parser.h"
#include "TreeParser.h"

namespace Video::VP9 {

// Parsing of binary trees is handled here, as defined in sections 9.3.
// Each syntax element is defined in its own section for each overarching section listed here:
// - 9.3.1: Selection of the binary tree to be used.
// - 9.3.2: Probability selection based on context and often the node of the tree.
// - 9.3.4: Counting each syntax element when it is read.

template<typename OutputType>
inline ErrorOr<OutputType> parse_tree(BitStream& bit_stream, TreeParser::TreeSelection tree_selection, Function<u8(u8)> const& probability_getter)
{
    // 9.3.3: The tree decoding function.
    if (tree_selection.is_single_value())
        return static_cast<OutputType>(tree_selection.single_value());

    int const* tree = tree_selection.tree();
    int n = 0;
    do {
        u8 node = n >> 1;
        n = tree[n + TRY(bit_stream.read_bool(probability_getter(node)))];
    } while (n > 0);

    return static_cast<OutputType>(-n);
}

inline void increment_counter(u8& counter)
{
    counter = min(static_cast<u32>(counter) + 1, 255);
}

ErrorOr<Partition> TreeParser::parse_partition(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, bool has_rows, bool has_columns, BlockSubsize block_subsize, u8 num_8x8, Vector<u8> const& above_partition_context, Vector<u8> const& left_partition_context, u32 row, u32 column, bool frame_is_intra)
{
    // Tree array
    TreeParser::TreeSelection tree = { PartitionSplit };
    if (has_rows && has_columns)
        tree = { partition_tree };
    else if (has_rows)
        tree = { rows_partition_tree };
    else if (has_columns)
        tree = { cols_partition_tree };

    // Probability array
    u32 above = 0;
    u32 left = 0;
    auto bsl = mi_width_log2_lookup[block_subsize];
    auto block_offset = mi_width_log2_lookup[Block_64x64] - bsl;
    for (auto i = 0; i < num_8x8; i++) {
        above |= above_partition_context[column + i];
        left |= left_partition_context[row + i];
    }
    above = (above & (1 << block_offset)) > 0;
    left = (left & (1 << block_offset)) > 0;
    auto context = bsl * 4 + left * 2 + above;
    u8 const* probabilities = frame_is_intra ? probability_table.kf_partition_probs()[context] : probability_table.partition_probs()[context];

    Function<u8(u8)> probability_getter = [&](u8 node) {
        if (has_rows && has_columns)
            return probabilities[node];
        if (has_columns)
            return probabilities[1];
        return probabilities[2];
    };

    auto value = TRY(parse_tree<Partition>(bit_stream, tree, probability_getter));
    increment_counter(counter.m_counts_partition[context][value]);
    return value;
}

ErrorOr<PredictionMode> TreeParser::parse_default_intra_mode(BitStream& bit_stream, ProbabilityTables const& probability_table, BlockSubsize mi_size, FrameBlockContext above, FrameBlockContext left, Array<PredictionMode, 4> const& block_sub_modes, u8 index_x, u8 index_y)
{
    // FIXME: This should use a struct for the above and left contexts.

    // Tree
    TreeParser::TreeSelection tree = { intra_mode_tree };

    // Probabilities
    PredictionMode above_mode, left_mode;
    if (mi_size >= Block_8x8) {
        above_mode = above.sub_modes[2];
        left_mode = left.sub_modes[1];
    } else {
        if (index_y > 0)
            above_mode = block_sub_modes[index_x];
        else
            above_mode = above.sub_modes[2 + index_x];

        if (index_x > 0)
            left_mode = block_sub_modes[index_y << 1];
        else
            left_mode = left.sub_modes[1 + (index_y << 1)];
    }
    u8 const* probabilities = probability_table.kf_y_mode_probs()[to_underlying(above_mode)][to_underlying(left_mode)];

    auto value = TRY(parse_tree<PredictionMode>(bit_stream, tree, [&](u8 node) { return probabilities[node]; }));
    // Default intra mode is not counted.
    return value;
}

ErrorOr<PredictionMode> TreeParser::parse_default_uv_mode(BitStream& bit_stream, ProbabilityTables const& probability_table, PredictionMode y_mode)
{
    // Tree
    TreeParser::TreeSelection tree = { intra_mode_tree };

    // Probabilities
    u8 const* probabilities = probability_table.kf_uv_mode_prob()[to_underlying(y_mode)];

    auto value = TRY(parse_tree<PredictionMode>(bit_stream, tree, [&](u8 node) { return probabilities[node]; }));
    // Default UV mode is not counted.
    return value;
}

ErrorOr<PredictionMode> TreeParser::parse_intra_mode(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, BlockSubsize mi_size)
{
    // Tree
    TreeParser::TreeSelection tree = { intra_mode_tree };

    // Probabilities
    auto context = size_group_lookup[mi_size];
    u8 const* probabilities = probability_table.y_mode_probs()[context];

    auto value = TRY(parse_tree<PredictionMode>(bit_stream, tree, [&](u8 node) { return probabilities[node]; }));
    increment_counter(counter.m_counts_intra_mode[context][to_underlying(value)]);
    return value;
}

ErrorOr<PredictionMode> TreeParser::parse_sub_intra_mode(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter)
{
    // Tree
    TreeParser::TreeSelection tree = { intra_mode_tree };

    // Probabilities
    u8 const* probabilities = probability_table.y_mode_probs()[0];

    auto value = TRY(parse_tree<PredictionMode>(bit_stream, tree, [&](u8 node) { return probabilities[node]; }));
    increment_counter(counter.m_counts_intra_mode[0][to_underlying(value)]);
    return value;
}

ErrorOr<PredictionMode> TreeParser::parse_uv_mode(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, PredictionMode y_mode)
{
    // Tree
    TreeParser::TreeSelection tree = { intra_mode_tree };

    // Probabilities
    u8 const* probabilities = probability_table.uv_mode_probs()[to_underlying(y_mode)];

    auto value = TRY(parse_tree<PredictionMode>(bit_stream, tree, [&](u8 node) { return probabilities[node]; }));
    increment_counter(counter.m_counts_uv_mode[to_underlying(y_mode)][to_underlying(value)]);
    return value;
}

ErrorOr<u8> TreeParser::parse_segment_id(BitStream& bit_stream, Array<u8, 7> const& probabilities)
{
    auto value = TRY(parse_tree<u8>(bit_stream, { segment_tree }, [&](u8 node) { return probabilities[node]; }));
    // Segment ID is not counted.
    return value;
}

ErrorOr<bool> TreeParser::parse_segment_id_predicted(BitStream& bit_stream, Array<u8, 3> const& probabilities, u8 above_seg_pred_context, u8 left_seg_pred_context)
{
    auto context = left_seg_pred_context + above_seg_pred_context;
    auto value = TRY(parse_tree<bool>(bit_stream, { binary_tree }, [&](u8) { return probabilities[context]; }));
    // Segment ID prediction is not counted.
    return value;
}

ErrorOr<PredictionMode> TreeParser::parse_inter_mode(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, u8 mode_context_for_ref_frame_0)
{
    // Tree
    TreeParser::TreeSelection tree = { inter_mode_tree };

    // Probabilities
    u8 const* probabilities = probability_table.inter_mode_probs()[mode_context_for_ref_frame_0];

    auto value = TRY(parse_tree<PredictionMode>(bit_stream, tree, [&](u8 node) { return probabilities[node]; }));
    increment_counter(counter.m_counts_inter_mode[mode_context_for_ref_frame_0][to_underlying(value) - to_underlying(PredictionMode::NearestMv)]);
    return value;
}

ErrorOr<InterpolationFilter> TreeParser::parse_interpolation_filter(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, FrameBlockContext above, FrameBlockContext left)
{
    // FIXME: Above and left context should be provided by a struct.

    // Tree
    TreeParser::TreeSelection tree = { interp_filter_tree };

    // Probabilities
    // NOTE: SWITCHABLE_FILTERS is not used in the spec for this function. Therefore, the number
    //       was demystified by referencing the reference codec libvpx:
    //       https://github.com/webmproject/libvpx/blob/705bf9de8c96cfe5301451f1d7e5c90a41c64e5f/vp9/common/vp9_pred_common.h#L69
    u8 left_interp = !left.is_intra_predicted() ? left.interpolation_filter : SWITCHABLE_FILTERS;
    u8 above_interp = !above.is_intra_predicted() ? above.interpolation_filter : SWITCHABLE_FILTERS;
    u8 context = SWITCHABLE_FILTERS;
    if (above_interp == left_interp || above_interp == SWITCHABLE_FILTERS)
        context = left_interp;
    else if (left_interp == SWITCHABLE_FILTERS)
        context = above_interp;
    u8 const* probabilities = probability_table.interp_filter_probs()[context];

    auto value = TRY(parse_tree<InterpolationFilter>(bit_stream, tree, [&](u8 node) { return probabilities[node]; }));
    increment_counter(counter.m_counts_interp_filter[context][to_underlying(value)]);
    return value;
}

ErrorOr<bool> TreeParser::parse_skip(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, FrameBlockContext above, FrameBlockContext left)
{
    // Probabilities
    u8 context = 0;
    context += static_cast<u8>(above.skip_coefficients);
    context += static_cast<u8>(left.skip_coefficients);
    u8 probability = probability_table.skip_prob()[context];

    auto value = TRY(parse_tree<bool>(bit_stream, { binary_tree }, [&](u8) { return probability; }));
    increment_counter(counter.m_counts_skip[context][value]);
    return value;
}

ErrorOr<TransformSize> TreeParser::parse_tx_size(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, TransformSize max_tx_size, FrameBlockContext above, FrameBlockContext left)
{
    // FIXME: Above and left contexts should be in structs.

    // Tree
    TreeParser::TreeSelection tree { tx_size_8_tree };
    if (max_tx_size == Transform_16x16)
        tree = { tx_size_16_tree };
    if (max_tx_size == Transform_32x32)
        tree = { tx_size_32_tree };

    // Probabilities
    auto above_context = max_tx_size;
    auto left_context = max_tx_size;
    if (above.is_available && !above.skip_coefficients)
        above_context = above.transform_size;
    if (left.is_available && !left.skip_coefficients)
        left_context = left.transform_size;
    if (!left.is_available)
        left_context = above_context;
    if (!above.is_available)
        above_context = left_context;
    auto context = (above_context + left_context) > max_tx_size;

    u8 const* probabilities = probability_table.tx_probs()[max_tx_size][context];

    auto value = TRY(parse_tree<TransformSize>(bit_stream, tree, [&](u8 node) { return probabilities[node]; }));
    increment_counter(counter.m_counts_tx_size[max_tx_size][context][value]);
    return value;
}

ErrorOr<bool> TreeParser::parse_block_is_inter_predicted(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, FrameBlockContext above, FrameBlockContext left)
{
    // FIXME: Above and left contexts should be in structs.

    // Probabilities
    u8 context = 0;
    if (above.is_available && left.is_available)
        context = (left.is_intra_predicted() && above.is_intra_predicted()) ? 3 : static_cast<u8>(above.is_intra_predicted() || left.is_intra_predicted());
    else if (above.is_available || left.is_available)
        context = 2 * static_cast<u8>(above.is_available ? above.is_intra_predicted() : left.is_intra_predicted());
    u8 probability = probability_table.is_inter_prob()[context];

    auto value = TRY(parse_tree<bool>(bit_stream, { binary_tree }, [&](u8) { return probability; }));
    increment_counter(counter.m_counts_is_inter[context][value]);
    return value;
}

ErrorOr<ReferenceMode> TreeParser::parse_comp_mode(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, ReferenceFrameType comp_fixed_ref, FrameBlockContext above, FrameBlockContext left)
{
    // FIXME: Above and left contexts should be in structs.

    // Probabilities
    u8 context;
    if (above.is_available && left.is_available) {
        if (above.is_single_reference() && left.is_single_reference()) {
            auto is_above_fixed = above.ref_frames.primary == comp_fixed_ref;
            auto is_left_fixed = left.ref_frames.primary == comp_fixed_ref;
            context = is_above_fixed ^ is_left_fixed;
        } else if (above.is_single_reference()) {
            auto is_above_fixed = above.ref_frames.primary == comp_fixed_ref;
            context = 2 + static_cast<u8>(is_above_fixed || above.is_intra_predicted());
        } else if (left.is_single_reference()) {
            auto is_left_fixed = left.ref_frames.primary == comp_fixed_ref;
            context = 2 + static_cast<u8>(is_left_fixed || left.is_intra_predicted());
        } else {
            context = 4;
        }
    } else if (above.is_available) {
        if (above.is_single_reference())
            context = above.ref_frames.primary == comp_fixed_ref;
        else
            context = 3;
    } else if (left.is_available) {
        if (left.is_single_reference())
            context = static_cast<u8>(left.ref_frames.primary == comp_fixed_ref);
        else
            context = 3;
    } else {
        context = 1;
    }
    u8 probability = probability_table.comp_mode_prob()[context];

    auto value = TRY(parse_tree<ReferenceMode>(bit_stream, { binary_tree }, [&](u8) { return probability; }));
    increment_counter(counter.m_counts_comp_mode[context][value]);
    return value;
}

ErrorOr<ReferenceIndex> TreeParser::parse_comp_ref(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, ReferenceFrameType comp_fixed_ref, ReferenceFramePair comp_var_ref, ReferenceIndex variable_reference_index, FrameBlockContext above, FrameBlockContext left)
{
    // FIXME: Above and left contexts should be in structs.

    // Probabilities
    u8 context;

    if (above.is_available && left.is_available) {
        if (above.is_intra_predicted() && left.is_intra_predicted()) {
            context = 2;
        } else if (left.is_intra_predicted()) {
            if (above.is_single_reference()) {
                context = 1 + 2 * (above.ref_frames.primary != comp_var_ref.secondary);
            } else {
                context = 1 + 2 * (above.ref_frames[variable_reference_index] != comp_var_ref.secondary);
            }
        } else if (above.is_intra_predicted()) {
            if (left.is_single_reference()) {
                context = 1 + 2 * (left.ref_frames.primary != comp_var_ref.secondary);
            } else {
                context = 1 + 2 * (left.ref_frames[variable_reference_index] != comp_var_ref.secondary);
            }
        } else {
            auto var_ref_above = above.is_single_reference() ? above.ref_frames.primary : above.ref_frames[variable_reference_index];
            auto var_ref_left = left.is_single_reference() ? left.ref_frames.primary : left.ref_frames[variable_reference_index];
            if (var_ref_above == var_ref_left && comp_var_ref.secondary == var_ref_above) {
                context = 0;
            } else if (left.is_single_reference() && above.is_single_reference()) {
                if ((var_ref_above == comp_fixed_ref && var_ref_left == comp_var_ref.primary)
                    || (var_ref_left == comp_fixed_ref && var_ref_above == comp_var_ref.primary)) {
                    context = 4;
                } else if (var_ref_above == var_ref_left) {
                    context = 3;
                } else {
                    context = 1;
                }
            } else if (left.is_single_reference() || above.is_single_reference()) {
                auto vrfc = left.is_single_reference() ? var_ref_above : var_ref_left;
                auto rfs = above.is_single_reference() ? var_ref_above : var_ref_left;
                if (vrfc == comp_var_ref.secondary && rfs != comp_var_ref.secondary) {
                    context = 1;
                } else if (rfs == comp_var_ref.secondary && vrfc != comp_var_ref.secondary) {
                    context = 2;
                } else {
                    context = 4;
                }
            } else if (var_ref_above == var_ref_left) {
                context = 4;
            } else {
                context = 2;
            }
        }
    } else if (above.is_available) {
        if (above.is_intra_predicted()) {
            context = 2;
        } else {
            if (above.is_single_reference()) {
                context = 3 * static_cast<u8>(above.ref_frames.primary != comp_var_ref.secondary);
            } else {
                context = 4 * static_cast<u8>(above.ref_frames[variable_reference_index] != comp_var_ref.secondary);
            }
        }
    } else if (left.is_available) {
        if (left.is_intra_predicted()) {
            context = 2;
        } else {
            if (left.is_single_reference()) {
                context = 3 * static_cast<u8>(left.ref_frames.primary != comp_var_ref.secondary);
            } else {
                context = 4 * static_cast<u8>(left.ref_frames[variable_reference_index] != comp_var_ref.secondary);
            }
        }
    } else {
        context = 2;
    }

    u8 probability = probability_table.comp_ref_prob()[context];

    auto value = TRY(parse_tree<ReferenceIndex>(bit_stream, { binary_tree }, [&](u8) { return probability; }));
    increment_counter(counter.m_counts_comp_ref[context][to_underlying(value)]);
    return value;
}

ErrorOr<bool> TreeParser::parse_single_ref_part_1(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, FrameBlockContext above, FrameBlockContext left)
{
    // FIXME: Above and left contexts should be in structs.

    // Probabilities
    u8 context;
    if (above.is_available && left.is_available) {
        if (above.is_intra_predicted() && left.is_intra_predicted()) {
            context = 2;
        } else if (left.is_intra_predicted()) {
            if (above.is_single_reference()) {
                context = 4 * (above.ref_frames.primary == ReferenceFrameType::LastFrame);
            } else {
                context = 1 + (above.ref_frames.primary == ReferenceFrameType::LastFrame || above.ref_frames.secondary == ReferenceFrameType::LastFrame);
            }
        } else if (above.is_intra_predicted()) {
            if (left.is_single_reference()) {
                context = 4 * (left.ref_frames.primary == ReferenceFrameType::LastFrame);
            } else {
                context = 1 + (left.ref_frames.primary == ReferenceFrameType::LastFrame || left.ref_frames.secondary == ReferenceFrameType::LastFrame);
            }
        } else {
            if (left.is_single_reference() && above.is_single_reference()) {
                context = 2 * (above.ref_frames.primary == ReferenceFrameType::LastFrame) + 2 * (left.ref_frames.primary == ReferenceFrameType::LastFrame);
            } else if (!left.is_single_reference() && !above.is_single_reference()) {
                auto above_used_last_frame = above.ref_frames.primary == ReferenceFrameType::LastFrame || above.ref_frames.secondary == ReferenceFrameType::LastFrame;
                auto left_used_last_frame = left.ref_frames.primary == ReferenceFrameType::LastFrame || left.ref_frames.secondary == ReferenceFrameType::LastFrame;
                context = 1 + (above_used_last_frame || left_used_last_frame);
            } else {
                auto single_reference_type = above.is_single_reference() ? above.ref_frames.primary : left.ref_frames.primary;
                auto compound_reference_a_type = above.is_single_reference() ? left.ref_frames.primary : above.ref_frames.primary;
                auto compound_reference_b_type = above.is_single_reference() ? left.ref_frames.secondary : above.ref_frames.secondary;
                context = compound_reference_a_type == ReferenceFrameType::LastFrame || compound_reference_b_type == ReferenceFrameType::LastFrame;
                if (single_reference_type == ReferenceFrameType::LastFrame)
                    context += 3;
            }
        }
    } else if (above.is_available) {
        if (above.is_intra_predicted()) {
            context = 2;
        } else {
            if (above.is_single_reference()) {
                context = 4 * (above.ref_frames.primary == ReferenceFrameType::LastFrame);
            } else {
                context = 1 + (above.ref_frames.primary == ReferenceFrameType::LastFrame || above.ref_frames.secondary == ReferenceFrameType::LastFrame);
            }
        }
    } else if (left.is_available) {
        if (left.is_intra_predicted()) {
            context = 2;
        } else {
            if (left.is_single_reference()) {
                context = 4 * (left.ref_frames.primary == ReferenceFrameType::LastFrame);
            } else {
                context = 1 + (left.ref_frames.primary == ReferenceFrameType::LastFrame || left.ref_frames.secondary == ReferenceFrameType::LastFrame);
            }
        }
    } else {
        context = 2;
    }
    u8 probability = probability_table.single_ref_prob()[context][0];

    auto value = TRY(parse_tree<bool>(bit_stream, { binary_tree }, [&](u8) { return probability; }));
    increment_counter(counter.m_counts_single_ref[context][0][value]);
    return value;
}

ErrorOr<bool> TreeParser::parse_single_ref_part_2(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, FrameBlockContext above, FrameBlockContext left)
{
    // FIXME: Above and left contexts should be in structs.

    // Probabilities
    u8 context;
    if (above.is_available && left.is_available) {
        if (above.is_intra_predicted() && left.is_intra_predicted()) {
            context = 2;
        } else if (left.is_intra_predicted()) {
            if (above.is_single_reference()) {
                if (above.ref_frames.primary == ReferenceFrameType::LastFrame) {
                    context = 3;
                } else {
                    context = 4 * (above.ref_frames.primary == ReferenceFrameType::GoldenFrame);
                }
            } else {
                context = 1 + 2 * (above.ref_frames.primary == ReferenceFrameType::GoldenFrame || above.ref_frames.secondary == ReferenceFrameType::GoldenFrame);
            }
        } else if (above.is_intra_predicted()) {
            if (left.is_single_reference()) {
                if (left.ref_frames.primary == ReferenceFrameType::LastFrame) {
                    context = 3;
                } else {
                    context = 4 * (left.ref_frames.primary == ReferenceFrameType::GoldenFrame);
                }
            } else {
                context = 1 + 2 * (left.ref_frames.primary == ReferenceFrameType::GoldenFrame || left.ref_frames.secondary == ReferenceFrameType::GoldenFrame);
            }
        } else {
            if (left.is_single_reference() && above.is_single_reference()) {
                auto above_last = above.ref_frames.primary == ReferenceFrameType::LastFrame;
                auto left_last = left.ref_frames.primary == ReferenceFrameType::LastFrame;
                if (above_last && left_last) {
                    context = 3;
                } else if (above_last) {
                    context = 4 * (left.ref_frames.primary == ReferenceFrameType::GoldenFrame);
                } else if (left_last) {
                    context = 4 * (above.ref_frames.primary == ReferenceFrameType::GoldenFrame);
                } else {
                    context = 2 * (above.ref_frames.primary == ReferenceFrameType::GoldenFrame) + 2 * (left.ref_frames.primary == ReferenceFrameType::GoldenFrame);
                }
            } else if (!left.is_single_reference() && !above.is_single_reference()) {
                if (above.ref_frames.primary == left.ref_frames.primary && above.ref_frames.secondary == left.ref_frames.secondary) {
                    context = 3 * (above.ref_frames.primary == ReferenceFrameType::GoldenFrame || above.ref_frames.secondary == ReferenceFrameType::GoldenFrame);
                } else {
                    context = 2;
                }
            } else {
                auto single_reference_type = above.is_single_reference() ? above.ref_frames.primary : left.ref_frames.primary;
                auto compound_reference_a_type = above.is_single_reference() ? left.ref_frames.primary : above.ref_frames.primary;
                auto compound_reference_b_type = above.is_single_reference() ? left.ref_frames.secondary : above.ref_frames.secondary;
                context = compound_reference_a_type == ReferenceFrameType::GoldenFrame || compound_reference_b_type == ReferenceFrameType::GoldenFrame;
                if (single_reference_type == ReferenceFrameType::GoldenFrame) {
                    context += 3;
                } else if (single_reference_type != ReferenceFrameType::AltRefFrame) {
                    context = 1 + (2 * context);
                }
            }
        }
    } else if (above.is_available) {
        if (above.is_intra_predicted() || (above.ref_frames.primary == ReferenceFrameType::LastFrame && above.is_single_reference())) {
            context = 2;
        } else if (above.is_single_reference()) {
            context = 4 * (above.ref_frames.primary == ReferenceFrameType::GoldenFrame);
        } else {
            context = 3 * (above.ref_frames.primary == ReferenceFrameType::GoldenFrame || above.ref_frames.secondary == ReferenceFrameType::GoldenFrame);
        }
    } else if (left.is_available) {
        if (left.is_intra_predicted() || (left.ref_frames.primary == ReferenceFrameType::LastFrame && left.is_single_reference())) {
            context = 2;
        } else if (left.is_single_reference()) {
            context = 4 * (left.ref_frames.primary == ReferenceFrameType::GoldenFrame);
        } else {
            context = 3 * (left.ref_frames.primary == ReferenceFrameType::GoldenFrame || left.ref_frames.secondary == ReferenceFrameType::GoldenFrame);
        }
    } else {
        context = 2;
    }
    u8 probability = probability_table.single_ref_prob()[context][1];

    auto value = TRY(parse_tree<bool>(bit_stream, { binary_tree }, [&](u8) { return probability; }));
    increment_counter(counter.m_counts_single_ref[context][1][value]);
    return value;
}

ErrorOr<MvJoint> TreeParser::parse_motion_vector_joint(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter)
{
    auto value = TRY(parse_tree<MvJoint>(bit_stream, { mv_joint_tree }, [&](u8 node) { return probability_table.mv_joint_probs()[node]; }));
    increment_counter(counter.m_counts_mv_joint[value]);
    return value;
}

ErrorOr<bool> TreeParser::parse_motion_vector_sign(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, u8 component)
{
    auto value = TRY(parse_tree<bool>(bit_stream, { binary_tree }, [&](u8) { return probability_table.mv_sign_prob()[component]; }));
    increment_counter(counter.m_counts_mv_sign[component][value]);
    return value;
}

ErrorOr<MvClass> TreeParser::parse_motion_vector_class(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, u8 component)
{
    // Spec doesn't mention node, but the probabilities table has an extra dimension
    // so we will use node for that.
    auto value = TRY(parse_tree<MvClass>(bit_stream, { mv_class_tree }, [&](u8 node) { return probability_table.mv_class_probs()[component][node]; }));
    increment_counter(counter.m_counts_mv_class[component][value]);
    return value;
}

ErrorOr<bool> TreeParser::parse_motion_vector_class0_bit(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, u8 component)
{
    auto value = TRY(parse_tree<bool>(bit_stream, { binary_tree }, [&](u8) { return probability_table.mv_class0_bit_prob()[component]; }));
    increment_counter(counter.m_counts_mv_class0_bit[component][value]);
    return value;
}

ErrorOr<u8> TreeParser::parse_motion_vector_class0_fr(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, u8 component, bool class_0_bit)
{
    auto value = TRY(parse_tree<u8>(bit_stream, { mv_fr_tree }, [&](u8 node) { return probability_table.mv_class0_fr_probs()[component][class_0_bit][node]; }));
    increment_counter(counter.m_counts_mv_class0_fr[component][class_0_bit][value]);
    return value;
}

ErrorOr<bool> TreeParser::parse_motion_vector_class0_hp(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, u8 component, bool use_hp)
{
    TreeParser::TreeSelection tree { 1 };
    if (use_hp)
        tree = { binary_tree };
    auto value = TRY(parse_tree<bool>(bit_stream, tree, [&](u8) { return probability_table.mv_class0_hp_prob()[component]; }));
    increment_counter(counter.m_counts_mv_class0_hp[component][value]);
    return value;
}

ErrorOr<bool> TreeParser::parse_motion_vector_bit(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, u8 component, u8 bit_index)
{
    auto value = TRY(parse_tree<bool>(bit_stream, { binary_tree }, [&](u8) { return probability_table.mv_bits_prob()[component][bit_index]; }));
    increment_counter(counter.m_counts_mv_bits[component][bit_index][value]);
    return value;
}

ErrorOr<u8> TreeParser::parse_motion_vector_fr(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, u8 component)
{
    auto value = TRY(parse_tree<u8>(bit_stream, { mv_fr_tree }, [&](u8 node) { return probability_table.mv_fr_probs()[component][node]; }));
    increment_counter(counter.m_counts_mv_fr[component][value]);
    return value;
}

ErrorOr<bool> TreeParser::parse_motion_vector_hp(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, u8 component, bool use_hp)
{
    TreeParser::TreeSelection tree { 1 };
    if (use_hp)
        tree = { binary_tree };
    auto value = TRY(parse_tree<u8>(bit_stream, tree, [&](u8) { return probability_table.mv_hp_prob()[component]; }));
    increment_counter(counter.m_counts_mv_hp[component][value]);
    return value;
}

TokensContext TreeParser::get_tokens_context(bool subsampling_x, bool subsampling_y, u32 rows, u32 columns, Array<Vector<bool>, 3> const& above_nonzero_context, Array<Vector<bool>, 3> const& left_nonzero_context, u8 token_cache[1024], TransformSize transform_size, TransformSet transform_set, u8 plane, u32 start_x, u32 start_y, u16 position, bool is_inter, u8 band, u16 coef_index)
{
    u8 context;
    if (coef_index == 0) {
        auto sx = plane > 0 ? subsampling_x : false;
        auto sy = plane > 0 ? subsampling_y : false;
        auto max_x = (2 * columns) >> sx;
        auto max_y = (2 * rows) >> sy;
        u8 numpts = 1 << transform_size;
        auto x4 = start_x >> 2;
        auto y4 = start_y >> 2;
        u32 above = 0;
        u32 left = 0;
        for (size_t i = 0; i < numpts; i++) {
            if (x4 + i < max_x)
                above |= above_nonzero_context[plane][x4 + i];
            if (y4 + i < max_y)
                left |= left_nonzero_context[plane][y4 + i];
        }
        context = above + left;
    } else {
        u32 neighbor_0, neighbor_1;
        auto n = 4 << transform_size;
        auto i = position / n;
        auto j = position % n;
        auto a = i > 0 ? (i - 1) * n + j : 0;
        auto a2 = i * n + j - 1;
        if (i > 0 && j > 0) {
            if (transform_set == TransformSet { TransformType::DCT, TransformType::ADST }) {
                neighbor_0 = a;
                neighbor_1 = a;
            } else if (transform_set == TransformSet { TransformType::ADST, TransformType::DCT }) {
                neighbor_0 = a2;
                neighbor_1 = a2;
            } else {
                neighbor_0 = a;
                neighbor_1 = a2;
            }
        } else if (i > 0) {
            neighbor_0 = a;
            neighbor_1 = a;
        } else {
            neighbor_0 = a2;
            neighbor_1 = a2;
        }
        context = (1 + token_cache[neighbor_0] + token_cache[neighbor_1]) >> 1;
    }

    return TokensContext { transform_size, plane > 0, is_inter, band, context };
}

ErrorOr<bool> TreeParser::parse_more_coefficients(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, TokensContext const& context)
{
    auto probability = probability_table.coef_probs()[context.m_tx_size][context.m_is_uv_plane][context.m_is_inter][context.m_band][context.m_context_index][0];
    auto value = TRY(parse_tree<u8>(bit_stream, { binary_tree }, [&](u8) { return probability; }));
    increment_counter(counter.m_counts_more_coefs[context.m_tx_size][context.m_is_uv_plane][context.m_is_inter][context.m_band][context.m_context_index][value]);
    return value;
}

ErrorOr<Token> TreeParser::parse_token(BitStream& bit_stream, ProbabilityTables const& probability_table, SyntaxElementCounter& counter, TokensContext const& context)
{
    Function<u8(u8)> probability_getter = [&](u8 node) -> u8 {
        auto prob = probability_table.coef_probs()[context.m_tx_size][context.m_is_uv_plane][context.m_is_inter][context.m_band][context.m_context_index][min(2, 1 + node)];
        if (node < 2)
            return prob;
        auto x = (prob - 1) / 2;
        auto const& pareto_table = probability_table.pareto_table();
        if ((prob & 1) != 0)
            return pareto_table[x][node - 2];
        return (pareto_table[x][node - 2] + pareto_table[x + 1][node - 2]) >> 1;
    };

    auto value = TRY(parse_tree<Token>(bit_stream, { token_tree }, probability_getter));
    increment_counter(counter.m_counts_token[context.m_tx_size][context.m_is_uv_plane][context.m_is_inter][context.m_band][context.m_context_index][min(2, value)]);
    return value;
}

}
