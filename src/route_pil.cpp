// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_pi.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <wx/brush.h>
#include <wx/dc.h>
#include <wx/fileconf.h>
#include <wx/pen.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kPilLegPickRadiusNm = 0.08;
constexpr double kPilMinimumOffsetNm = 0.005;
constexpr double kPilMaxOffsetNm = 5.0;
constexpr int kPilDashSegments = 28;

struct XY {
  double x{0.0};  // east, NM
  double y{0.0};  // north, NM
};

XY ToLocalNm(double lat, double lon, double ref_lat, double ref_lon) {
  const double cos_lat = std::max(0.01, std::cos(ref_lat * kPi / 180.0));
  return {(lon - ref_lon) * 60.0 * cos_lat,
          (lat - ref_lat) * 60.0};
}

double DistancePointToSegmentNm(const XY& p, const XY& a, const XY& b) {
  const double dx = b.x - a.x;
  const double dy = b.y - a.y;
  const double len2 = dx * dx + dy * dy;
  if (len2 < 1e-12) return std::hypot(p.x - a.x, p.y - a.y);
  const double t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / len2,
                              0.0, 1.0);
  const double cx = a.x + t * dx;
  const double cy = a.y + t * dy;
  return std::hypot(p.x - cx, p.y - cy);
}

wxString PilSide(double offset_nm) {
  return offset_nm >= 0.0 ? wxString("Stbd") : wxString("Port");
}

wxString PilLabel(double offset_nm) {
  return wxString::Format("PIL %+.2f NM %s", offset_nm, PilSide(offset_nm));
}
}  // namespace

void route_toolbar_pi::BeginPil() {
  if (!m_dialog_visible) ShowToolWindow();
  m_ebl_mode = EblMode::Idle;
  m_pending_start = RouteWaypointHit{};
  m_edit_item_index = -1;
  m_edit_pil_index = -1;
  m_pending_leg = RouteLegHit{};
  m_pil_mode = PilMode::AwaitLeg;
  SetStatus(_("Route PIL: click a route leg. Then click the chart object / point where the parallel index line should pass. Right-click to cancel."));
  if (m_parent_window) RequestRefresh(m_parent_window);
}

route_toolbar_pi::RouteLegHit route_toolbar_pi::FindNearestRouteLeg(
    double lat, double lon, double max_distance_nm) const {
  RouteLegHit best;
  best.pick_distance_nm = std::numeric_limits<double>::max();

  const wxArrayString route_guids = GetRouteGUIDArray();
  for (size_t r = 0; r < route_guids.GetCount(); ++r) {
    auto route = GetRoute_Plugin(route_guids[r]);
    if (!route || !route->pWaypointList) continue;

    std::vector<PlugIn_Waypoint*> waypoints;
    for (auto node = route->pWaypointList->GetFirst(); node;
         node = node->GetNext()) {
      if (node->GetData()) waypoints.push_back(node->GetData());
    }

    for (size_t i = 0; i + 1 < waypoints.size(); ++i) {
      PlugIn_Waypoint* from = waypoints[i];
      PlugIn_Waypoint* to = waypoints[i + 1];
      const XY p{0.0, 0.0};
      const XY a = ToLocalNm(from->m_lat, from->m_lon, lat, lon);
      const XY b = ToLocalNm(to->m_lat, to->m_lon, lat, lon);
      const double distance = DistancePointToSegmentNm(p, a, b);
      if (distance > max_distance_nm || distance >= best.pick_distance_nm)
        continue;

      double course = 0.0;
      double leg_distance = 0.0;
      DistanceBearingMercator_Plugin(to->m_lat, to->m_lon,
                                     from->m_lat, from->m_lon,
                                     &course, &leg_distance);

      best.found = true;
      best.pick_distance_nm = distance;
      best.start_lat = from->m_lat;
      best.start_lon = from->m_lon;
      best.end_lat = to->m_lat;
      best.end_lon = to->m_lon;
      best.course_true = NormalizeBearing(course);
      best.distance_nm = leg_distance;
      best.leg_index = static_cast<int>(i + 1);
      best.route_guid = route_guids[r];
      best.route_name = route->m_NameString;
      best.from_guid = from->m_GUID;
      best.from_name = from->m_MarkName;
      best.to_guid = to->m_GUID;
      best.to_name = to->m_MarkName;
    }
  }

  return best;
}

