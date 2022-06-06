/*
    src/vscrollpanel.cpp -- Adds a vertical scrollbar around a widget
    that is too big to fit into a certain area

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    The widget drawing code is based on the NanoVG demo application
    by Mikko Mononen.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "GLFW/glfw3.h"
#include "nanogui/vector.h"
#include "nanovg.h"
#include <nanogui/vscrollpanel.h>
#include <nanogui/theme.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>

NAMESPACE_BEGIN(nanogui)

VScrollPanel::VScrollPanel(Widget *parent)
    : Widget(parent), m_child_preferred_size(0, 0), m_scroll(0.f, 0.0f),
      m_scrollbar_size(10), m_arrow_size(1), m_is_overflow(false),
      m_update_layout(false) {}

void VScrollPanel::perform_layout(NVGcontext *ctx) {
    Widget::perform_layout(ctx);

    if (m_children.empty())
        return;
    if (m_children.size() > 1)
        throw std::runtime_error("VScrollPanel should have one child.");

    Widget *child = m_children[0];
    m_child_preferred_size = child->preferred_size(ctx);
    m_overflow = m_child_preferred_size - m_size;
    m_overflow[0] = std::max(m_overflow[0], 0.f);
    m_overflow[1] = std::max(m_overflow[1], 0.f);
    m_is_overflow = m_overflow[0] >0 || m_overflow[1] > 0;
    m_both_scrollbars = m_is_overflow && m_overflow[0] > 0 && m_overflow[1] > 0;

    if (m_is_overflow) {
      child->set_position(-m_scroll* m_overflow);
      // If we don't set the size, we cannot get the mouse events (captured by the big child?)
      child->set_size(Vector2i(m_size.x() - m_scrollbar_size, m_size.y() - m_scrollbar_size));
    } else {
        child->set_position(Vector2i(0));
        child->set_size(m_size);
        m_scroll[0] = 0;
        m_scroll[1] = 0;
        m_is_overflow = false;
        m_overflow[0] = 0;
        m_overflow[1] = 0;
    }
    child->perform_layout(ctx);
}

Vector2i VScrollPanel::preferred_size(NVGcontext *ctx) const {
    if (m_children.empty())
        return Vector2i(0);
    return m_children[0]->preferred_size(ctx) + Vector2i(m_scrollbar_size, 0);
}

bool VScrollPanel::mouse_drag_event(const Vector2i &p, const Vector2i &rel,
                                    int button, int modifiers) {
  if(m_children.empty()) {
    return Widget::mouse_drag_event(p, rel, button, modifiers);
  }
  if(!m_is_overflow) {
    return m_children[0]->mouse_drag_event(p, rel, button, modifiers);
  }
  if(m_state != ClickedHScrollBar && m_state != ClickedVScrollbar) {
    return false;
  }
  int axis = m_state == ClickedVScrollbar ? 1 : 0;
  float scroll =
      m_size[axis] *
      std::min(1.f, m_size[axis] / (float)m_child_preferred_size[axis]);

  m_scroll[axis] =
      std::max(0.f, std::min(1.f, m_scroll[axis] + rel[axis] / (m_size[axis] -
                                                                8.f - scroll)));
  m_update_layout = true;

  return true;
}

bool VScrollPanel::mouse_button_event(const Vector2i &p, int button, bool down,
                                      int modifiers) {
  if (Widget::mouse_button_event(p, button, down, modifiers)) {
    return true;
  }
  if(m_children.empty() || !m_is_overflow) {
    return false;
  }
  if(!down || button != GLFW_MOUSE_BUTTON_1) {
    m_state = Normal;
    return false;
  }

  bool clicked_vscrollbar = p[0] > m_pos[0] + m_size[0] - m_scrollbar_size;
  bool clicked_hscrollbar = p[1] > m_pos[1] + m_size[1] - m_scrollbar_size;
  if(!clicked_hscrollbar && !clicked_vscrollbar) {
    m_state = Normal;
    return m_children[0]->mouse_button_event(p, button, down, modifiers);
  }

  if(clicked_hscrollbar) {
    m_state = ClickedHScrollBar;
  } else {
    m_state = ClickedVScrollbar;
  }

  int axis = clicked_vscrollbar ? 1 : 0;
  int scroll =
      (int)(m_size[axis] *
            std::min(1.f, m_size[axis] / (float)m_child_preferred_size[axis]));
  int start =
      (int)(m_pos[axis] + 4 + 1 + (m_size[axis] - 8 - scroll) * m_scroll[1]);

  float delta = 0.f;

  if (p[axis] < start)
    delta = -m_size[axis] / (float)m_child_preferred_size[axis];
  else if (p[axis] > start + scroll)
    delta = m_size[axis] / (float)m_child_preferred_size[axis];

  m_scroll[axis] = std::max(0.f, std::min(1.f, m_scroll[axis] + delta * 0.98f));

  m_children[0]->set_position(-m_scroll * m_overflow);
  m_update_layout = true;
  return true;
}

bool VScrollPanel::scroll_event(const Vector2i &p, const Vector2f &rel) {
  if(m_children.empty() || !m_is_overflow) {
    // we do not handle the event here
    return Widget::scroll_event(p, rel);
  }
  // GLFW_MOD_SHIFT
  int mods = 0;
  Screen* parent_screen = screen();
  if(parent_screen) {
    mods = parent_screen->keyboard_mods();
  }
  bool is_horizontal = rel[0] != 0.f || (rel[1] != 0.f && mods == GLFW_MOD_SHIFT );
  float scroll_delta = is_horizontal ? (rel[0] != 0.f ? rel[0] : rel[1]) : rel[1];
  if (m_overflow[1] > 0 && !is_horizontal) {
    float scroll_amount = scroll_delta * m_size[1] * .25f;
    m_scroll[1] = std::max(
        0.f,
        std::min(1.f, m_scroll[1] - scroll_amount / m_child_preferred_size[1]));

    // NB: there was a mouse_motion event sent to the first child (m_children[0]) in the previous nanogui code
    return true;
  }

  if(m_overflow[0] && is_horizontal) {
    float scroll_amount = scroll_delta * m_size[0] * .25f;

    m_scroll[0] = std::max(
        0.f,
        std::min(1.f, m_scroll[0] - scroll_amount / m_child_preferred_size[0]));

    m_update_layout = true;
    return true;
  }

  return Widget::scroll_event(p, rel);
}

void VScrollPanel::draw(NVGcontext *ctx) {
    if (m_children.empty())
        return;
    Widget *child = m_children[0];
    Vector2f offset = -m_scroll*m_overflow;
    child->set_position(offset);

    // TODO should we update the child preferred size here?
    // m_child_preferred_size = child->preferred_size(ctx);

    if (m_update_layout) {
      m_update_layout = false;
      child->perform_layout(ctx);
    }

    nvgSave(ctx);
    nvgTranslate(ctx, m_pos.x(), m_pos.y());
    nvgIntersectScissor(ctx, 0, 0, m_size.x(), m_size.y());
    if (child->visible())
      child->draw(ctx);
    nvgRestore(ctx);

    if (!m_is_overflow)
      return;
    if (m_overflow[0] >0) {
      // horizontal scrollbar
      draw_scrollbar(ctx, 0);
    }
    if (m_overflow[1] > 0) {
      // vertical scrollbar
      draw_scrollbar(ctx, 1);
    }
    return;
}
void VScrollPanel::draw_scrollbar(NVGcontext *ctx, int axis) {
  // https://stackoverflow.com/a/16367035/8720686
  float viewable_ratio = (float)m_size[axis] / (float)m_child_preferred_size[axis];
  int scrollbar_area = m_size[axis] - 2*m_arrow_size;
  if(m_both_scrollbars) {
    scrollbar_area -= m_scrollbar_size;
  }

  int thumb_size = scrollbar_area * viewable_ratio;
  int thumb_position = m_scroll[axis] * (scrollbar_area - thumb_size);

  int other_axis = !axis;

  Vector2i v_scrollbar_pos;
  v_scrollbar_pos[axis] = m_pos[axis];
  // for the vertical scrollbar [1], we draw x [0] at the right side (offsetting the scrollbar size)
  v_scrollbar_pos[other_axis] = m_pos[other_axis] + m_size[other_axis] - m_scrollbar_size;

  Vector2i v_scrollbar_size;
  v_scrollbar_size[axis] = m_size[axis];
  if(m_both_scrollbars) {
    v_scrollbar_size[axis] -= m_scrollbar_size;
  }
  v_scrollbar_size[other_axis] = m_scrollbar_size;

  Vector2i v_thumb_size;
  v_thumb_size[axis] = thumb_size;
  v_thumb_size[other_axis] = m_scrollbar_size;

  Vector2i v_thumb_position;
  v_thumb_position[axis] = m_pos[axis] + m_arrow_size + thumb_position;
  v_thumb_position[other_axis] = v_scrollbar_pos[other_axis] + 1;

  // background
  nvgFillColor(ctx, nvgRGBf(0, 0, 0));
  nvgBeginPath(ctx);
  nvgRect(ctx, v_scrollbar_pos[0], v_scrollbar_pos[1], v_scrollbar_size[0],
          v_scrollbar_size[1]);
  nvgFill(ctx);

  // thumb
  nvgFillColor(ctx, nvgRGBf(0.5, 0.5, 0.5));
  nvgBeginPath(ctx);
  nvgRect(ctx,
          v_thumb_position[0], v_thumb_position[1], //
          v_thumb_size[0], v_thumb_size[1]);
  nvgFill(ctx);
}


NAMESPACE_END(nanogui)
