// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_pi.h"
#include "route_toolbar_dialog.h"

#include <algorithm>
#include <cmath>

#include <wx/dcmemory.h>
#include <wx/event.h>
#include <wx/eventfilter.h>
#include <wx/fileconf.h>

namespace {

wxSize MeasureLabel(const wxFont& font, const wxString& first,
                    const wxString& second = wxString()) {
  wxBitmap bitmap(1, 1, 24);
  wxMemoryDC dc(bitmap);
  dc.SetFont(font);
  const wxSize a = dc.GetTextExtent(first);
  const wxSize b = second.IsEmpty() ? wxSize(0, 0) : dc.GetTextExtent(second);
  dc.SelectObject(wxNullBitmap);
  const int gap = second.IsEmpty() ? 0 : 2;
  return wxSize(std::max(a.x, b.x) + 8, a.y + gap + b.y + 6);
}

wxString PilSide(double offset_nm) {
  return offset_nm >= 0.0 ? wxString("Stbd") : wxString("Port");
}

wxString PilLabel(double offset_nm) {
  return wxString::Format("PIL %+.2f NM %s", offset_nm, PilSide(offset_nm));
}

wxString TurnSide(double angle_deg) {
  return angle_deg >= 0.0 ? wxString("SB") : wxString("BB");
}

wxString TurnLabel(double turn_angle_deg, double radius_nm,
                   double rudder_angle_deg) {
  wxString label = wxString::Format(
      "WO · ΔK %.0f° %s · R %.2f NM", std::fabs(turn_angle_deg),
      TurnSide(turn_angle_deg), radius_nm);
  if (rudder_angle_deg > 0.0)
    label += wxString::Format(" · Rudder %.0f° %s", rudder_angle_deg,
                              TurnSide(turn_angle_deg));
  return label;
}

}  // namespace

class RouteToolbarLabelMoveFilter final : public wxEventFilter {
public:
  explicit RouteToolbarLabelMoveFilter(route_toolbar_pi* owner)
      : m_owner(owner) {}

  int FilterEvent(wxEvent& event) override {
    if (!m_owner || !m_owner->m_move_labels_active ||
        event.GetEventObject() != m_owner->m_parent_window)
      return wxEventFilter::Event_Skip;

    const wxEventType type = event.GetEventType();
    if (type != wxEVT_LEFT_DOWN && type != wxEVT_RIGHT_DOWN &&
        type != wxEVT_MOTION)
      return wxEventFilter::Event_Skip;

    auto* mouse = dynamic_cast<wxMouseEvent*>(&event);
    if (!mouse) return wxEventFilter::Event_Skip;

    m_owner->OnMoveLabelsMouse(*mouse);
    return wxEventFilter::Event_Processed;
  }

private:
  route_toolbar_pi* m_owner{nullptr};
};

void route_toolbar_pi::LoadUiPreferences() {
  if (m_ui_preferences_loaded) return;
  m_ui_preferences_loaded = true;

  wxFileConfig* config = GetOCPNConfigObject();
  if (!config) return;
  const wxString old_path = config->GetPath();
  config->SetPath("/PlugIns/RouteToolbar");
  config->Read("LegInfoVisible", &m_leg_info_visible, m_notes_visible);
  config->SetPath(old_path);
}