bool route_toolbar_pi::BuildPilFromAnchor(const RouteLegHit& leg,
                                          double anchor_lat,
                                          double anchor_lon,
                                          RoutePil& item) const {
  if (!leg.found) return false;

  const XY anchor = ToLocalNm(anchor_lat, anchor_lon,
                              leg.start_lat, leg.start_lon);
  const double theta = leg.course_true * kPi / 180.0;
  const double right_east = std::cos(theta);
  const double right_north = -std::sin(theta);
  const double signed_offset =
      anchor.x * right_east + anchor.y * right_north;

  if (!std::isfinite(signed_offset) ||
      std::fabs(signed_offset) < kPilMinimumOffsetNm ||
      std::fabs(signed_offset) > kPilMaxOffsetNm)
    return false;

  item = RoutePil{};
  item.leg_start_lat = leg.start_lat;
  item.leg_start_lon = leg.start_lon;
  item.leg_end_lat = leg.end_lat;
  item.leg_end_lon = leg.end_lon;
  item.anchor_lat = anchor_lat;
  item.anchor_lon = anchor_lon;
  item.course_true = leg.course_true;
  item.leg_distance_nm = leg.distance_nm;
  item.offset_nm = signed_offset;
  item.leg_index = leg.leg_index;
  item.route_guid = leg.route_guid;
  item.route_name = leg.route_name;
  item.from_guid = leg.from_guid;
  item.from_name = leg.from_name;
  item.to_guid = leg.to_guid;
  item.to_name = leg.to_name;

  const double offset_bearing = NormalizeBearing(
      leg.course_true + (signed_offset >= 0.0 ? 90.0 : -90.0));
  const double offset_distance = std::fabs(signed_offset);

  double shifted_start_lat = 0.0;
  double shifted_start_lon = 0.0;
  double shifted_end_lat = 0.0;
  double shifted_end_lon = 0.0;
  PositionBearingDistanceMercator_Plugin(
      leg.start_lat, leg.start_lon, offset_bearing, offset_distance,
      &shifted_start_lat, &shifted_start_lon);
  PositionBearingDistanceMercator_Plugin(
      leg.end_lat, leg.end_lon, offset_bearing, offset_distance,
      &shifted_end_lat, &shifted_end_lon);

  const double extension = std::clamp(leg.distance_nm * 0.20, 0.08, 0.35);
  PositionBearingDistanceMercator_Plugin(
      shifted_start_lat, shifted_start_lon,
      NormalizeBearing(leg.course_true + 180.0), extension,
      &item.line_start_lat, &item.line_start_lon);
  PositionBearingDistanceMercator_Plugin(
      shifted_end_lat, shifted_end_lon, leg.course_true, extension,
      &item.line_end_lat, &item.line_end_lon);

  return true;
}

