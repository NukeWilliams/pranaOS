/*
 * Copyright (c) 2021, Krisna Pranav
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// includes
#include <base/StdLibExtras.h>
#include <libweb/layout/BlockBox.h>
#include <libweb/layout/BlockFormattingContext.h>
#include <libweb/layout/Box.h>
#include <libweb/layout/FlexFormattingContext.h>
#include <libweb/layout/InitialContainingBlockBox.h>
#include <libweb/layout/TextNode.h>

namespace Web::Layout {

FlexFormattingContext::FlexFormattingContext(Box& context_box, FormattingContext* parent)
    : FormattingContext(context_box, parent)
{
}

FlexFormattingContext::~FlexFormattingContext()
{
}

struct FlexItem {
    Box& box;
    float flex_base_size { 0 };
    float hypothetical_main_size { 0 };
    float hypothetical_cross_size { 0 };
    float target_main_size { 0 };
    bool frozen { false };
    Optional<float> flex_factor {};
    float scaled_flex_shrink_factor { 0 };
    float max_content_flex_fraction { 0 };
    float main_size { 0 };
    float cross_size { 0 };
    float main_offset { 0 };
    float cross_offset { 0 };
    bool is_min_violation { false };
    bool is_max_violation { false };
};

struct FlexLine {
    Vector<FlexItem*> items;
    float cross_size { 0 };
};

void FlexFormattingContext::run(Box& box, LayoutMode)
{
    auto flex_direction = box.computed_values().flex_direction();
    auto is_row = (flex_direction == CSS::FlexDirection::Row || flex_direction == CSS::FlexDirection::RowReverse);
    auto main_size_is_infinite = false;
    auto get_pixel_size = [](Box& box, const CSS::Length& length) {
        return length.resolved(CSS::Length::make_px(0), box, box.containing_block()->width()).to_px(box);
    };
    auto layout_for_maximum_main_size = [&](Box& box) {
        if (is_row)
            layout_inside(box, LayoutMode::OnlyRequiredLineBreaks);
        else
            layout_inside(box, LayoutMode::AllPossibleLineBreaks);
    };
    auto containing_block_effective_main_size = [&is_row, &main_size_is_infinite](Box& box) {
        if (is_row) {
            if (box.containing_block()->has_definite_width())
                return box.containing_block()->width();
            main_size_is_infinite = true;
            return NumericLimits<float>::max();
        } else {
            if (box.containing_block()->has_definite_height())
                return box.containing_block()->height();
            main_size_is_infinite = true;
            return NumericLimits<float>::max();
        }
    };
    auto has_definite_main_size = [&is_row](Box& box) {
        return is_row ? box.has_definite_width() : box.has_definite_height();
    };
    auto has_definite_cross_size = [&is_row](Box& box) {
        return is_row ? box.has_definite_height() : box.has_definite_width();
    };
    auto specified_main_size = [&is_row, &get_pixel_size](Box& box) {
        return is_row
            ? get_pixel_size(box, box.computed_values().width())
            : get_pixel_size(box, box.computed_values().height());
    };
    auto specified_cross_size = [&is_row, &get_pixel_size](Box& box) {
        return is_row
            ? get_pixel_size(box, box.computed_values().height())
            : get_pixel_size(box, box.computed_values().width());
    };
    auto has_main_min_size = [&is_row](Box& box) {
        return is_row
            ? !box.computed_values().min_width().is_undefined_or_auto()
            : !box.computed_values().min_height().is_undefined_or_auto();
    };
    auto has_cross_min_size = [&is_row](Box& box) {
        return is_row
            ? !box.computed_values().min_height().is_undefined_or_auto()
            : !box.computed_values().min_width().is_undefined_or_auto();
    };
    auto specified_main_min_size = [&is_row, &get_pixel_size](Box& box) {
        return is_row
            ? get_pixel_size(box, box.computed_values().min_width())
            : get_pixel_size(box, box.computed_values().min_height());
    };
    auto specified_cross_min_size = [&is_row, &get_pixel_size](Box& box) {
        return is_row
            ? get_pixel_size(box, box.computed_values().min_height())
            : get_pixel_size(box, box.computed_values().min_width());
    };
    auto has_main_max_size = [&is_row](Box& box) {
        return is_row
            ? !box.computed_values().max_width().is_undefined_or_auto()
            : !box.computed_values().max_height().is_undefined_or_auto();
    };
    auto has_cross_max_size = [&is_row](Box& box) {
        return is_row
            ? !box.computed_values().max_height().is_undefined_or_auto()
            : !box.computed_values().max_width().is_undefined_or_auto();
    };
    auto specified_main_max_size = [&is_row, &get_pixel_size](Box& box) {
        return is_row
            ? get_pixel_size(box, box.computed_values().max_width())
            : get_pixel_size(box, box.computed_values().max_height());
    };
    auto specified_cross_max_size = [&is_row, &get_pixel_size](Box& box) {
        return is_row
            ? get_pixel_size(box, box.computed_values().max_height())
            : get_pixel_size(box, box.computed_values().max_width());
    };
    auto calculated_main_size = [&is_row](Box& box) {
        return is_row ? box.width() : box.height();
    };
    auto is_cross_auto = [&is_row](Box& box) {
        return is_row ? box.computed_values().height().is_auto() : box.computed_values().width().is_auto();
    };
    auto is_main_axis_margin_first_auto = [&is_row](Box& box) {
        return is_row ? box.computed_values().margin().left.is_auto() : box.computed_values().margin().top.is_auto();
    };
    auto is_main_axis_margin_second_auto = [&is_row](Box& box) {
        return is_row ? box.computed_values().margin().right.is_auto() : box.computed_values().margin().bottom.is_auto();
    };
    auto sum_of_margin_padding_border_in_main_axis = [&is_row](Box& box) {
        if (is_row) {
            return box.box_model().margin.left
                + box.box_model().margin.right
                + box.box_model().padding.left
                + box.box_model().padding.right
                + box.box_model().border.left
                + box.box_model().border.right;
        } else {
            return box.box_model().margin.top
                + box.box_model().margin.bottom
                + box.box_model().padding.top
                + box.box_model().padding.bottom
                + box.box_model().border.top
                + box.box_model().border.bottom;
        }
    };
    auto calculate_hypothetical_cross_size = [&is_row, this](Box& box) {
        if (is_row) {
            return BlockFormattingContext::compute_theoretical_height(box);
        } else {

            BlockFormattingContext context(box, this);
            context.compute_width(box);
            return box.width();
        }
    };
    auto set_main_size = [&is_row](Box& box, float size) {
        if (is_row)
            box.set_width(size);
        else
            box.set_height(size);
    };
    auto set_cross_size = [&is_row](Box& box, float size) {
        if (is_row)
            box.set_height(size);
        else
            box.set_width(size);
    };
    auto set_offset = [&is_row](Box& box, float main_offset, float cross_offset) {
        if (is_row)
            box.set_offset(main_offset, cross_offset);
        else
            box.set_offset(cross_offset, main_offset);
    };
    auto set_main_axis_first_margin = [&is_row](Box& box, float margin) {
        if (is_row)
            box.box_model().margin.left = margin;
        else
            box.box_model().margin.top = margin;
    };
    auto set_main_axis_second_margin = [&is_row](Box& box, float margin) {
        if (is_row)
            box.box_model().margin.right = margin;
        else
            box.box_model().margin.bottom = margin;
    };

    Vector<FlexItem> flex_items;
    box.for_each_child_of_type<Box>([&](Box& child_box) {
        layout_inside(child_box, LayoutMode::Default);

        if (child_box.is_anonymous()) {
            bool contains_only_white_space = true;
            child_box.for_each_in_inclusive_subtree_of_type<TextNode>([&contains_only_white_space](auto& text_node) {
                if (!text_node.text_for_rendering().is_whitespace()) {
                    contains_only_white_space = false;
                    return IterationDecision::Break;
                }
                return IterationDecision::Continue;
            });
            if (contains_only_white_space)
                return IterationDecision::Continue;
        }

        child_box.set_flex_item(true);
        flex_items.append({ child_box });
        return IterationDecision::Continue;
    });

    float main_available_size = 0;
    [[maybe_unused]] float cross_available_size = 0;
    [[maybe_unused]] float main_max_size = NumericLimits<float>::max();
    [[maybe_unused]] float main_min_size = 0;
    float cross_max_size = NumericLimits<float>::max();
    float cross_min_size = 0;

    bool main_is_constrained = false;
    bool cross_is_constrained = false;

    if (has_definite_main_size(box)) {
        main_is_constrained = true;
        main_available_size = specified_main_size(box);
    } else {
        if (has_main_max_size(box)) {
            main_max_size = specified_main_max_size(box);
            main_is_constrained = true;
        }
        if (has_main_min_size(box)) {
            main_min_size = specified_main_min_size(box);
            main_is_constrained = true;
        }

        if (!main_is_constrained) {
            auto available_main_size = containing_block_effective_main_size(box);
            main_available_size = available_main_size - sum_of_margin_padding_border_in_main_axis(box);
        }
    }

    if (has_definite_cross_size(box)) {
        cross_available_size = specified_cross_size(box);
    } else {
        if (has_cross_max_size(box)) {
            cross_max_size = specified_cross_max_size(box);
            cross_is_constrained = true;
        }
        if (has_cross_min_size(box)) {
            cross_min_size = specified_cross_min_size(box);
            cross_is_constrained = true;
        }

        if (!cross_is_constrained)
            cross_available_size = cross_max_size;
    }

    for (auto& flex_item : flex_items) {
        auto& child_box = flex_item.box;
        auto flex_basis = child_box.computed_values().flex_basis();
        if (flex_basis.type == CSS::FlexBasis::Length) {
            flex_item.flex_base_size = get_pixel_size(child_box, flex_basis.length);
        } else if (flex_basis.type == CSS::FlexBasis::Content
            && has_definite_cross_size(child_box)
            && false) {
            TODO();

        } else if (flex_basis.type == CSS::FlexBasis::Content
            && false) {
            TODO();
        } else if (flex_basis.type == CSS::FlexBasis::Content
            && false && false) {
            TODO();
        } else {
            if (has_definite_main_size(child_box)) {
                flex_item.flex_base_size = specified_main_size(child_box);
            } else {
                layout_for_maximum_main_size(child_box);
                flex_item.flex_base_size = calculated_main_size(child_box);
            }
        }

        auto clamp_min = has_main_min_size(child_box)
            ? specified_main_min_size(child_box)
            : 0;
        auto clamp_max = has_main_max_size(child_box)
            ? specified_main_max_size(child_box)
            : NumericLimits<float>::max();

        flex_item.hypothetical_main_size = clamp(flex_item.flex_base_size, clamp_min, clamp_max);
    }

    if (!main_is_constrained || main_available_size == 0) {
        float largest_max_content_flex_fraction = 0;
        for (auto& flex_item : flex_items) {
            float max_content_contribution = calculated_main_size(flex_item.box);
            float max_content_flex_fraction = max_content_contribution - flex_item.flex_base_size;
            if (max_content_flex_fraction > 0) {
                max_content_flex_fraction /= max(flex_item.box.computed_values().flex_grow_factor().value_or(1), 1.0f);
            } else {
                max_content_flex_fraction /= max(flex_item.box.computed_values().flex_shrink_factor().value_or(1), 1.0f) * flex_item.flex_base_size;
            }
            flex_item.max_content_flex_fraction = max_content_flex_fraction;

            if (max_content_flex_fraction > largest_max_content_flex_fraction)
                largest_max_content_flex_fraction = max_content_flex_fraction;
        }

        float result = 0;
        for (auto& flex_item : flex_items) {
            auto product = 0;
            if (flex_item.max_content_flex_fraction > 0) {
                product = largest_max_content_flex_fraction * flex_item.box.computed_values().flex_grow_factor().value_or(1);
            } else {
                product = largest_max_content_flex_fraction * max(flex_item.box.computed_values().flex_shrink_factor().value_or(1), 1.0f) * flex_item.flex_base_size;
            }
            result += flex_item.flex_base_size + product;
        }
        main_available_size = clamp(result, main_min_size, main_max_size);
    }
    set_main_size(box, main_available_size);

    Vector<FlexLine> flex_lines;

    if (box.computed_values().flex_wrap() == CSS::FlexWrap::Nowrap) {
        FlexLine line;
        for (auto& flex_item : flex_items) {
            line.items.append(&flex_item);
        }
        flex_lines.append(line);
    } else {
        FlexLine line;
        float line_main_size = 0;
        for (auto& flex_item : flex_items) {
            if ((line_main_size + flex_item.hypothetical_main_size) > main_available_size) {
                flex_lines.append(line);
                line = {};
                line_main_size = 0;
            }
            line.items.append(&flex_item);
            line_main_size += flex_item.hypothetical_main_size;
        }
        flex_lines.append(line);
    }

    enum FlexFactor {
        FlexGrowFactor,
        FlexShrinkFactor
    };

    FlexFactor used_flex_factor;

    for (auto& flex_line : flex_lines) {

        size_t number_of_unfrozen_items_on_line = flex_line.items.size();

        float sum_of_hypothetical_main_sizes = 0;
        for (auto& flex_item : flex_line.items) {
            sum_of_hypothetical_main_sizes += flex_item->hypothetical_main_size;
        }
        if (sum_of_hypothetical_main_sizes < main_available_size)
            used_flex_factor = FlexFactor::FlexGrowFactor;
        else
            used_flex_factor = FlexFactor::FlexShrinkFactor;

        for (auto& flex_item : flex_line.items) {
            if (used_flex_factor == FlexFactor::FlexGrowFactor)
                flex_item->flex_factor = flex_item->box.computed_values().flex_grow_factor();
            else if (used_flex_factor == FlexFactor::FlexShrinkFactor)
                flex_item->flex_factor = flex_item->box.computed_values().flex_shrink_factor();
        }

        auto freeze_item_setting_target_main_size_to_hypothetical_main_size = [&number_of_unfrozen_items_on_line](FlexItem& item) {
            item.target_main_size = item.hypothetical_main_size;
            number_of_unfrozen_items_on_line--;
            item.frozen = true;
        };
        for (auto& flex_item : flex_line.items) {
            if (flex_item->flex_factor.has_value() && flex_item->flex_factor.value() == 0) {
                freeze_item_setting_target_main_size_to_hypothetical_main_size(*flex_item);
            } else if (flex_item->flex_factor.has_value()) {

                continue;
            } else if (used_flex_factor == FlexFactor::FlexGrowFactor) {

                if (flex_item->flex_base_size >= flex_item->hypothetical_main_size) {
                    freeze_item_setting_target_main_size_to_hypothetical_main_size(*flex_item);
                }
            } else if (used_flex_factor == FlexFactor::FlexShrinkFactor) {
                if (flex_item->flex_base_size < flex_item->hypothetical_main_size) {
                    freeze_item_setting_target_main_size_to_hypothetical_main_size(*flex_item);
                }
            }
        }

        auto calculate_free_space = [&]() {
            float sum_of_items_on_line = 0;
            for (auto& flex_item : flex_line.items) {
                if (flex_item->frozen)
                    sum_of_items_on_line += flex_item->target_main_size;
                else
                    sum_of_items_on_line += flex_item->flex_base_size;
            }
            return main_available_size - sum_of_items_on_line;
        };

        float initial_free_space = calculate_free_space();

        auto for_each_unfrozen_item = [&flex_line](auto callback) {
            for (auto& flex_item : flex_line.items) {
                if (!flex_item->frozen)
                    callback(flex_item);
            }
        };

        while (number_of_unfrozen_items_on_line > 0) {
        
            auto remaining_free_space = calculate_free_space();
            float sum_of_unfrozen_flex_items_flex_factors = 0;
            for_each_unfrozen_item([&](FlexItem* item) {
                sum_of_unfrozen_flex_items_flex_factors += item->flex_factor.value_or(1);
            });

            if (sum_of_unfrozen_flex_items_flex_factors < 1) {
                auto intermediate_free_space = initial_free_space * sum_of_unfrozen_flex_items_flex_factors;
                if (Base::abs(intermediate_free_space) < Base::abs(remaining_free_space))
                    remaining_free_space = intermediate_free_space;
            }

            if (remaining_free_space != 0) {
                if (used_flex_factor == FlexFactor::FlexGrowFactor) {
                    float sum_of_flex_grow_factor_of_unfrozen_items = sum_of_unfrozen_flex_items_flex_factors;
                    for_each_unfrozen_item([&](FlexItem* flex_item) {
                        float ratio = flex_item->flex_factor.value_or(1) / sum_of_flex_grow_factor_of_unfrozen_items;
                        flex_item->target_main_size = flex_item->flex_base_size + (remaining_free_space * ratio);
                    });
                } else if (used_flex_factor == FlexFactor::FlexShrinkFactor) {
                    float sum_of_scaled_flex_shrink_factor_of_unfrozen_items = 0;
                    for_each_unfrozen_item([&](FlexItem* flex_item) {
                        flex_item->scaled_flex_shrink_factor = flex_item->flex_factor.value_or(1) * flex_item->flex_base_size;
                        sum_of_scaled_flex_shrink_factor_of_unfrozen_items += flex_item->scaled_flex_shrink_factor;
                    });

                    for_each_unfrozen_item([&](FlexItem* flex_item) {
                        float ratio = flex_item->scaled_flex_shrink_factor / sum_of_scaled_flex_shrink_factor_of_unfrozen_items;
                        flex_item->target_main_size = flex_item->flex_base_size - (Base::abs(remaining_free_space) * ratio);
                    });
                }
            } else {

                for_each_unfrozen_item([&](FlexItem* flex_item) {
                    flex_item->target_main_size = flex_item->flex_base_size;
                });
            }

            float adjustments = 0;
            for_each_unfrozen_item([&](FlexItem* item) {
                auto min_main = has_main_min_size(item->box)
                    ? specified_main_min_size(item->box)
                    : 0;
                auto max_main = has_main_max_size(item->box)
                    ? specified_main_max_size(item->box)
                    : NumericLimits<float>::max();

                float original_target_size = item->target_main_size;

                if (item->target_main_size < min_main) {
                    item->target_main_size = min_main;
                    item->is_min_violation = true;
                }

                if (item->target_main_size > max_main) {
                    item->target_main_size = max_main;
                    item->is_max_violation = true;
                }
                float delta = item->target_main_size - original_target_size;

                adjustments += delta;
            });

            if (adjustments == 0) {
                for_each_unfrozen_item([&](FlexItem* item) {
                    --number_of_unfrozen_items_on_line;
                    item->frozen = true;
                });
            } else if (adjustments > 0) {
                for_each_unfrozen_item([&](FlexItem* item) {
                    if (item->is_min_violation) {
                        --number_of_unfrozen_items_on_line;
                        item->frozen = true;
                    }
                });
            } else if (adjustments < 0) {
                for_each_unfrozen_item([&](FlexItem* item) {
                    if (item->is_max_violation) {
                        --number_of_unfrozen_items_on_line;
                        item->frozen = true;
                    }
                });
            }
        }

        for (auto& flex_item : flex_line.items) {
            flex_item->main_size = flex_item->target_main_size;
        };
    }

    for (auto& flex_item : flex_items) {
        flex_item.hypothetical_cross_size = calculate_hypothetical_cross_size(flex_item.box);
    }

    if (flex_lines.size() == 1 && has_definite_cross_size(box)) {
        flex_lines[0].cross_size = specified_cross_size(box);
    } else {
        for (auto& flex_line : flex_lines) {

            float largest_hypothetical_cross_size = 0;
            for (auto& flex_item : flex_line.items) {
                if (largest_hypothetical_cross_size < flex_item->hypothetical_cross_size)
                    largest_hypothetical_cross_size = flex_item->hypothetical_cross_size;
            }

            flex_line.cross_size = max(0.0f, largest_hypothetical_cross_size);
        }

        if (flex_lines.size() == 1) {
            clamp(flex_lines[0].cross_size, cross_min_size, cross_max_size);
        }
    }
    
    for (auto& flex_line : flex_lines) {
        for (auto& flex_item : flex_line.items) {
            if (is_cross_auto(flex_item->box)) {
                flex_item->cross_size = flex_line.cross_size;
            } else {
                flex_item->cross_size = flex_item->hypothetical_cross_size;
            }
        }
    }

    for (auto& flex_line : flex_lines) {

        float used_main_space = 0;
        size_t auto_margins = 0;
        for (auto& flex_item : flex_line.items) {
            used_main_space += flex_item->cross_size;
            if (is_main_axis_margin_first_auto(flex_item->box))
                ++auto_margins;
            if (is_main_axis_margin_second_auto(flex_item->box))
                ++auto_margins;
        }
        float remaining_free_space = main_available_size - used_main_space;
        if (remaining_free_space > 0) {
            float size_per_auto_margin = remaining_free_space / (float)auto_margins;
            for (auto& flex_item : flex_line.items) {
                if (is_main_axis_margin_first_auto(flex_item->box))
                    set_main_axis_first_margin(flex_item->box, size_per_auto_margin);
                if (is_main_axis_margin_second_auto(flex_item->box))
                    set_main_axis_second_margin(flex_item->box, size_per_auto_margin);
            }
        } else {
            for (auto& flex_item : flex_line.items) {
                if (is_main_axis_margin_first_auto(flex_item->box))
                    set_main_axis_first_margin(flex_item->box, 0);
                if (is_main_axis_margin_second_auto(flex_item->box))
                    set_main_axis_second_margin(flex_item->box, 0);
            }
        }

        float space_between_items = 0;
        float space_before_first_item = 0;
        auto number_of_items = flex_line.items.size();

        switch (box.computed_values().justify_content()) {
        case CSS::JustifyContent::FlexStart:
            break;
        case CSS::JustifyContent::FlexEnd:
            space_before_first_item = main_available_size - used_main_space;
            break;
        case CSS::JustifyContent::Center:
            space_before_first_item = (main_available_size - used_main_space) / 2.0f;
            break;
        case CSS::JustifyContent::SpaceBetween:
            space_between_items = remaining_free_space / (number_of_items - 1);
            break;
        case CSS::JustifyContent::SpaceAround:
            space_between_items = remaining_free_space / number_of_items;
            space_before_first_item = space_between_items / 2.0f;
            break;
        }

        float main_offset = space_before_first_item;
        for (auto& flex_item : flex_line.items) {
            flex_item->main_offset = main_offset;
            main_offset += flex_item->main_size + space_between_items;
        }
    }

    if (has_definite_cross_size(box)) {
        float clamped_cross_size = clamp(specified_cross_size(box), cross_min_size, cross_max_size);
        set_cross_size(box, clamped_cross_size);
    } else {
        float sum_of_flex_lines_cross_sizes = 0;
        for (auto& flex_line : flex_lines) {
            sum_of_flex_lines_cross_sizes += flex_line.cross_size;
        }
        float clamped_cross_size = clamp(sum_of_flex_lines_cross_sizes, cross_min_size, cross_max_size);
        set_cross_size(box, clamped_cross_size);
    }

    float cross_offset = 0;
    for (auto& flex_line : flex_lines) {
        for (auto* flex_item : flex_line.items) {
            flex_item->cross_offset = cross_offset;
        }
        cross_offset += flex_line.cross_size;
    }
    for (auto& flex_line : flex_lines) {
        for (auto* flex_item : flex_line.items) {
            set_main_size(flex_item->box, flex_item->main_size);
            set_cross_size(flex_item->box, flex_item->cross_size);
            set_offset(flex_item->box, flex_item->main_offset, flex_item->cross_offset);
        }
    }
}
}