void route_toolbar_pi::SetLegInfoVisible(bool visible) {
  LoadUiPreferences();
  if (m_leg_info_visible == visible) return;
  m_leg_info_visible = visible;

  wxFileConfig* config = GetOCPNConfigObject();
  if (config) {
    const wxString old_path = config->GetPath();
    config->SetPath("/PlugIns/RouteToolbar");
    config->Write("LegInfoVisible", m_leg_info_visible);
    config->SetPath(old_path);
    config->Flush();
  }

  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::BeginMoveLabels() {
  if (m_move_labels_active) return;
  if (!m_parent_window) return;

  if (!m_notes_visible) {
    SetStatus(_("Labels are hidden. Click Show labels before using Move labels."));
    if (m_dialog) m_dialog->SetMoveLabelsActive(false);
    return;
  }

  CancelInteractiveTool();
  m_move_labels_active = true;
  m_move_label_type = LabelMoveType::None;
  m_move_label_index = -1;

  if (!m_label_move_filter) {
    m_label_move_filter = new RouteToolbarLabelMoveFilter(this);
    wxEvtHandler::AddFilter(m_label_move_filter);
  }

  SetStatus(_("Move labels: click any EBL/VRM, PIL or TURN/WO label, move it, then click again to place it. Continue with another label, or right-click to finish."));
}

void route_toolbar_pi::EndMoveLabels(bool cancel_current) {
  if (cancel_current && m_move_label_type != LabelMoveType::None)
    RestoreActiveLabelPosition();

  m_move_label_type = LabelMoveType::None;
  m_move_label_index = -1;
  m_move_labels_active = false;

  if (m_label_move_filter) {
    wxEvtHandler::RemoveFilter(m_label_move_filter);
    delete m_label_move_filter;
    m_label_move_filter = nullptr;
  }

  if (m_dialog) m_dialog->SetMoveLabelsActive(false);
  if (m_parent_window) RequestRefresh(m_parent_window);
}

wxRect route_toolbar_pi::EblLabelRect(PlugIn_ViewPort* vp,
                                      const RouteEblVrm& item) const {
  if (!vp) return wxRect();
  wxPoint target;
  GetCanvasPixLL(vp, &target, item.target_lat, item.target_lon);

  const wxString second = wxString::Format("%03.0f°T · %.2f NM",
                                           item.bearing_true, item.range_nm);
  const wxString first = item.target_name;
  const wxSize size = MeasureLabel(GetEblLabelFont(), first, second);

  wxPoint pos;
  if (item.label_manual)
    pos = wxPoint(target.x + item.label_dx_px,
                  target.y + item.label_dy_px);
  else
    pos = FindSafeLabelBox(vp, item, target, size.x, size.y);

  return wxRect(pos, size);
}

wxRect route_toolbar_pi::PilLabelRect(PlugIn_ViewPort* vp,
                                      const RoutePil& item) const {
  if (!vp) return wxRect();
  wxPoint anchor;
  GetCanvasPixLL(vp, &anchor, item.anchor_lat, item.anchor_lon);
  const wxSize size = MeasureLabel(GetEblLabelFont(), PilLabel(item.offset_nm));
  const wxPoint pos(anchor.x + (item.label_manual ? item.label_dx_px : 12),
                    anchor.y + (item.label_manual ? item.label_dy_px : -30));
  return wxRect(pos, size);
}

wxRect route_toolbar_pi::TurnLabelRect(PlugIn_ViewPort* vp,
                                       const RouteTurn& item) const {
  if (!vp) return wxRect();
  wxPoint start;
  GetCanvasPixLL(vp, &start, item.start_lat, item.start_lon);
  const wxString label = TurnLabel(item.turn_angle_deg, item.radius_nm,
                                   item.rudder_angle_deg);
  const wxSize size = MeasureLabel(GetEblLabelFont(), label);
  const wxPoint pos(start.x + (item.label_manual ? item.label_dx_px : 10),
                    start.y + (item.label_manual ? item.label_dy_px : 8));
  return wxRect(pos, size);
}

bool route_toolbar_pi::TryBeginLabelMove(const wxPoint& mouse) {
  if (!m_last_viewport || !m_notes_visible) return false;

  auto start = [&](LabelMoveType type, int index, const wxRect& rect,
                   bool manual, int dx, int dy) {
    m_move_label_type = type;
    m_move_label_index = index;
    m_label_grab_dx_px = mouse.x - rect.x;
    m_label_grab_dy_px = mouse.y - rect.y;
    m_move_original_manual = manual;
    m_move_original_dx_px = dx;
    m_move_original_dy_px = dy;
  };

  // Render order is TURN -> PIL -> EBL, so test in reverse order to pick the
  // visually topmost label when boxes overlap.
  for (int i = static_cast<int>(m_ebl_vrms.size()) - 1; i >= 0; --i) {
    const auto& item = m_ebl_vrms[static_cast<size_t>(i)];
    if (!item.visible) continue;
    const wxRect rect = EblLabelRect(m_last_viewport, item);
    if (rect.Contains(mouse)) {
      start(LabelMoveType::Ebl, i, rect, item.label_manual,
            item.label_dx_px, item.label_dy_px);
      SetStatus(_("Move label: EBL/VRM selected. Move the mouse, then click to place it. Right-click cancels and exits."));
      return true;
    }
  }

  for (int i = static_cast<int>(m_pils.size()) - 1; i >= 0; --i) {
    const auto& item = m_pils[static_cast<size_t>(i)];
    if (!item.visible) continue;
    const wxRect rect = PilLabelRect(m_last_viewport, item);
    if (rect.Contains(mouse)) {
      start(LabelMoveType::Pil, i, rect, item.label_manual,
            item.label_dx_px, item.label_dy_px);
      SetStatus(_("Move label: PIL selected. Move the mouse, then click to place it. Right-click cancels and exits."));
      return true;
    }
  }

  for (int i = static_cast<int>(m_turns.size()) - 1; i >= 0; --i) {
    const auto& item = m_turns[static_cast<size_t>(i)];
    if (!item.visible) continue;
    const wxRect rect = TurnLabelRect(m_last_viewport, item);
    if (rect.Contains(mouse)) {
      start(LabelMoveType::Turn, i, rect, item.label_manual,
            item.label_dx_px, item.label_dy_px);
      SetStatus(_("Move label: TURN/WO selected. Move the mouse, then click to place it. Right-click cancels and exits."));
      return true;
    }
  }

  SetStatus(_("Move labels: click directly inside a yellow/cyan Route Toolbar label."));
  return false;
}

void route_toolbar_pi::UpdateActiveLabelPosition(const wxPoint& mouse) {
  if (!m_last_viewport || m_move_label_type == LabelMoveType::None ||
      m_move_label_index < 0)
    return;

  wxPoint anchor;
  wxRect rect;

  if (m_move_label_type == LabelMoveType::Ebl &&
      static_cast<size_t>(m_move_label_index) < m_ebl_vrms.size()) {
    RouteEblVrm& item = m_ebl_vrms[static_cast<size_t>(m_move_label_index)];
    rect = EblLabelRect(m_last_viewport, item);
    GetCanvasPixLL(m_last_viewport, &anchor, item.target_lat, item.target_lon);

    const int x = std::clamp(mouse.x - m_label_grab_dx_px, 4,
                             std::max(4, m_last_viewport->pix_width - rect.width - 4));
    const int y = std::clamp(mouse.y - m_label_grab_dy_px, 4,
                             std::max(4, m_last_viewport->pix_height - rect.height - 4));
    item.label_manual = true;
    item.label_dx_px = x - anchor.x;
    item.label_dy_px = y - anchor.y;
  } else if (m_move_label_type == LabelMoveType::Pil &&
             static_cast<size_t>(m_move_label_index) < m_pils.size()) {
    RoutePil& item = m_pils[static_cast<size_t>(m_move_label_index)];
    rect = PilLabelRect(m_last_viewport, item);
    GetCanvasPixLL(m_last_viewport, &anchor, item.anchor_lat, item.anchor_lon);

    const int x = std::clamp(mouse.x - m_label_grab_dx_px, 4,
                             std::max(4, m_last_viewport->pix_width - rect.width - 4));
    const int y = std::clamp(mouse.y - m_label_grab_dy_px, 4,
                             std::max(4, m_last_viewport->pix_height - rect.height - 4));
    item.label_manual = true;
    item.label_dx_px = x - anchor.x;
    item.label_dy_px = y - anchor.y;
  } else if (m_move_label_type == LabelMoveType::Turn &&
             static_cast<size_t>(m_move_label_index) < m_turns.size()) {
    RouteTurn& item = m_turns[static_cast<size_t>(m_move_label_index)];
    rect = TurnLabelRect(m_last_viewport, item);
    GetCanvasPixLL(m_last_viewport, &anchor, item.start_lat, item.start_lon);

    const int x = std::clamp(mouse.x - m_label_grab_dx_px, 4,
                             std::max(4, m_last_viewport->pix_width - rect.width - 4));
    const int y = std::clamp(mouse.y - m_label_grab_dy_px, 4,
                             std::max(4, m_last_viewport->pix_height - rect.height - 4));
    item.label_manual = true;
    item.label_dx_px = x - anchor.x;
    item.label_dy_px = y - anchor.y;
  }

  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::RestoreActiveLabelPosition() {
  if (m_move_label_index < 0) return;

  if (m_move_label_type == LabelMoveType::Ebl &&
      static_cast<size_t>(m_move_label_index) < m_ebl_vrms.size()) {
    auto& item = m_ebl_vrms[static_cast<size_t>(m_move_label_index)];
    item.label_manual = m_move_original_manual;
    item.label_dx_px = m_move_original_dx_px;
    item.label_dy_px = m_move_original_dy_px;
  } else if (m_move_label_type == LabelMoveType::Pil &&
             static_cast<size_t>(m_move_label_index) < m_pils.size()) {
    auto& item = m_pils[static_cast<size_t>(m_move_label_index)];
    item.label_manual = m_move_original_manual;
    item.label_dx_px = m_move_original_dx_px;
    item.label_dy_px = m_move_original_dy_px;
  } else if (m_move_label_type == LabelMoveType::Turn &&
             static_cast<size_t>(m_move_label_index) < m_turns.size()) {
    auto& item = m_turns[static_cast<size_t>(m_move_label_index)];
    item.label_manual = m_move_original_manual;
    item.label_dx_px = m_move_original_dx_px;
    item.label_dy_px = m_move_original_dy_px;
  }

  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::CommitActiveLabelPosition() {
  if (m_move_label_type == LabelMoveType::None) return;
  SavePersistentState();
  m_move_label_type = LabelMoveType::None;
  m_move_label_index = -1;
  SetStatus(_("Label position saved. Click another label to move it, or right-click to finish."));
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::OnMoveLabelsMouse(wxMouseEvent& event) {
  if (!m_move_labels_active) return;

  if (event.RightDown()) {
    EndMoveLabels(true);
    SetStatus(_("Move labels finished."));
    return;
  }

  const wxPoint mouse = event.GetPosition();

  if (event.LeftDown()) {
    if (m_move_label_type == LabelMoveType::None)
      TryBeginLabelMove(mouse);
    else
      CommitActiveLabelPosition();
    return;
  }

  if (event.Moving() && m_move_label_type != LabelMoveType::None)
    UpdateActiveLabelPosition(mouse);
}
