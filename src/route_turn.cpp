// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_pi.h"
#include "route_toolbar_dialog.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <wx/brush.h>
#include <wx/button.h>
#include <wx/choicdlg.h>
#include <wx/dc.h>
#include <wx/fileconf.h>
#include <wx/font.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/pen.h>
#include <wx/textdlg.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMinTurnRadiusNm = 0.005;
constexpr double kMaxTurnRadiusNm = 5.0;
constexpr double kMinTurnAngleDeg = 1.0;
constexpr double kMaxTurnAngleDeg = 175.0;
constexpr int kTurnArcSegments = 48;

double SignedCourseChange(double from_deg, double to_deg) {
  double delta = std::fmod(to_deg - from_deg + 540.0, 360.0) - 180.0;
  if (delta <= -180.0) delta += 360.0;
  return delta;
}

wxString TurnSide(double angle_deg) {
  return angle_deg >= 0.0 ? wxString("SB") : wxString("BB");
}

wxString RadiusText(double radius_nm) {
  return wxString::Format("%.2f", radius_nm);
}

void DrawLabel(wxDC& dc, const wxString& text, const wxPoint& pos) {
  const wxFont old_font = dc.GetFont();
  const wxPen old_pen = dc.GetPen();
  const wxBrush old_brush = dc.GetBrush();
  const wxColour old_text = dc.GetTextForeground();

  wxFont font(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL,
              wxFONTWEIGHT_NORMAL, false);
  dc.SetFont(font);
  const wxSize extent = dc.GetTextExtent(text);

  dc.SetPen(wxPen(wxColour(190, 145, 65), 1));
  dc.SetBrush(wxBrush(wxColour(255, 238, 180)));
  dc.DrawRectangle(pos.x - 4, pos.y - 3, extent.x + 8, extent.y + 6);
  dc.SetTextForeground(*wxBLACK);
  dc.DrawText(text, pos);

  dc.SetFont(old_font);
  dc.SetPen(old_pen);
  dc.SetBrush(old_brush);
  dc.SetTextForeground(old_text);
}


int ShowTurnActionButtons(wxWindow* parent, const wxString& prompt) {
  wxDialog dialog(parent, wxID_ANY, _("Route TURN"), wxDefaultPosition,
                  wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP);
  auto* top = new wxBoxSizer(wxVERTICAL);
  auto* text = new wxStaticText(&dialog, wxID_ANY, prompt);
  top->Add(text, 0, wxEXPAND | wxALL, 12);
  auto* change = new wxButton(&dialog, wxID_APPLY, _("Change radius"));
  auto* rudder = new wxButton(&dialog, wxID_ANY, _("Set planned rudder"));
  auto* remove = new wxButton(&dialog, wxID_DELETE, _("Delete TURN"));
  auto* cancel = new wxButton(&dialog, wxID_CANCEL, _("Cancel"));
  top->Add(change, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
  top->Add(rudder, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
  top->Add(remove, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
  top->Add(cancel, 0, wxALIGN_RIGHT | wxALL, 10);
  change->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) { dialog.EndModal(1); });
  rudder->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) { dialog.EndModal(2); });
  remove->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) { dialog.EndModal(3); });
  dialog.SetSizerAndFit(top);
  return dialog.ShowModal();
}


}  // namespace

void route_toolbar_pi::BeginTurn() {
  if (!m_dialog_visible) ShowToolWindow();

  m_ebl_mode = EblMode::Idle;
  m_pending_start = RouteWaypointHit{};
  m_edit_item_index = -1;

  m_pil_mode = PilMode::Idle;
  m_pending_leg = RouteLegHit{};
  m_edit_pil_index = -1;

  m_turn_mode = TurnMode::AwaitWaypoint;

  SetStatus(
      _("Route TURN: click an interior route waypoint. "
        "Enter the desired turn radius in NM. The plugin calculates the "
        "course change, geometric WO/turn-start point, tangent arc and "
        "turn-end point. Right-click to cancel."));

  if (m_parent_window) RequestRefresh(m_parent_window);
}