void route_toolbar_pi::CompletePil(double anchor_lat, double anchor_lon) {
  RoutePil item;
  if (!BuildPilFromAnchor(m_pending_leg, anchor_lat, anchor_lon, item)) {
    SetStatus(_("PIL offset is too small or invalid. Click a point clearly to port or starboard of the selected route leg, or right-click to cancel."));
    return;
  }

  if (m_edit_pil_index >= 0 &&
      static_cast<size_t>(m_edit_pil_index) < m_pils.size()) {
    m_pils[static_cast<size_t>(m_edit_pil_index)] = item;
    SavePersistentState();
    SetStatus(wxString::Format(_("PIL moved to %+.2f NM (%s)."),
                               item.offset_nm, PilSide(item.offset_nm)));
    m_edit_pil_index = -1;
    m_pending_leg = RouteLegHit{};
    m_pil_mode = PilMode::Idle;
    if (m_parent_window) RequestRefresh(m_parent_window);
    return;
  }

  m_pils.push_back(item);
  SavePersistentState();

  wxString from = item.from_name;
  wxString to = item.to_name;
  if (from.IsEmpty()) from = wxString::Format("WP%d", item.leg_index);
  if (to.IsEmpty()) to = wxString::Format("WP%d", item.leg_index + 1);
  SetStatus(wxString::Format(
      _("PIL %s → %s saved: %+.2f NM %s. Click another route leg, or right-click to finish."),
      from, to, item.offset_nm, PilSide(item.offset_nm)));

  m_pending_leg = RouteLegHit{};
  m_pil_mode = PilMode::AwaitLeg;
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::BeginMovePilAnchor(int index) {
  if (index < 0 || static_cast<size_t>(index) >= m_pils.size()) return;

  const RoutePil& current = m_pils[static_cast<size_t>(index)];
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

  m_ebl_mode = EblMode::Idle;
  m_edit_item_index = -1;
  m_pending_leg = leg;
  m_edit_pil_index = index;

  // Reuse the normal live PIL preview: the line follows the cursor and the
  // next click commits the new offset, replacing the existing PIL.
  m_pil_mode = PilMode::AwaitOffsetPoint;
  SetStatus(_("Move PIL graphically: move the cursor to the new offset and click to save. Right-click to cancel."));
  if (m_parent_window) RequestRefresh(m_parent_window);
}

bool route_toolbar_pi::CommitMovePilAnchor(double anchor_lat,
                                           double anchor_lon) {
  if (m_edit_pil_index < 0 ||
      static_cast<size_t>(m_edit_pil_index) >= m_pils.size() ||
      !m_pending_leg.found) {
    m_pil_mode = PilMode::Idle;
    m_pending_leg = RouteLegHit{};
    m_edit_pil_index = -1;
    return true;
  }

  RoutePil moved;
  if (!BuildPilFromAnchor(m_pending_leg, anchor_lat, anchor_lon, moved)) {
    SetStatus(_("PIL offset is too small or invalid. Move farther from the route leg, or right-click to cancel."));
    return true;
  }

  m_pils[static_cast<size_t>(m_edit_pil_index)] = moved;
  SavePersistentState();
  SetStatus(wxString::Format(_("PIL moved to %+.2f NM (%s)."),
                             moved.offset_nm, PilSide(moved.offset_nm)));

  m_pil_mode = PilMode::Idle;
  m_pending_leg = RouteLegHit{};
  m_edit_pil_index = -1;
  if (m_parent_window) RequestRefresh(m_parent_window);
  return true;
}

void route_toolbar_pi::DrawPil(wxDC& dc, PlugIn_ViewPort* vp,
                               const RoutePil& item, bool preview) const {
  wxPoint start_px;
  wxPoint end_px;
  wxPoint anchor_px;
  GetCanvasPixLL(vp, &start_px, item.line_start_lat, item.line_start_lon);
  GetCanvasPixLL(vp, &end_px, item.line_end_lat, item.line_end_lon);
  GetCanvasPixLL(vp, &anchor_px, item.anchor_lat, item.anchor_lon);

  const wxPen old_pen = dc.GetPen();
  const wxBrush old_brush = dc.GetBrush();
  const wxFont old_font = dc.GetFont();
  const wxColour old_text = dc.GetTextForeground();

  const wxColour colour = preview ? wxColour(40, 135, 150)
                                  : wxColour(0, 105, 125);
  dc.SetPen(wxPen(colour, preview ? 1 : 2, wxPENSTYLE_LONG_DASH));
  dc.SetBrush(*wxTRANSPARENT_BRUSH);
  dc.DrawLine(start_px, end_px);
  dc.DrawCircle(anchor_px, preview ? 4 : 6);

  const wxString label = PilLabel(item.offset_nm);
  dc.SetFont(GetEblLabelFont());
  const wxSize text = dc.GetTextExtent(label);
  const wxPoint pos(anchor_px.x + 12, anchor_px.y - text.y - 10);
  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.SetBrush(wxBrush(wxColour(205, 245, 245)));
  dc.DrawRectangle(pos.x - 4, pos.y - 3, text.x + 8, text.y + 6);
  dc.SetTextForeground(*wxBLACK);
  dc.DrawText(label, pos);

  dc.SetPen(old_pen);
  dc.SetBrush(old_brush);
  dc.SetFont(old_font);
  dc.SetTextForeground(old_text);
}

void route_toolbar_pi::DrawPilGL(PlugIn_ViewPort* vp, const RoutePil& item,
                                 bool preview) const {
  wxPoint start_px;
  wxPoint end_px;
  wxPoint anchor_px;
  GetCanvasPixLL(vp, &start_px, item.line_start_lat, item.line_start_lon);
  GetCanvasPixLL(vp, &end_px, item.line_end_lat, item.line_end_lon);
  GetCanvasPixLL(vp, &anchor_px, item.anchor_lat, item.anchor_lon);

  if (preview)
    glColor4ub(40, 135, 150, 190);
  else
    glColor4ub(0, 105, 125, 255);

  glDisable(GL_TEXTURE_2D);
  glLineWidth(preview ? 1.0f : 2.0f);

  const double dx = static_cast<double>(end_px.x - start_px.x);
  const double dy = static_cast<double>(end_px.y - start_px.y);
  glBegin(GL_LINES);
  for (int i = 0; i < kPilDashSegments; i += 2) {
    const double t0 = static_cast<double>(i) / kPilDashSegments;
    const double t1 = static_cast<double>(i + 1) / kPilDashSegments;
    glVertex2d(start_px.x + dx * t0, start_px.y + dy * t0);
    glVertex2d(start_px.x + dx * t1, start_px.y + dy * t1);
  }
  glEnd();

  const int ring = preview ? 4 : 6;
  glBegin(GL_LINE_LOOP);
  for (int i = 0; i < 20; ++i) {
    const double angle = 2.0 * kPi * i / 20.0;
    glVertex2d(anchor_px.x + std::cos(angle) * ring,
               anchor_px.y + std::sin(angle) * ring);
  }
  glEnd();

  DrawGlLabel(PilLabel(item.offset_nm), anchor_px.x + 12,
              anchor_px.y - 30);
}

void route_toolbar_pi::LoadPilPersistentState(wxFileConfig* config) {
  m_pils.clear();
  if (!config) return;

  config->SetPath("/PlugIns/RouteToolbar/PIL");
  long count = 0;
  config->Read("Count", &count, 0L);
  count = std::clamp(count, 0L, 1000L);

  for (long i = 0; i < count; ++i) {
    const wxString prefix = wxString::Format("Item%03ld/", i);
    RoutePil item;
    if (!config->Read(prefix + "LegStartLat", &item.leg_start_lat) ||
        !config->Read(prefix + "LegStartLon", &item.leg_start_lon) ||
        !config->Read(prefix + "LegEndLat", &item.leg_end_lat) ||
        !config->Read(prefix + "LegEndLon", &item.leg_end_lon) ||
        !config->Read(prefix + "AnchorLat", &item.anchor_lat) ||
        !config->Read(prefix + "AnchorLon", &item.anchor_lon))
      continue;

    long leg_index = 0;
    config->Read(prefix + "LegIndex", &leg_index, 0L);
    item.leg_index = static_cast<int>(leg_index);
    config->Read(prefix + "RouteGuid", &item.route_guid, wxEmptyString);
    config->Read(prefix + "RouteName", &item.route_name, wxEmptyString);
    config->Read(prefix + "FromGuid", &item.from_guid, wxEmptyString);
    config->Read(prefix + "FromName", &item.from_name, wxEmptyString);
    config->Read(prefix + "ToGuid", &item.to_guid, wxEmptyString);
    config->Read(prefix + "ToName", &item.to_name, wxEmptyString);

    RouteLegHit leg;
    leg.found = true;
    leg.start_lat = item.leg_start_lat;
    leg.start_lon = item.leg_start_lon;
    leg.end_lat = item.leg_end_lat;
    leg.end_lon = item.leg_end_lon;
    leg.leg_index = item.leg_index;
    leg.route_guid = item.route_guid;
    leg.route_name = item.route_name;
    leg.from_guid = item.from_guid;
    leg.from_name = item.from_name;
    leg.to_guid = item.to_guid;
    leg.to_name = item.to_name;
    DistanceBearingMercator_Plugin(
        leg.end_lat, leg.end_lon, leg.start_lat, leg.start_lon,
        &leg.course_true, &leg.distance_nm);
    leg.course_true = NormalizeBearing(leg.course_true);

    RoutePil rebuilt;
    if (BuildPilFromAnchor(leg, item.anchor_lat, item.anchor_lon, rebuilt))
      m_pils.push_back(rebuilt);
  }
}

void route_toolbar_pi::SavePilPersistentState(wxFileConfig* config) const {
  if (!config) return;

  config->SetPath("/PlugIns/RouteToolbar");
  config->DeleteGroup("PIL");
  config->SetPath("PIL");
  config->Write("Count", static_cast<long>(m_pils.size()));

  for (size_t i = 0; i < m_pils.size(); ++i) {
    const wxString prefix = wxString::Format(
        "Item%03lu/", static_cast<unsigned long>(i));
    const RoutePil& item = m_pils[i];
    config->Write(prefix + "LegStartLat", item.leg_start_lat);
    config->Write(prefix + "LegStartLon", item.leg_start_lon);
    config->Write(prefix + "LegEndLat", item.leg_end_lat);
    config->Write(prefix + "LegEndLon", item.leg_end_lon);
    config->Write(prefix + "AnchorLat", item.anchor_lat);
    config->Write(prefix + "AnchorLon", item.anchor_lon);
    config->Write(prefix + "LegIndex", static_cast<long>(item.leg_index));
    config->Write(prefix + "RouteGuid", item.route_guid);
    config->Write(prefix + "RouteName", item.route_name);
    config->Write(prefix + "FromGuid", item.from_guid);
    config->Write(prefix + "FromName", item.from_name);
    config->Write(prefix + "ToGuid", item.to_guid);
    config->Write(prefix + "ToName", item.to_name);
  }
}
