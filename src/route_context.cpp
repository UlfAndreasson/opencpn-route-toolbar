// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_pi.h"
#include "route_toolbar_dialog.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <wx/choicdlg.h>
#include <wx/button.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textdlg.h>

namespace {
constexpr double kPi = 3.14159265358979323846;

struct XY {
  double x{0.0};
  double y{0.0};
};

XY ToLocalNm(double lat, double lon, double ref_lat, double ref_lon) {
  const double cos_lat = std::max(0.01, std::cos(ref_lat * kPi / 180.0));
  return {(lon - ref_lon) * 60.0 * cos_lat, (lat - ref_lat) * 60.0};
}

double DistancePointToSegmentNm(const XY& p, const XY& a, const XY& b) {
  const double dx = b.x - a.x;
  const double dy = b.y - a.y;
  const double len2 = dx * dx + dy * dy;
  if (len2 < 1e-12) return std::hypot(p.x - a.x, p.y - a.y);
  const double t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / len2,
                              0.0, 1.0);
  return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

int ShowPilActionButtons(wxWindow* parent,
                         const wxString& prompt,
                         const wxArrayString& labels) {
  wxDialog dialog(parent, wxID_ANY, _("Route PIL"), wxDefaultPosition,
                  wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP);
  auto* top = new wxBoxSizer(wxVERTICAL);
  auto* text = new wxStaticText(&dialog, wxID_ANY, prompt);
  text->Wrap(360);
  top->Add(text, 0, wxEXPAND | wxALL, 12);
  for (size_t i = 0; i < labels.GetCount(); ++i) {
    auto* b = new wxButton(&dialog, wxID_ANY, labels[i]);
    top->Add(b, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    b->Bind(wxEVT_BUTTON, [&dialog, i](wxCommandEvent&) {
      dialog.EndModal(static_cast<int>(i) + 1);
    });
  }
  auto* cancel = new wxButton(&dialog, wxID_CANCEL, _("Cancel"));
  top->Add(cancel, 0, wxALIGN_RIGHT | wxALL, 10);
  dialog.SetSizerAndFit(top);
  dialog.SetMinSize(wxSize(380, dialog.GetSize().GetHeight()));
  const int result = dialog.ShowModal();
  return result > 0 ? result - 1 : -1;
}


}  // namespace

void route_toolbar_pi::EnsureContextMenu() {
  if (m_context_ebl_id >= 0 || m_context_pil_id >= 0) return;

  m_context_menu_owner = new wxMenu(_("Route Toolbar"));

  m_context_ebl_item = new wxMenuItem(
      m_context_menu_owner, wxID_ANY, _("Route Toolbar: Edit EBL/VRM..."),
      _("Edit the nearest Route Toolbar EBL/VRM"), wxITEM_NORMAL);
  m_context_ebl_id = AddCanvasContextMenuItem(m_context_ebl_item, this);

  m_context_pil_item = new wxMenuItem(
      m_context_menu_owner, wxID_ANY, _("Route Toolbar: Edit PIL..."),
      _("Edit the nearest Route Toolbar PIL"), wxITEM_NORMAL);
  m_context_pil_id = AddCanvasContextMenuItem(m_context_pil_item, this);

  SetCanvasContextMenuItemViz(m_context_ebl_id, true);
  SetCanvasContextMenuItemViz(m_context_pil_id, true);
}

int route_toolbar_pi::FindNearestEblGeometry(double lat, double lon,
                                             double max_distance_nm) const {
  int best = -1;
  double best_distance = std::numeric_limits<double>::max();
  const XY p{0.0, 0.0};
  for (size_t i = 0; i < m_ebl_vrms.size(); ++i) {
    const RouteEblVrm& ebl = m_ebl_vrms[i];
    if (!ebl.visible) continue;
    const XY a = ToLocalNm(ebl.start_lat, ebl.start_lon, lat, lon);
    const XY b = ToLocalNm(ebl.target_lat, ebl.target_lon, lat, lon);
    const double distance = std::min(
        DistancePointToSegmentNm(p, a, b),
        std::min(std::hypot(a.x, a.y), std::hypot(b.x, b.y)));
    if (distance <= max_distance_nm && distance < best_distance) {
      best = static_cast<int>(i);
      best_distance = distance;
    }
  }
  return best;
}

int route_toolbar_pi::FindNearestPil(double lat, double lon,
                                     double max_distance_nm) const {
  int best = -1;
  double best_distance = std::numeric_limits<double>::max();
  const XY p{0.0, 0.0};

  for (size_t i = 0; i < m_pils.size(); ++i) {
    const RoutePil& pil = m_pils[i];
    if (!pil.visible) continue;
    const XY a = ToLocalNm(pil.line_start_lat, pil.line_start_lon, lat, lon);
    const XY b = ToLocalNm(pil.line_end_lat, pil.line_end_lon, lat, lon);
    const double distance = DistancePointToSegmentNm(p, a, b);
    if (distance <= max_distance_nm && distance < best_distance) {
      best = static_cast<int>(i);
      best_distance = distance;
    }
  }
  return best;
}

void route_toolbar_pi::UpdateContextMenuHit() {
  // Menu entries stay visible; proximity is checked when a command is chosen.
}

void route_toolbar_pi::OnContextMenuItemCallback(int id) {
  if (!m_has_cursor && !m_has_context_click) return;

  wxWindow* parent = m_dialog ? static_cast<wxWindow*>(m_dialog)
                              : m_parent_window;

  const double click_lat =
      m_has_context_click ? m_context_click_lat : m_cursor_lat;
  const double click_lon =
      m_has_context_click ? m_context_click_lon : m_cursor_lon;

  wxPoint click_px;
  bool have_click_px = false;

  if (m_last_viewport) {
    GetCanvasPixLL(m_last_viewport, &click_px, click_lat, click_lon);
    have_click_px = true;
  }

  if (id == m_context_ebl_id) {
    int ebl = -1;

    // First test the exact visible yellow label rectangle.
    if (have_click_px && m_notes_visible) {
      for (int i = static_cast<int>(m_ebl_vrms.size()) - 1; i >= 0; --i) {
        const RouteEblVrm& item =
            m_ebl_vrms[static_cast<size_t>(i)];

        if (!item.visible) continue;

        if (EblLabelRect(m_last_viewport, item).Contains(click_px)) {
          ebl = i;
          break;
        }
      }
    }

    // Otherwise retain the existing geometry-based behaviour.
    if (ebl < 0)
      ebl = FindNearestEblGeometry(click_lat, click_lon, 0.05);

    if (ebl >= 0) {
      ShowEblTargetActions(ebl);
    } else {
      wxMessageBox(
          _("No Route Toolbar EBL/VRM label, start point, target or line "
            "was found close to the right-click position."),
          _("Route EBL / VRM"),
          wxOK | wxICON_INFORMATION,
          parent);
    }

    m_has_context_click = false;
    return;
  }

  if (id == m_context_pil_id) {
    int pil = -1;

    if (have_click_px && m_notes_visible) {
      for (int i = static_cast<int>(m_pils.size()) - 1; i >= 0; --i) {
        const RoutePil& item =
            m_pils[static_cast<size_t>(i)];

        if (!item.visible) continue;

        if (PilLabelRect(m_last_viewport, item).Contains(click_px)) {
          pil = i;
          break;
        }
      }
    }

    if (pil < 0)
      pil = FindNearestPil(click_lat, click_lon, 0.05);

    if (pil >= 0) {
      ShowPilActions(pil);
    } else {
      wxMessageBox(
          _("No Route Toolbar PIL label or line was found close to "
            "the right-click position."),
          _("Route PIL"),
          wxOK | wxICON_INFORMATION,
          parent);
    }

    m_has_context_click = false;
    return;
  }
}

void route_toolbar_pi::ShowPilActions(int index) {
  if (index < 0 || static_cast<size_t>(index) >= m_pils.size()) return;
  wxWindow* parent = m_dialog ? static_cast<wxWindow*>(m_dialog)
                              : m_parent_window;

  wxArrayString choices;
  choices.Add(_("Move graphically"));
  choices.Add(_("Change offset"));
  choices.Add(_("Delete PIL"));

  const RoutePil& current = m_pils[static_cast<size_t>(index)];
  const int selected = ShowPilActionButtons(
      parent,
      wxString::Format(_("PIL on leg %d: %+.2f NM. What do you want to do?"),
                       current.leg_index, current.offset_nm),
      choices);
  if (selected < 0) return;

  if (selected == 0) {
    BeginMovePilAnchor(index);
    return;
  }

  if (selected == 2) {
    DeletePil(index);
    return;
  }

  wxTextEntryDialog offset_dialog(
      parent,
      _("Enter signed offset in NM. Positive = starboard, negative = port."),
      _("Change Route PIL offset"),
      wxString::Format("%+.2f", current.offset_nm));
  if (offset_dialog.ShowModal() != wxID_OK) return;

  double new_offset = 0.0;
  if (!offset_dialog.GetValue().ToDouble(&new_offset) ||
      std::fabs(new_offset) < 0.005 || std::fabs(new_offset) > 5.0) {
    wxMessageBox(_("Enter an offset between -5.00 and +5.00 NM, excluding zero."),
                 _("Route PIL"), wxOK | wxICON_WARNING, parent);
    return;
  }

  RouteLegHit leg;
  leg.found = true;
  leg.start_lat = current.leg_start_lat;
  leg.start_lon = current.leg_start_lon;
  leg.end_lat = current.leg_end_lat;
  leg.end_lon = current.leg_end_lon;
  leg.course_true = current.course_true;
  leg.distance_nm = current.leg_distance_nm;
  leg.leg_index = current.leg_index;
  leg.route_guid = current.route_guid;
  leg.route_name = current.route_name;
  leg.from_guid = current.from_guid;
  leg.from_name = current.from_name;
  leg.to_guid = current.to_guid;
  leg.to_name = current.to_name;

  double mid_lat = 0.0;
  double mid_lon = 0.0;
  PositionBearingDistanceMercator_Plugin(
      leg.start_lat, leg.start_lon, leg.course_true, leg.distance_nm / 2.0,
      &mid_lat, &mid_lon);

  const double side_bearing = NormalizeBearing(
      leg.course_true + (new_offset >= 0.0 ? 90.0 : -90.0));
  double anchor_lat = 0.0;
  double anchor_lon = 0.0;
  PositionBearingDistanceMercator_Plugin(
      mid_lat, mid_lon, side_bearing, std::fabs(new_offset),
      &anchor_lat, &anchor_lon);

  RoutePil updated;
  if (!BuildPilFromAnchor(leg, anchor_lat, anchor_lon, updated)) return;
  m_pils[static_cast<size_t>(index)] = updated;
  SavePersistentState();
  if (m_parent_window) RequestRefresh(m_parent_window);
  SetStatus(wxString::Format(_("PIL updated to %+.2f NM (%s)."),
                             updated.offset_nm,
                             updated.offset_nm >= 0.0 ? _("Stbd") : _("Port")));
}

void route_toolbar_pi::DeletePil(int index) {
  if (index < 0 || static_cast<size_t>(index) >= m_pils.size()) return;
  wxWindow* parent = m_dialog ? static_cast<wxWindow*>(m_dialog)
                              : m_parent_window;
  const RoutePil& pil = m_pils[static_cast<size_t>(index)];

  wxMessageDialog confirm(
      parent,
      wxString::Format(_("Delete PIL on leg %d (%+.2f NM)?"),
                       pil.leg_index, pil.offset_nm),
      _("Route PIL"), wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
  if (confirm.ShowModal() != wxID_YES) return;

  m_pils.erase(m_pils.begin() + index);
  SavePersistentState();
  if (m_parent_window) RequestRefresh(m_parent_window);
  SetStatus(_("Route PIL deleted."));
}