route_toolbar_pi::RouteWaypointHit
route_toolbar_pi::FindNearestInteriorRouteWaypoint(
    double lat, double lon, double max_distance_nm) const {
  RouteWaypointHit best;
  best.distance_nm = std::numeric_limits<double>::max();

  const wxArrayString route_guids = GetRouteGUIDArray();

  for (size_t r = 0; r < route_guids.GetCount(); ++r) {
    auto route = GetRoute_Plugin(route_guids[r]);
    if (!route || !route->pWaypointList) continue;

    std::vector<PlugIn_Waypoint*> waypoints;
    for (auto node = route->pWaypointList->GetFirst(); node;
         node = node->GetNext()) {
      if (node->GetData()) waypoints.push_back(node->GetData());
    }

    if (waypoints.size() < 3) continue;

    for (size_t i = 1; i + 1 < waypoints.size(); ++i) {
      PlugIn_Waypoint* wp = waypoints[i];
      if (!wp) continue;

      double distance_nm = 0.0;
      DistanceBearingMercator_Plugin(
          lat, lon, wp->m_lat, wp->m_lon, nullptr, &distance_nm);

      if (distance_nm > max_distance_nm ||
          distance_nm >= best.distance_nm)
        continue;

      best.found = true;
      best.is_mark = false;
      best.lat = wp->m_lat;
      best.lon = wp->m_lon;
      best.distance_nm = distance_nm;
      best.route_guid = route_guids[r];
      best.route_name = route->m_NameString;
      best.waypoint_guid = wp->m_GUID;
      best.waypoint_name = wp->m_MarkName;
    }
  }

  return best;
}

bool route_toolbar_pi::BuildTurnFromRoute(
    const wxString& route_guid,
    const wxString& waypoint_guid,
    double radius_nm,
    RouteTurn& item,
    wxString* error) const {
  auto fail = [&](const wxString& text) {
    if (error) *error = text;
    return false;
  };

  if (!std::isfinite(radius_nm) ||
      radius_nm < kMinTurnRadiusNm ||
      radius_nm > kMaxTurnRadiusNm) {
    return fail(
        wxString::Format(
            _("Turn radius must be between %.3f and %.1f NM."),
            kMinTurnRadiusNm, kMaxTurnRadiusNm));
  }

  auto route = GetRoute_Plugin(route_guid);
  if (!route || !route->pWaypointList)
    return fail(_("The OpenCPN route could not be found."));

  std::vector<PlugIn_Waypoint*> waypoints;
  for (auto node = route->pWaypointList->GetFirst(); node;
       node = node->GetNext()) {
    if (node->GetData()) waypoints.push_back(node->GetData());
  }

  size_t index = waypoints.size();
  for (size_t i = 0; i < waypoints.size(); ++i) {
    if (waypoints[i] && waypoints[i]->m_GUID == waypoint_guid) {
      index = i;
      break;
    }
  }

  if (index == waypoints.size())
    return fail(_("The selected waypoint is no longer present in the route."));

  if (index == 0 || index + 1 >= waypoints.size())
    return fail(
        _("TURN requires an interior waypoint with both an inbound and "
          "an outbound route leg."));

  PlugIn_Waypoint* prev = waypoints[index - 1];
  PlugIn_Waypoint* wp = waypoints[index];
  PlugIn_Waypoint* next = waypoints[index + 1];

  if (!prev || !wp || !next)
    return fail(_("Invalid waypoint geometry."));

  double inbound_course = 0.0;
  double inbound_distance = 0.0;
  double outbound_course = 0.0;
  double outbound_distance = 0.0;

  DistanceBearingMercator_Plugin(
      wp->m_lat, wp->m_lon,
      prev->m_lat, prev->m_lon,
      &inbound_course, &inbound_distance);

  DistanceBearingMercator_Plugin(
      next->m_lat, next->m_lon,
      wp->m_lat, wp->m_lon,
      &outbound_course, &outbound_distance);

  inbound_course = NormalizeBearing(inbound_course);
  outbound_course = NormalizeBearing(outbound_course);

  const double delta =
      SignedCourseChange(inbound_course, outbound_course);
  const double abs_delta = std::fabs(delta);

  if (abs_delta < kMinTurnAngleDeg)
    return fail(_("The course change is too small to create a useful turn."));

  if (abs_delta >= kMaxTurnAngleDeg)
    return fail(
        _("The course change is too close to 180°. "
          "A simple tangent-radius turn is not defined reliably here."));

  const double half_angle_rad = abs_delta * kPi / 360.0;
  const double tan_half = std::tan(half_angle_rad);

  if (!std::isfinite(tan_half) || tan_half <= 1e-9)
    return fail(_("Invalid turn geometry."));

  const double tangent_nm = radius_nm * tan_half;
  const double available_nm =
      std::min(inbound_distance, outbound_distance);

  if (!std::isfinite(tangent_nm) ||
      tangent_nm >= inbound_distance ||
      tangent_nm >= outbound_distance) {
    const double max_radius =
        std::max(0.0, available_nm / tan_half);
    return fail(
        wxString::Format(
            _("Radius %.3f NM does not fit on the adjacent route legs.\n\n"
              "Required distance from WP: %.3f NM\n"
              "Available shortest leg: %.3f NM\n"
              "Maximum theoretical radius here: %.3f NM"),
            radius_nm, tangent_nm, available_nm, max_radius));
  }

  RouteTurn built;
  built.route_guid = route_guid;
  built.route_name = route->m_NameString;
  built.waypoint_guid = wp->m_GUID;
  built.waypoint_name = wp->m_MarkName;
  built.prev_guid = prev->m_GUID;
  built.prev_name = prev->m_MarkName;
  built.next_guid = next->m_GUID;
  built.next_name = next->m_MarkName;

  built.waypoint_lat = wp->m_lat;
  built.waypoint_lon = wp->m_lon;
  built.inbound_course_true = inbound_course;
  built.outbound_course_true = outbound_course;
  built.inbound_leg_nm = inbound_distance;
  built.outbound_leg_nm = outbound_distance;
  built.turn_angle_deg = delta;
  built.radius_nm = radius_nm;
  built.tangent_nm = tangent_nm;

  PositionBearingDistanceMercator_Plugin(
      wp->m_lat, wp->m_lon,
      NormalizeBearing(inbound_course + 180.0),
      tangent_nm,
      &built.start_lat, &built.start_lon);

  PositionBearingDistanceMercator_Plugin(
      wp->m_lat, wp->m_lon,
      outbound_course,
      tangent_nm,
      &built.end_lat, &built.end_lon);

  const double center_bearing =
      NormalizeBearing(inbound_course + (delta > 0.0 ? 90.0 : -90.0));

  PositionBearingDistanceMercator_Plugin(
      built.start_lat, built.start_lon,
      center_bearing, radius_nm,
      &built.center_lat, &built.center_lon);

  item = built;
  if (error) error->clear();
  return true;
}

int route_toolbar_pi::FindTurnByWaypoint(
    const wxString& route_guid,
    const wxString& waypoint_guid) const {
  for (size_t i = 0; i < m_turns.size(); ++i) {
    if (m_turns[i].route_guid == route_guid &&
        m_turns[i].waypoint_guid == waypoint_guid)
      return static_cast<int>(i);
  }
  return -1;
}

void route_toolbar_pi::CreateOrEditTurn(const RouteWaypointHit& hit) {
  if (!hit.found || hit.route_guid.IsEmpty() ||
      hit.waypoint_guid.IsEmpty())
    return;

  const int existing =
      FindTurnByWaypoint(hit.route_guid, hit.waypoint_guid);

  double existing_rudder_deg = 0.0;

  if (existing >= 0) {
    wxString wp_name = hit.waypoint_name;
    if (wp_name.IsEmpty()) wp_name = _("Waypoint");

    existing_rudder_deg =
        m_turns[static_cast<size_t>(existing)].rudder_angle_deg;

    const int action = ShowTurnActionButtons(
        m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
        _("A TURN already exists at ") + wp_name + _("."));

    if (action <= 0) return;

    if (action == 3) {
      DeleteTurn(existing);
      return;
    }

    if (action == 2) {
      wxTextEntryDialog rudder_dialog(
          m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
          _("Enter planned rudder angle in degrees. This is a manual planning value, not calculated by Route Toolbar. Enter 0 to hide it."),
          _("Planned rudder — ") + wp_name,
          wxString::Format("%.0f", existing_rudder_deg));

      if (rudder_dialog.ShowModal() != wxID_OK) return;

      wxString rudder_text = rudder_dialog.GetValue();
      rudder_text.Trim(true).Trim(false);
      rudder_text.Replace(",", ".");

      double rudder_deg = 0.0;
      if (!rudder_text.ToDouble(&rudder_deg) ||
          !std::isfinite(rudder_deg) ||
          rudder_deg < 0.0 || rudder_deg > 45.0) {
        wxMessageBox(
            _("Enter a planned rudder angle between 0 and 45 degrees."),
            _("Route TURN"), wxOK | wxICON_WARNING,
            m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window);
        return;
      }

      m_turns[static_cast<size_t>(existing)].rudder_angle_deg = rudder_deg;
      SavePersistentState();
      if (m_parent_window) RequestRefresh(m_parent_window);
      SetStatus(wxString::Format(
          _("Planned rudder at %s: %.0f° %s."),
          wp_name, rudder_deg,
          TurnSide(m_turns[static_cast<size_t>(existing)].turn_angle_deg)));
      return;
    }
  }

  const double default_radius =
      existing >= 0
          ? m_turns[static_cast<size_t>(existing)].radius_nm
          : m_default_turn_radius_nm;

  wxString wp_name = hit.waypoint_name;
  if (wp_name.IsEmpty()) wp_name = _("Waypoint");

  wxTextEntryDialog radius_dialog(
      m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
      _("Enter desired geometric turn radius in nautical miles.\n"
        "Examples: 0.05, 0.10, 0.20, 0.30"),
      _("TURN radius — ") + wp_name,
      RadiusText(default_radius));

  if (radius_dialog.ShowModal() != wxID_OK) return;

  wxString text = radius_dialog.GetValue();
  text.Trim(true).Trim(false);
  text.Replace(",", ".");

  double radius_nm = 0.0;
  if (!text.ToDouble(&radius_nm) ||
      !std::isfinite(radius_nm) ||
      radius_nm < kMinTurnRadiusNm ||
      radius_nm > kMaxTurnRadiusNm) {
    wxMessageBox(
        _("Invalid radius. Enter a value in NM, for example 0.10."),
        _("Route TURN"), wxOK | wxICON_WARNING,
        m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window);
    return;
  }

  double planned_rudder_deg = existing_rudder_deg;

  if (existing < 0) {
    wxTextEntryDialog rudder_dialog(
        m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
        _("Optional: enter planned rudder angle in degrees. This value is manual and is not calculated from the turn radius. Enter 0 to omit it."),
        _("Planned rudder — ") + wp_name,
        "0");

    if (rudder_dialog.ShowModal() != wxID_OK) return;

    wxString rudder_text = rudder_dialog.GetValue();
    rudder_text.Trim(true).Trim(false);
    rudder_text.Replace(",", ".");

    if (!rudder_text.ToDouble(&planned_rudder_deg) ||
        !std::isfinite(planned_rudder_deg) ||
        planned_rudder_deg < 0.0 || planned_rudder_deg > 45.0) {
      wxMessageBox(
          _("Enter a planned rudder angle between 0 and 45 degrees."),
          _("Route TURN"), wxOK | wxICON_WARNING,
          m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window);
      return;
    }
  }

  RouteTurn turn;
  wxString error;
  if (!BuildTurnFromRoute(
          hit.route_guid, hit.waypoint_guid,
          radius_nm, turn, &error)) {
    wxMessageBox(error, _("Route TURN"),
                 wxOK | wxICON_WARNING,
                 m_dialog ? static_cast<wxWindow*>(m_dialog)
                          : m_parent_window);
    SetStatus(error);
    return;
  }

  turn.rudder_angle_deg = planned_rudder_deg;
  if (existing >= 0)
    turn.visible = m_turns[static_cast<size_t>(existing)].visible;

  if (existing >= 0)
    m_turns[static_cast<size_t>(existing)] = turn;
  else
    m_turns.push_back(turn);

  m_default_turn_radius_nm = radius_nm;
  SavePersistentState();

  SetStatus(
      wxString::Format(
          _("TURN %s saved: %03.0f°T → %03.0f°T, "
            "Δ %.0f° %s, R %.2f NM. "
            "WO %.3f NM before WP; turn end %.3f NM after WP. "
            "Click another waypoint or right-click to finish."),
          wp_name,
          turn.inbound_course_true,
          turn.outbound_course_true,
          std::fabs(turn.turn_angle_deg),
          TurnSide(turn.turn_angle_deg),
          turn.radius_nm,
          turn.tangent_nm,
          turn.tangent_nm));

  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::DeleteTurn(int index) {
  if (index < 0 ||
      static_cast<size_t>(index) >= m_turns.size())
    return;

  const RouteTurn& turn = m_turns[static_cast<size_t>(index)];
  wxString wp_name = turn.waypoint_name;
  if (wp_name.IsEmpty()) wp_name = _("Waypoint");

  wxMessageDialog confirm(
      m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
      _("Delete TURN at ") + wp_name + _("?"),
      _("Route TURN"),
      wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);

  if (confirm.ShowModal() != wxID_YES) return;

  m_turns.erase(m_turns.begin() + index);
  SavePersistentState();
  SetStatus(_("TURN deleted."));

  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::DrawTurn(
    wxDC& dc, PlugIn_ViewPort* vp,
    const RouteTurn& item) const {
  if (!vp) return;

  double radial_start = 0.0;
  double radial_distance = 0.0;
  DistanceBearingMercator_Plugin(
      item.start_lat, item.start_lon,
      item.center_lat, item.center_lon,
      &radial_start, &radial_distance);
  radial_start = NormalizeBearing(radial_start);

  std::vector<wxPoint> points;
  points.reserve(kTurnArcSegments + 1);

  for (int i = 0; i <= kTurnArcSegments; ++i) {
    const double t =
        static_cast<double>(i) / kTurnArcSegments;
    const double bearing =
        NormalizeBearing(
            radial_start + item.turn_angle_deg * t);

    double lat = 0.0;
    double lon = 0.0;
    PositionBearingDistanceMercator_Plugin(
        item.center_lat, item.center_lon,
        bearing, item.radius_nm, &lat, &lon);

    wxPoint px;
    GetCanvasPixLL(vp, &px, lat, lon);
    points.push_back(px);
  }

  wxPoint start_px;
  wxPoint end_px;
  wxPoint wp_px;
  GetCanvasPixLL(vp, &start_px, item.start_lat, item.start_lon);
  GetCanvasPixLL(vp, &end_px, item.end_lat, item.end_lon);
  GetCanvasPixLL(vp, &wp_px, item.waypoint_lat, item.waypoint_lon);

  const wxPen old_pen = dc.GetPen();
  const wxBrush old_brush = dc.GetBrush();

  dc.SetPen(wxPen(m_turn_colour, 3, wxPENSTYLE_SOLID));
  dc.SetBrush(*wxTRANSPARENT_BRUSH);

  for (size_t i = 1; i < points.size(); ++i)
    dc.DrawLine(points[i - 1], points[i]);

  dc.SetBrush(wxBrush(wxColour(255, 238, 180)));
  dc.DrawCircle(start_px, 5);

  wxString turn_label =
      wxString::Format("WO · ΔK %.0f° %s · R %.2f NM",
                       std::fabs(item.turn_angle_deg),
                       TurnSide(item.turn_angle_deg),
                       item.radius_nm);
  if (item.rudder_angle_deg > 0.0)
    turn_label += wxString::Format(" · Rudder %.0f° %s",
                                   item.rudder_angle_deg,
                                   TurnSide(item.turn_angle_deg));

  DrawLabel(dc, turn_label,
            wxPoint(start_px.x + 10, start_px.y + 8));

  dc.SetPen(old_pen);
  dc.SetBrush(old_brush);
}

void route_toolbar_pi::DrawTurnGL(
    PlugIn_ViewPort* vp,
    const RouteTurn& item) const {
  if (!vp) return;

  double radial_start = 0.0;
  double radial_distance = 0.0;
  DistanceBearingMercator_Plugin(
      item.start_lat, item.start_lon,
      item.center_lat, item.center_lon,
      &radial_start, &radial_distance);
  radial_start = NormalizeBearing(radial_start);

  glDisable(GL_TEXTURE_2D);
  glColor4ub(m_turn_colour.Red(),
             m_turn_colour.Green(),
             m_turn_colour.Blue(), 255);
  glLineWidth(3.0f);

  glBegin(GL_LINE_STRIP);
  for (int i = 0; i <= kTurnArcSegments; ++i) {
    const double t =
        static_cast<double>(i) / kTurnArcSegments;
    const double bearing =
        NormalizeBearing(radial_start + item.turn_angle_deg * t);

    double lat = 0.0;
    double lon = 0.0;
    PositionBearingDistanceMercator_Plugin(
        item.center_lat, item.center_lon,
        bearing, item.radius_nm, &lat, &lon);

    wxPoint px;
    GetCanvasPixLL(vp, &px, lat, lon);
    glVertex2i(px.x, px.y);
  }
  glEnd();

  wxPoint start_px;
  wxPoint end_px;
  wxPoint wp_px;
  GetCanvasPixLL(vp, &start_px, item.start_lat, item.start_lon);
  GetCanvasPixLL(vp, &end_px, item.end_lat, item.end_lon);
  GetCanvasPixLL(vp, &wp_px, item.waypoint_lat, item.waypoint_lon);

  auto ring = [](const wxPoint& p) {
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 24; ++i) {
      const double a =
          2.0 * kPi * static_cast<double>(i) / 24.0;
      glVertex2d(p.x + std::cos(a) * 5.0,
                 p.y + std::sin(a) * 5.0);
    }
    glEnd();
  };

  ring(start_px);

  wxString turn_label =
      wxString::Format("WO · ΔK %.0f° %s · R %.2f NM",
                       std::fabs(item.turn_angle_deg),
                       TurnSide(item.turn_angle_deg),
                       item.radius_nm);
  if (item.rudder_angle_deg > 0.0)
    turn_label += wxString::Format(" · Rudder %.0f° %s",
                                   item.rudder_angle_deg,
                                   TurnSide(item.turn_angle_deg));

  DrawGlLabel(turn_label, start_px.x + 10, start_px.y + 8);
}

void route_toolbar_pi::LoadTurnPersistentState(
    wxFileConfig* config) {
  m_turns.clear();
  if (!config) return;

  config->SetPath("/PlugIns/RouteToolbar/TURN");

  config->Read("DefaultRadiusNm",
               &m_default_turn_radius_nm, 0.10);

  if (!std::isfinite(m_default_turn_radius_nm) ||
      m_default_turn_radius_nm < kMinTurnRadiusNm ||
      m_default_turn_radius_nm > kMaxTurnRadiusNm)
    m_default_turn_radius_nm = 0.10;

  long count = 0;
  config->Read("Count", &count, 0L);
  count = std::clamp(count, 0L, 1000L);

  for (long i = 0; i < count; ++i) {
    const wxString prefix =
        wxString::Format("Item%03ld/", i);

    wxString route_guid;
    wxString waypoint_guid;
    double radius_nm = 0.10;

    config->Read(prefix + "RouteGuid",
                 &route_guid, wxEmptyString);
    config->Read(prefix + "WaypointGuid",
                 &waypoint_guid, wxEmptyString);
    config->Read(prefix + "RadiusNm",
                 &radius_nm, 0.10);

    bool visible = true;
    double rudder_angle_deg = 0.0;
    config->Read(prefix + "Visible", &visible, true);
    config->Read(prefix + "RudderAngleDeg",
                 &rudder_angle_deg, 0.0);

    if (route_guid.IsEmpty() ||
        waypoint_guid.IsEmpty())
      continue;

    RouteTurn rebuilt;
    wxString error;
    if (BuildTurnFromRoute(route_guid, waypoint_guid,
                           radius_nm, rebuilt, &error)) {
      rebuilt.visible = visible;
      rebuilt.rudder_angle_deg =
          std::clamp(rudder_angle_deg, 0.0, 45.0);
      m_turns.push_back(rebuilt);
    }
  }
}

void route_toolbar_pi::SaveTurnPersistentState(
    wxFileConfig* config) const {
  if (!config) return;

  config->SetPath("/PlugIns/RouteToolbar");
  config->DeleteGroup("TURN");
  config->SetPath("TURN");

  config->Write("DefaultRadiusNm",
                m_default_turn_radius_nm);
  config->Write("Count",
                static_cast<long>(m_turns.size()));

  for (size_t i = 0; i < m_turns.size(); ++i) {
    const wxString prefix =
        wxString::Format(
            "Item%03lu/",
            static_cast<unsigned long>(i));

    const RouteTurn& item = m_turns[i];

    config->Write(prefix + "RouteGuid",
                  item.route_guid);
    config->Write(prefix + "RouteName",
                  item.route_name);
    config->Write(prefix + "WaypointGuid",
                  item.waypoint_guid);
    config->Write(prefix + "WaypointName",
                  item.waypoint_name);
    config->Write(prefix + "RadiusNm",
                  item.radius_nm);
    config->Write(prefix + "Visible",
                  item.visible);
    config->Write(prefix + "RudderAngleDeg",
                  item.rudder_angle_deg);
  }
}
