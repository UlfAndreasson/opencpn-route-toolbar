// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_pi.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#include <wx/app.h>
#include <wx/brush.h>
#include <wx/choicdlg.h>
#include <wx/dc.h>
#include <wx/dcmemory.h>
#include <wx/ffile.h>
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/font.h>
#include <wx/image.h>
#include <wx/intl.h>
#include <wx/log.h>
#include <wx/msgdlg.h>
#include <wx/pen.h>
#include <wx/stdpaths.h>
#include <wx/textdlg.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "route_toolbar_dialog.h"

#ifndef DECL_EXP
#if defined(__WXMSW__) || defined(__CYGWIN__)
#define DECL_EXP __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#define DECL_EXP __attribute__((visibility("default")))
#else
#define DECL_EXP
#endif
#endif

namespace {
constexpr int kPluginVersionMajor = 0;
constexpr int kPluginVersionMinor = 3;
constexpr int kPluginVersionPatch = 3;
constexpr double kWaypointPickRadiusNm = 0.10;
constexpr double kTargetPickRadiusNm = 0.025;
constexpr double kDefaultArcWidthDeg = 30.0;
constexpr int kArcSegments = 30;
constexpr int kTargetRingRadiusPx = 7;
constexpr int kTargetCrosshairRadiusPx = 11;
constexpr int kLabelOffsetXPx = 18;
constexpr int kLabelOffsetYPx = 18;
constexpr int kDefaultLabelFontSize = 14;
constexpr int kMinLabelFontSize = 8;
constexpr int kMaxLabelFontSize = 30;
constexpr const char* kConfigPath = "/PlugIns/RouteToolbar";

static const char* route_toolbar_xpm[] = {
    "16 16 2 1",
    "  c None",
    "D c #777777",
    "                ",
    "  DDD           ",
    "  D D           ",
    "  DDD           ",
    "     D          ",
    "      DDD       ",
    "      D D       ",
    "      DDD       ",
    "         D      ",
    "          DDD   ",
    "          D D   ",
    "          DDD   ",
    "                ",
    "                ",
    "                ",
    "                "};

wxString EnsureToolbarSvg() {
  const wxString base = wxStandardPaths::Get().GetUserDataDir();
  const wxString dir = base + wxFILE_SEP_PATH + "route_toolbar_pi";
  if (!wxFileName::DirExists(dir) &&
      !wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL))
    return wxEmptyString;

  const wxString path = dir + wxFILE_SEP_PATH + "route-toolbar.svg";
  static const char* svg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32">
  <rect x="1.6" y="1.6" width="28.8" height="28.8" rx="4.8" fill="#4f5255" stroke="#c9cccf" stroke-width="0.45"/>
  <path d="M7.2 8.5 L15.5 19 L24.8 10.5" fill="none" stroke="#d6d9dc" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/>
  <circle cx="7.2" cy="8.5" r="2.8" fill="#4f5255" stroke="#d6d9dc" stroke-width="1.55"/>
  <circle cx="15.5" cy="19" r="2.8" fill="#4f5255" stroke="#d6d9dc" stroke-width="1.55"/>
  <circle cx="24.8" cy="10.5" r="2.8" fill="#4f5255" stroke="#d6d9dc" stroke-width="1.55"/>
</svg>)SVG";

  wxFFile file(path, "wb");
  if (!file.IsOpened()) return wxEmptyString;
  file.Write(svg, wxConvUTF8);
  file.Close();
  return path;
}

wxColour PreviewColour(const wxColour& c) {
  return wxColour((static_cast<int>(c.Red()) + 255) / 2,
                  (static_cast<int>(c.Green()) + 255) / 2,
                  (static_cast<int>(c.Blue()) + 255) / 2);
}

wxPoint PlaceLabelBoxAwayFromStart(const wxPoint& start, const wxPoint& target,
                                   int box_width, int box_height,
                                   int canvas_width, int canvas_height) {
  int x = target.x >= start.x
              ? target.x + kLabelOffsetXPx
              : target.x - kLabelOffsetXPx - box_width;
  int y = target.y >= start.y
              ? target.y + kLabelOffsetYPx
              : target.y - kLabelOffsetYPx - box_height;

  const int max_x = std::max(4, canvas_width - box_width - 4);
  const int max_y = std::max(4, canvas_height - box_height - 4);
  x = std::clamp(x, 4, max_x);
  y = std::clamp(y, 4, max_y);
  return wxPoint(x, y);
}

wxPoint ClosestPointOnLabelBox(const wxPoint& target, const wxPoint& box,
                               int box_width, int box_height) {
  return wxPoint(std::clamp(target.x, box.x, box.x + box_width),
                 std::clamp(target.y, box.y, box.y + box_height));
}

wxPoint PointOnTargetRingTowards(const wxPoint& target, const wxPoint& towards,
                                 int radius) {
  const double dx = static_cast<double>(towards.x - target.x);
  const double dy = static_cast<double>(towards.y - target.y);
  const double length = std::hypot(dx, dy);
  if (length < 0.5) return target;
  return wxPoint(target.x + static_cast<int>(std::lround(radius * dx / length)),
                 target.y + static_cast<int>(std::lround(radius * dy / length)));
}
}  // namespace

extern "C" DECL_EXP opencpn_plugin* create_pi(void* ppimgr) {
  return new route_toolbar_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* plugin) { delete plugin; }

route_toolbar_pi::route_toolbar_pi(void* ppimgr)
    : opencpn_plugin_121(ppimgr), m_toolbar_bitmap(route_toolbar_xpm) {}

route_toolbar_pi::~route_toolbar_pi() = default;

int route_toolbar_pi::Init() {
  wxLogMessage("Route Toolbar: Init begin");
  m_parent_window = GetOCPNCanvasWindow();
  LoadPersistentState();
  wxLogMessage("Route Toolbar: state loaded: %zu EBL/VRM, %zu PIL",
               m_ebl_vrms.size(), m_pils.size());

  const wxString svg_path = EnsureToolbarSvg();
  if (!svg_path.IsEmpty()) {
    m_toolbar_id = InsertPlugInToolSVG(
        "", svg_path, svg_path, svg_path, wxITEM_NORMAL,
        _("Route Toolbar"),
        _("Route planning tools: EBL/VRM, PIL and report"),
        nullptr, -1, 0, this);
  } else {
    m_toolbar_id = InsertPlugInTool(
        _("Route Toolbar"), &m_toolbar_bitmap, &m_toolbar_bitmap, wxITEM_NORMAL,
        _("Route Toolbar"), _("Route planning tools: EBL/VRM, PIL and report"),
        nullptr, -1, 0, this);
  }
  wxLogMessage("Route Toolbar: toolbar inserted, id=%d", m_toolbar_id);

  // Do not register canvas context-menu items synchronously from Init().
  // OpenCPN is still constructing the frame/menu infrastructure at this point
  // on some macOS builds. Queue registration for the GUI event loop instead.
  if (wxTheApp) {
    wxTheApp->CallAfter([this]() {
      wxLogMessage("Route Toolbar: deferred context-menu registration");
      EnsureContextMenu();
    });
  }

  wxLogMessage("Route Toolbar: Init complete");
  return WANTS_TOOLBAR_CALLBACK | INSTALLS_TOOLBAR_TOOL |
         WANTS_CURSOR_LATLON | WANTS_MOUSE_EVENTS | WANTS_OVERLAY_CALLBACK |
         WANTS_OPENGL_OVERLAY_CALLBACK;
}

bool route_toolbar_pi::DeInit() {
  SavePersistentState();
  CancelInteractiveTool();
  if (m_dialog) {
    m_dialog->Destroy();
    m_dialog = nullptr;
  }
  m_dialog_visible = false;
  return true;
}

int route_toolbar_pi::GetAPIVersionMajor() { return 1; }
int route_toolbar_pi::GetAPIVersionMinor() { return 21; }
int route_toolbar_pi::GetPlugInVersionMajor() { return kPluginVersionMajor; }
int route_toolbar_pi::GetPlugInVersionMinor() { return kPluginVersionMinor; }
int route_toolbar_pi::GetPlugInVersionPatch() { return kPluginVersionPatch; }

wxString route_toolbar_pi::GetCommonName() { return _("Route Toolbar"); }
wxString route_toolbar_pi::GetShortDescription() {
  return _("Route-aware passage planning tools for OpenCPN.");
}
wxString route_toolbar_pi::GetLongDescription() {
  return _("Route Toolbar provides route-aware EBL/VRM, parallel index lines "
           "and passage-plan reporting without the manual centre-line workflow "
           "required by generic drawing tools.");
}
wxBitmap* route_toolbar_pi::GetPlugInBitmap() { return &m_toolbar_bitmap; }
int route_toolbar_pi::GetToolbarToolCount() { return 1; }

void route_toolbar_pi::OnToolbarToolCallback(int id) {
  if (id != m_toolbar_id) return;
  if (m_dialog_visible) HideToolWindow(); else ShowToolWindow();
}

void route_toolbar_pi::ShowToolWindow() {
  if (!m_dialog) m_dialog = new RouteToolbarDialog(m_parent_window, this);
  // Safe fallback in case deferred registration has not run yet.
  EnsureContextMenu();
  m_dialog->Show();
  m_dialog->Raise();
  m_dialog_visible = true;
}

void route_toolbar_pi::HideToolWindow() {
  CancelInteractiveTool();
  if (m_dialog) m_dialog->Hide();
  m_dialog_visible = false;
}

void route_toolbar_pi::SetLabelFontSize(int point_size) {
  const int clamped = std::clamp(point_size, kMinLabelFontSize, kMaxLabelFontSize);
  if (clamped == m_label_font_size) return;
  m_label_font_size = clamped;
  SavePersistentState();
  SetStatus(wxString::Format(_("EBL label size set to %d pt."), m_label_font_size));
  if (m_parent_window) RequestRefresh(m_parent_window);
}

wxFont route_toolbar_pi::GetEblLabelFont() const {
  wxFont font(m_label_font_size, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL,
              wxFONTWEIGHT_NORMAL, false);
  if (!font.IsOk())
    font = wxFont(m_label_font_size, wxFONTFAMILY_DEFAULT,
                  wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false);
  return font;
}

void route_toolbar_pi::BeginEblVrm() {
  if (!m_dialog_visible) ShowToolWindow();
  m_pil_mode = PilMode::Idle;
  m_pending_leg = RouteLegHit{};
  m_pending_start = RouteWaypointHit{};
  m_edit_item_index = -1;
  m_edit_pil_index = -1;
  m_ebl_mode = EblMode::AwaitRoutePoint;
  SetStatus(_("Route EBL/VRM: click a route waypoint or an OpenCPN mark. The nearest start object within 0.10 NM is selected. Then click the lighthouse, buoy, beacon or other sight mark. Right-click to cancel."));
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::CancelInteractiveTool() {
  m_ebl_mode = EblMode::Idle;
  m_pending_start = RouteWaypointHit{};
  m_edit_item_index = -1;
  m_pil_mode = PilMode::Idle;
  m_pending_leg = RouteLegHit{};
  m_edit_pil_index = -1;
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::SetCursorLatLon(double lat, double lon) {
  m_cursor_lat = lat;
  m_cursor_lon = lon;
  m_has_cursor = true;
  if ((m_ebl_mode == EblMode::AwaitTarget ||
       m_ebl_mode == EblMode::MoveExistingTarget ||
       m_pil_mode == PilMode::AwaitOffsetPoint) && m_parent_window)
    RequestRefresh(m_parent_window);
}

bool route_toolbar_pi::MouseEventHook(wxMouseEvent& event) {
  if (m_pil_mode != PilMode::Idle) {
    if (event.RightDown()) {
      m_pil_mode = PilMode::Idle;
      m_pending_leg = RouteLegHit{};
      m_edit_pil_index = -1;
      SetStatus(_("Route PIL stopped. Existing PIL objects remain visible."));
      if (m_parent_window) RequestRefresh(m_parent_window);
      return true;
    }
    if (!event.LeftDown() || !m_has_cursor) return false;
    if (m_pil_mode == PilMode::AwaitLeg) {
      const RouteLegHit leg = FindNearestRouteLeg(m_cursor_lat, m_cursor_lon, 0.08);
      if (!leg.found) {
        SetStatus(_("No route leg found nearby. Click closer to the blue route line."));
        return true;
      }
      m_pending_leg = leg;
      m_pil_mode = PilMode::AwaitOffsetPoint;
      wxString from = leg.from_name;
      wxString to = leg.to_name;
      if (from.IsEmpty()) from = wxString::Format("WP%d", leg.leg_index);
      if (to.IsEmpty()) to = wxString::Format("WP%d", leg.leg_index + 1);
      SetStatus(wxString::Format(
          _("PIL leg selected: %s → %s, %03.0f°T. Now click the chart point / object where the parallel line should pass."),
          from, to, leg.course_true));
      if (m_parent_window) RequestRefresh(m_parent_window);
      return true;
    }
    if (m_pil_mode == PilMode::AwaitOffsetPoint) {
      CompletePil(m_cursor_lat, m_cursor_lon);
      return true;
    }
  }

  if (m_ebl_mode == EblMode::MoveExistingTarget) {
    if (event.RightDown()) {
      m_ebl_mode = EblMode::Idle;
      m_edit_item_index = -1;
      SetStatus(_("Move cancelled. The original EBL/VRM target was kept."));
      if (m_parent_window) RequestRefresh(m_parent_window);
      return true;
    }
    if (event.LeftDown() && m_has_cursor)
      return CommitMoveEblTarget(m_cursor_lat, m_cursor_lon);
    return false;
  }

  if (event.RightDown()) {
    if (m_ebl_mode == EblMode::Idle) return false;
    CancelInteractiveTool();
    SetStatus(_("Route EBL/VRM stopped. Existing EBL/VRM objects remain visible."));
    return true;
  }
  if (!event.LeftDown() || !m_has_cursor) return false;

  if (m_dialog_visible && m_pil_mode == PilMode::Idle &&
      (m_ebl_mode == EblMode::Idle || m_ebl_mode == EblMode::AwaitRoutePoint)) {
    const int existing = FindNearestEblTarget(m_cursor_lat, m_cursor_lon,
                                              kTargetPickRadiusNm);
    if (existing >= 0) {
      m_ebl_mode = EblMode::Idle;
      m_pending_start = RouteWaypointHit{};
      ShowEblTargetActions(existing);
      return true;
    }
  }

  if (m_ebl_mode == EblMode::Idle) return false;
  if (m_ebl_mode == EblMode::AwaitRoutePoint) {
    const auto hit = FindNearestEblStart(m_cursor_lat, m_cursor_lon,
                                         kWaypointPickRadiusNm);
    if (!hit.found) {
      SetStatus(_("No route waypoint or OpenCPN mark found within 0.10 NM. Click closer to the intended start point."));
      return true;
    }
    m_pending_start = hit;
    m_ebl_mode = EblMode::AwaitTarget;
    wxString start_name = hit.waypoint_name;
    if (start_name.IsEmpty()) start_name = hit.is_mark ? _("Mark") : _("Waypoint");
    if (hit.is_mark) {
      SetStatus(_("Start selected: mark '") + start_name +
                _("'. Now click the lighthouse, buoy, beacon or other sight mark."));
    } else {
      wxString route_name = hit.route_name;
      if (route_name.IsEmpty()) route_name = _("route");
      SetStatus(_("Start selected: ") + start_name + _(" on '") +
                route_name + _("'. Now click the lighthouse, buoy, beacon or other sight mark."));
    }
    if (m_parent_window) RequestRefresh(m_parent_window);
    return true;
  }

  if (m_ebl_mode == EblMode::AwaitTarget) {
    CompleteEblVrm(m_cursor_lat, m_cursor_lon);
    return true;
  }

  return false;
}

void route_toolbar_pi::OnDialogHidden() {
  m_dialog_visible = false;
  CancelInteractiveTool();
}

void route_toolbar_pi::SetStatus(const wxString& text) {
  if (m_dialog) m_dialog->SetStatus(text);
}

void route_toolbar_pi::UpdateToolButtons() {
  if (!m_dialog) return;
  m_dialog->SetEblActive(m_ebl_mode != EblMode::Idle);
  m_dialog->SetPilActive(m_pil_mode != PilMode::Idle);
}

route_toolbar_pi::RouteWaypointHit route_toolbar_pi::FindNearestRouteWaypoint(
    double lat, double lon, double max_distance_nm) const {
  RouteWaypointHit best;
  best.distance_nm = std::numeric_limits<double>::max();

  const wxArrayString route_guids = GetRouteGUIDArray();
  for (size_t r = 0; r < route_guids.GetCount(); ++r) {
    auto route = GetRoute_Plugin(route_guids[r]);
    if (!route || !route->pWaypointList) continue;

    for (auto node = route->pWaypointList->GetFirst(); node;
         node = node->GetNext()) {
      PlugIn_Waypoint* waypoint = node->GetData();
      if (!waypoint) continue;

      double bearing = 0.0;
      double distance = 0.0;
      DistanceBearingMercator_Plugin(waypoint->m_lat, waypoint->m_lon,
                                     lat, lon, &bearing, &distance);
      if (distance > max_distance_nm || distance >= best.distance_nm) continue;

      best.found = true;
      best.is_mark = false;
      best.lat = waypoint->m_lat;
      best.lon = waypoint->m_lon;
      best.distance_nm = distance;
      best.route_guid = route_guids[r];
      best.route_name = route->m_NameString;
      best.waypoint_guid = waypoint->m_GUID;
      best.waypoint_name = waypoint->m_MarkName;
    }
  }
  return best;
}

route_toolbar_pi::RouteWaypointHit route_toolbar_pi::FindNearestStandaloneMark(
    double lat, double lon, double max_distance_nm) const {
  RouteWaypointHit best;
  best.distance_nm = std::numeric_limits<double>::max();

  const wxArrayString waypoint_guids = GetWaypointGUIDArray();
  std::set<wxString> route_waypoint_guids;
  const wxArrayString route_guids = GetRouteGUIDArray();
  for (size_t r = 0; r < route_guids.GetCount(); ++r) {
    auto route = GetRoute_Plugin(route_guids[r]);
    if (!route || !route->pWaypointList) continue;
    for (auto node = route->pWaypointList->GetFirst(); node;
         node = node->GetNext()) {
      if (node->GetData()) route_waypoint_guids.insert(node->GetData()->m_GUID);
    }
  }

  for (size_t i = 0; i < waypoint_guids.GetCount(); ++i) {
    const wxString guid = waypoint_guids[i];
    if (route_waypoint_guids.count(guid)) continue;
    auto waypoint = GetSingleWaypoint(guid);
    if (!waypoint) continue;

    double bearing = 0.0;
    double distance = 0.0;
    DistanceBearingMercator_Plugin(waypoint->m_lat, waypoint->m_lon,
                                   lat, lon, &bearing, &distance);
    if (distance > max_distance_nm || distance >= best.distance_nm) continue;

    best.found = true;
    best.is_mark = true;
    best.lat = waypoint->m_lat;
    best.lon = waypoint->m_lon;
    best.distance_nm = distance;
    best.waypoint_guid = guid;
    best.waypoint_name = waypoint->m_MarkName;
  }
  return best;
}

route_toolbar_pi::RouteWaypointHit route_toolbar_pi::FindNearestEblStart(
    double lat, double lon, double max_distance_nm) const {
  const RouteWaypointHit route_hit =
      FindNearestRouteWaypoint(lat, lon, max_distance_nm);
  const RouteWaypointHit mark_hit =
      FindNearestStandaloneMark(lat, lon, max_distance_nm);

  if (!route_hit.found) return mark_hit;
  if (!mark_hit.found) return route_hit;
  return mark_hit.distance_nm < route_hit.distance_nm ? mark_hit : route_hit;
}

bool route_toolbar_pi::RefreshEblStartFromMark(RouteEblVrm& item) const {
  if (!item.start_is_mark || item.start_mark_guid.IsEmpty()) return false;
  auto waypoint = GetSingleWaypoint(item.start_mark_guid);
  if (!waypoint) return false;
  const bool changed = std::fabs(item.start_lat - waypoint->m_lat) > 1e-10 ||
                       std::fabs(item.start_lon - waypoint->m_lon) > 1e-10;
  item.start_lat = waypoint->m_lat;
  item.start_lon = waypoint->m_lon;
  if (item.start_mark_name.IsEmpty()) item.start_mark_name = waypoint->m_MarkName;
  return changed;
}

void route_toolbar_pi::RecalculateEbl(RouteEblVrm& item) const {
  double bearing = 0.0;
  double distance = 0.0;
  DistanceBearingMercator_Plugin(item.target_lat, item.target_lon,
                                 item.start_lat, item.start_lon,
                                 &bearing, &distance);
  item.bearing_true = NormalizeBearing(bearing);
  item.range_nm = distance;
}

route_toolbar_pi::RouteWaypointHit route_toolbar_pi::FindNearestEblStart(
    double lat, double lon, double max_distance_nm) const;

void route_toolbar_pi::CompleteEblVrm(double target_lat, double target_lon) {
  if (!m_pending_start.found) {
    m_ebl_mode = EblMode::AwaitRoutePoint;
    SetStatus(_("No start waypoint/mark selected. Click a route waypoint or OpenCPN mark first."));
    return;
  }

  RouteEblVrm item;
  item.start_lat = m_pending_start.lat;
  item.start_lon = m_pending_start.lon;
  item.target_lat = target_lat;
  item.target_lon = target_lon;
  item.arc_width_deg = kDefaultArcWidthDeg;
  item.route_guid = m_pending_start.route_guid;
  item.route_name = m_pending_start.route_name;
  item.waypoint_guid = m_pending_start.is_mark ? wxString() : m_pending_start.waypoint_guid;
  item.waypoint_name = m_pending_start.is_mark ? wxString() : m_pending_start.waypoint_name;
  item.start_is_mark = m_pending_start.is_mark;
  item.start_mark_guid = m_pending_start.is_mark ? m_pending_start.waypoint_guid : wxString();
  item.start_mark_name = m_pending_start.is_mark ? m_pending_start.waypoint_name : wxString();
  RecalculateEbl(item);

  if (!std::isfinite(item.range_nm) || item.range_nm < 0.001) {
    SetStatus(_("Target is too close to the start point. Click another sight mark."));
    return;
  }

  if (m_parent_window) RequestRefresh(m_parent_window);

  wxTextEntryDialog name_dialog(
      m_parent_window,
      _("Name this sight mark / target. Examples: Rivö fyr, No 28, Gullisken."),
      _("Route EBL / VRM"));
  const int rc = name_dialog.ShowModal();
  if (rc != wxID_OK) {
    m_pending_start = RouteWaypointHit{};
    m_ebl_mode = EblMode::AwaitRoutePoint;
    SetStatus(_("Target cancelled. Click another route waypoint or OpenCPN mark, or right-click to finish."));
    if (m_parent_window) RequestRefresh(m_parent_window);
    return;
  }

  item.target_name = name_dialog.GetValue();
  item.target_name.Trim(true).Trim(false);
  if (item.target_name.IsEmpty()) {
    item.target_name = wxString::Format(_("Sight mark %lu"),
                                        static_cast<unsigned long>(m_ebl_vrms.size() + 1));
  }

  m_ebl_vrms.push_back(item);
  SavePersistentState();

  wxString start_name = item.start_is_mark ? item.start_mark_name : item.waypoint_name;
  if (start_name.IsEmpty()) start_name = item.start_is_mark ? _("Mark") : _("WP");
  SetStatus(wxString::Format(
      _("%s → %s  %03.0f°T  %.2f NM. Created with a %.0f° VRM arc. Click another start waypoint/mark, or right-click to finish."),
      start_name, item.target_name, item.bearing_true, item.range_nm,
      item.arc_width_deg));

  m_pending_start = RouteWaypointHit{};
  m_ebl_mode = EblMode::AwaitRoutePoint;
  if (m_parent_window) RequestRefresh(m_parent_window);
}

int route_toolbar_pi::FindNearestEblTarget(double lat, double lon,
                                            double max_distance_nm) const {
  int best_index = -1;
  double best_distance = std::numeric_limits<double>::max();
  for (size_t i = 0; i < m_ebl_vrms.size(); ++i) {
    double bearing = 0.0;
    double distance = 0.0;
    DistanceBearingMercator_Plugin(m_ebl_vrms[i].target_lat,
                                   m_ebl_vrms[i].target_lon,
                                   lat, lon, &bearing, &distance);
    if (distance <= max_distance_nm && distance < best_distance) {
      best_distance = distance;
      best_index = static_cast<int>(i);
    }
  }
  return best_index;
}

void route_toolbar_pi::BeginMoveEblTarget(int index) {
  if (index < 0 || static_cast<size_t>(index) >= m_ebl_vrms.size()) return;
  m_edit_item_index = index;
  m_ebl_mode = EblMode::MoveExistingTarget;
  m_pil_mode = PilMode::Idle;
  m_pending_leg = RouteLegHit{};
  SetStatus(_("Move EBL/VRM target: move the cursor to the new sight mark and click. Right-click to cancel."));
  if (m_parent_window) RequestRefresh(m_parent_window);
}

bool route_toolbar_pi::CommitMoveEblTarget(double target_lat,
                                            double target_lon) {
  if (m_edit_item_index < 0 ||
      static_cast<size_t>(m_edit_item_index) >= m_ebl_vrms.size()) {
    m_ebl_mode = EblMode::Idle;
    m_edit_item_index = -1;
    return true;
  }

  RouteEblVrm& item = m_ebl_vrms[static_cast<size_t>(m_edit_item_index)];
  item.target_lat = target_lat;
  item.target_lon = target_lon;
  RecalculateEbl(item);
  SavePersistentState();
  SetStatus(wxString::Format(_("EBL/VRM target moved: %03.0f°T  %.2f NM."),
                             item.bearing_true, item.range_nm));
  m_ebl_mode = EblMode::Idle;
  m_edit_item_index = -1;
  if (m_parent_window) RequestRefresh(m_parent_window);
  return true;
}

void route_toolbar_pi::RenameEblTarget(int index) {
  if (index < 0 || static_cast<size_t>(index) >= m_ebl_vrms.size()) return;
  RouteEblVrm& item = m_ebl_vrms[static_cast<size_t>(index)];
  wxTextEntryDialog dialog(m_parent_window, _("Sight mark / target name:"),
                           _("Rename Route EBL/VRM"), item.target_name);
  if (dialog.ShowModal() != wxID_OK) return;
  wxString name = dialog.GetValue();
  name.Trim(true).Trim(false);
  if (!name.IsEmpty()) item.target_name = name;
  SavePersistentState();
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::DeleteEblTarget(int index) {
  if (index < 0 || static_cast<size_t>(index) >= m_ebl_vrms.size()) return;
  m_ebl_vrms.erase(m_ebl_vrms.begin() + index);
  SavePersistentState();
  SetStatus(_("Route EBL/VRM deleted."));
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::ShowEblTargetActions(int index) {
  if (index < 0 || static_cast<size_t>(index) >= m_ebl_vrms.size()) return;
  const wxString choices[] = {_("Move target"), _("Rename"), _("Delete")};
  wxSingleChoiceDialog dialog(m_parent_window,
                              _("Choose an action for this Route EBL/VRM target."),
                              _("Route EBL/VRM"), WXSIZEOF(choices), choices);
  if (dialog.ShowModal() != wxID_OK) return;
  switch (dialog.GetSelection()) {
    case 0:
      BeginMoveEblTarget(index);
      break;
    case 1:
      RenameEblTarget(index);
      break;
    case 2:
      DeleteEblTarget(index);
      break;
    default:
      break;
  }
}

void route_toolbar_pi::LoadPersistentState() {
  m_ebl_vrms.clear();
  m_pils.clear();
  wxFileConfig* config = GetOCPNConfigObject();
  if (!config) return;

  const wxString old_path = config->GetPath();
  config->SetPath(wxString::FromUTF8(kConfigPath));
  long font_size = kDefaultLabelFontSize;
  config->Read("LabelFontSize", &font_size, static_cast<long>(kDefaultLabelFontSize));
  m_label_font_size = std::clamp(static_cast<int>(font_size), kMinLabelFontSize,
                                 kMaxLabelFontSize);
  wxString colour_text;
  if (config->Read("EblColour", &colour_text) && !colour_text.IsEmpty()) {
    wxColour parsed;
    if (parsed.Set(colour_text)) m_ebl_colour = parsed;
  }
  colour_text.clear();
  if (config->Read("PilColour", &colour_text) && !colour_text.IsEmpty()) {
    wxColour parsed;
    if (parsed.Set(colour_text)) m_pil_colour = parsed;
  }
  config->SetPath("EBLVRM");
  long count = 0;
  config->Read("Count", &count, 0L);
  count = std::clamp(count, 0L, 1000L);
  for (long i = 0; i < count; ++i) {
    const wxString prefix = wxString::Format("Item%03ld/", i);
    RouteEblVrm item;
    const bool have_start_lat = config->Read(prefix + "StartLat", &item.start_lat);
    const bool have_start_lon = config->Read(prefix + "StartLon", &item.start_lon);
    const bool have_target_lat = config->Read(prefix + "TargetLat", &item.target_lat);
    const bool have_target_lon = config->Read(prefix + "TargetLon", &item.target_lon);
    if (!(have_start_lat && have_start_lon && have_target_lat && have_target_lon)) continue;
    config->Read(prefix + "ArcWidthDeg", &item.arc_width_deg, kDefaultArcWidthDeg);
    config->Read(prefix + "RouteGuid", &item.route_guid, wxEmptyString);
    config->Read(prefix + "RouteName", &item.route_name, wxEmptyString);
    config->Read(prefix + "WaypointGuid", &item.waypoint_guid, wxEmptyString);
    config->Read(prefix + "WaypointName", &item.waypoint_name, wxEmptyString);
    config->Read(prefix + "StartIsMark", &item.start_is_mark, false);
    config->Read(prefix + "StartMarkGuid", &item.start_mark_guid, wxEmptyString);
    config->Read(prefix + "StartMarkName", &item.start_mark_name, wxEmptyString);
    config->Read(prefix + "TargetName", &item.target_name, wxEmptyString);
    const bool valid_coordinates =
        std::isfinite(item.start_lat) && std::isfinite(item.start_lon) &&
        std::isfinite(item.target_lat) && std::isfinite(item.target_lon) &&
        item.start_lat >= -90.0 && item.start_lat <= 90.0 &&
        item.target_lat >= -90.0 && item.target_lat <= 90.0 &&
        item.start_lon >= -180.0 && item.start_lon <= 180.0 &&
        item.target_lon >= -180.0 && item.target_lon <= 180.0;
    if (!valid_coordinates) continue;
    if (!std::isfinite(item.arc_width_deg) || item.arc_width_deg <= 0.0 ||
        item.arc_width_deg > 180.0)
      item.arc_width_deg = kDefaultArcWidthDeg;
    RecalculateEbl(item);
    if (item.range_nm >= 0.001) m_ebl_vrms.push_back(item);
  }
  LoadPilPersistentState(config);
  config->SetPath(old_path);
}

void route_toolbar_pi::SavePersistentState() const {
  wxFileConfig* config = GetOCPNConfigObject();
  if (!config) return;
  const wxString old_path = config->GetPath();
  config->SetPath(wxString::FromUTF8(kConfigPath));
  config->Write("LabelFontSize", static_cast<long>(m_label_font_size));
  config->Write("EblColour", m_ebl_colour.GetAsString(wxC2S_HTML_SYNTAX));
  config->Write("PilColour", m_pil_colour.GetAsString(wxC2S_HTML_SYNTAX));
  config->DeleteGroup("EBLVRM");
  config->SetPath("EBLVRM");
  config->Write("Count", static_cast<long>(m_ebl_vrms.size()));
  for (size_t i = 0; i < m_ebl_vrms.size(); ++i) {
    const wxString prefix = wxString::Format("Item%03lu/", static_cast<unsigned long>(i));
    const RouteEblVrm& item = m_ebl_vrms[i];
    config->Write(prefix + "StartLat", item.start_lat);
    config->Write(prefix + "StartLon", item.start_lon);
    config->Write(prefix + "TargetLat", item.target_lat);
    config->Write(prefix + "TargetLon", item.target_lon);
    config->Write(prefix + "ArcWidthDeg", item.arc_width_deg);
    config->Write(prefix + "RouteGuid", item.route_guid);
    config->Write(prefix + "RouteName", item.route_name);
    config->Write(prefix + "WaypointGuid", item.waypoint_guid);
    config->Write(prefix + "WaypointName", item.waypoint_name);
    config->Write(prefix + "StartIsMark", item.start_is_mark);
    config->Write(prefix + "StartMarkGuid", item.start_mark_guid);
    config->Write(prefix + "StartMarkName", item.start_mark_name);
    config->Write(prefix + "TargetName", item.target_name);
  }
  SavePilPersistentState(config);
  config->SetPath(old_path);
  config->Flush();
}

bool route_toolbar_pi::RenderOverlay(wxDC& dc, PlugIn_ViewPort* vp) {
  if (!vp || !vp->bValid) return false;
  bool rendered = false;
  for (size_t i = 0; i < m_pils.size(); ++i) {
    if (m_pil_mode == PilMode::AwaitOffsetPoint && m_edit_pil_index >= 0 &&
        static_cast<int>(i) == m_edit_pil_index) continue;
    DrawPil(dc, vp, m_pils[i], false);
    rendered = true;
  }
  if (m_pil_mode == PilMode::AwaitOffsetPoint && m_pending_leg.found && m_has_cursor) {
    RoutePil preview;
    if (BuildPilFromAnchor(m_pending_leg, m_cursor_lat, m_cursor_lon, preview)) {
      DrawPil(dc, vp, preview, true);
      rendered = true;
    }
  }
  for (size_t i = 0; i < m_ebl_vrms.size(); ++i) {
    if (m_ebl_mode == EblMode::MoveExistingTarget && static_cast<int>(i) == m_edit_item_index)
      continue;
    if (m_ebl_vrms[i].start_is_mark) RecalculateEbl(m_ebl_vrms[i]);
    DrawEblVrm(dc, vp, m_ebl_vrms[i], false);
    rendered = true;
  }
  if (m_ebl_mode == EblMode::AwaitTarget && m_pending_start.found && m_has_cursor) {
    RouteEblVrm preview;
    preview.start_lat = m_pending_start.lat;
    preview.start_lon = m_pending_start.lon;
    preview.target_lat = m_cursor_lat;
    preview.target_lon = m_cursor_lon;
    preview.arc_width_deg = kDefaultArcWidthDeg;
    preview.waypoint_name = m_pending_start.waypoint_name;
    preview.start_is_mark = m_pending_start.is_mark;
    preview.start_mark_guid = m_pending_start.is_mark ? wxString(m_pending_start.waypoint_guid) : wxString();
    preview.start_mark_name = m_pending_start.is_mark ? wxString(m_pending_start.waypoint_name) : wxString();
    RecalculateEbl(preview);
    if (preview.range_nm >= 0.001) {
      DrawEblVrm(dc, vp, preview, true);
      rendered = true;
    }
  }
  if (m_ebl_mode == EblMode::MoveExistingTarget && m_has_cursor &&
      m_edit_item_index >= 0 && static_cast<size_t>(m_edit_item_index) < m_ebl_vrms.size()) {
    RouteEblVrm preview = m_ebl_vrms[static_cast<size_t>(m_edit_item_index)];
    preview.target_lat = m_cursor_lat;
    preview.target_lon = m_cursor_lon;
    RecalculateEbl(preview);
    DrawEblVrm(dc, vp, preview, true);
    rendered = true;
  }
  return rendered;
}

bool route_toolbar_pi::RenderGLOverlay(wxGLContext*, PlugIn_ViewPort* vp) {
  if (!vp || !vp->bValid) return false;
  bool rendered = false;
  for (size_t i = 0; i < m_pils.size(); ++i) {
    if (m_pil_mode == PilMode::AwaitOffsetPoint && m_edit_pil_index >= 0 &&
        static_cast<int>(i) == m_edit_pil_index) continue;
    DrawPilGL(vp, m_pils[i], false);
    rendered = true;
  }
  if (m_pil_mode == PilMode::AwaitOffsetPoint && m_pending_leg.found && m_has_cursor) {
    RoutePil preview;
    if (BuildPilFromAnchor(m_pending_leg, m_cursor_lat, m_cursor_lon, preview)) {
      DrawPilGL(vp, preview, true);
      rendered = true;
    }
  }
  for (size_t i = 0; i < m_ebl_vrms.size(); ++i) {
    if (m_ebl_mode == EblMode::MoveExistingTarget && static_cast<int>(i) == m_edit_item_index)
      continue;
    if (m_ebl_vrms[i].start_is_mark) {
      if (RefreshEblStartFromMark(m_ebl_vrms[i])) RecalculateEbl(m_ebl_vrms[i]);
    }
    DrawEblVrmGL(vp, m_ebl_vrms[i], false);
    rendered = true;
  }
  if (m_ebl_mode == EblMode::AwaitTarget && m_pending_start.found && m_has_cursor) {
    RouteEblVrm preview;
    preview.start_lat = m_pending_start.lat;
    preview.start_lon = m_pending_start.lon;
    preview.target_lat = m_cursor_lat;
    preview.target_lon = m_cursor_lon;
    preview.arc_width_deg = kDefaultArcWidthDeg;
    preview.waypoint_name = m_pending_start.waypoint_name;
    preview.start_is_mark = m_pending_start.is_mark;
    preview.start_mark_guid = m_pending_start.is_mark ? wxString(m_pending_start.waypoint_guid) : wxString();
    preview.start_mark_name = m_pending_start.is_mark ? wxString(m_pending_start.waypoint_name) : wxString();
    RecalculateEbl(preview);
    if (preview.range_nm >= 0.001) {
      DrawEblVrmGL(vp, preview, true);
      rendered = true;
    }
  }
  if (m_ebl_mode == EblMode::MoveExistingTarget && m_has_cursor &&
      m_edit_item_index >= 0 && static_cast<size_t>(m_edit_item_index) < m_ebl_vrms.size()) {
    RouteEblVrm preview = m_ebl_vrms[static_cast<size_t>(m_edit_item_index)];
    preview.target_lat = m_cursor_lat;
    preview.target_lon = m_cursor_lon;
    RecalculateEbl(preview);
    DrawEblVrmGL(vp, preview, true);
    rendered = true;
  }
  return rendered;
}

void route_toolbar_pi::DrawEblVrm(wxDC& dc, PlugIn_ViewPort* vp,
                                   const RouteEblVrm& item, bool preview) const {
  wxPoint start_px;
  wxPoint target_px;
  GetCanvasPixLL(vp, &start_px, item.start_lat, item.start_lon);
  GetCanvasPixLL(vp, &target_px, item.target_lat, item.target_lon);

  const wxPen old_pen = dc.GetPen();
  const wxBrush old_brush = dc.GetBrush();
  const wxFont old_font = dc.GetFont();
  const wxColour old_text = dc.GetTextForeground();

  const wxColour ebl_colour = preview ? PreviewColour(m_ebl_colour) : m_ebl_colour;
  dc.SetPen(wxPen(ebl_colour, preview ? 1 : 2));
  dc.SetBrush(*wxTRANSPARENT_BRUSH);
  dc.DrawLine(start_px, target_px);

  const int ring_radius = preview ? kTargetRingRadiusPx - 1 : kTargetRingRadiusPx;
  dc.DrawCircle(target_px, ring_radius);
  dc.DrawLine(target_px.x - kTargetCrosshairRadiusPx, target_px.y,
              target_px.x + kTargetCrosshairRadiusPx, target_px.y);
  dc.DrawLine(target_px.x, target_px.y - kTargetCrosshairRadiusPx,
              target_px.x, target_px.y + kTargetCrosshairRadiusPx);

  const double arc_start_bearing = item.bearing_true + 180.0 - item.arc_width_deg / 2.0;
  const double arc_end_bearing = item.bearing_true + 180.0 + item.arc_width_deg / 2.0;
  wxPoint previous;
  bool have_previous = false;
  for (int i = 0; i <= kArcSegments; ++i) {
    const double bearing = arc_start_bearing +
                           (arc_end_bearing - arc_start_bearing) * i / kArcSegments;
    double arc_lat = 0.0;
    double arc_lon = 0.0;
    PositionBearingDistanceMercator_Plugin(item.start_lat, item.start_lon,
                                           NormalizeBearing(bearing), item.range_nm,
                                           &arc_lat, &arc_lon);
    wxPoint point;
    GetCanvasPixLL(vp, &point, arc_lat, arc_lon);
    if (have_previous) dc.DrawLine(previous, point);
    previous = point;
    have_previous = true;
  }

  const wxString line1 = item.target_name.IsEmpty() ? _("Sight mark") : item.target_name;
  const wxString line2 = wxString::Format("%03.0f°T · %.2f NM", item.bearing_true,
                                          item.range_nm);
  dc.SetFont(GetEblLabelFont());
  const wxSize e1 = dc.GetTextExtent(line1);
  const wxSize e2 = dc.GetTextExtent(line2);
  const int box_w = std::max(e1.x, e2.x) + 10;
  const int box_h = e1.y + e2.y + 8;
  const wxPoint box = PlaceLabelBoxAwayFromStart(start_px, target_px, box_w, box_h,
                                                  vp->pix_width, vp->pix_height);
  const wxPoint leader_end = ClosestPointOnLabelBox(target_px, box, box_w, box_h);
  const wxPoint leader_start = PointOnTargetRingTowards(target_px, leader_end, ring_radius);

  dc.DrawLine(leader_start, leader_end);
  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.SetBrush(wxBrush(wxColour(255, 245, 145)));
  dc.DrawRectangle(box.x, box.y, box_w, box_h);
  dc.SetTextForeground(*wxBLACK);
  dc.DrawText(line1, box.x + 5, box.y + 2);
  dc.DrawText(line2, box.x + 5, box.y + 3 + e1.y);

  dc.SetPen(old_pen);
  dc.SetBrush(old_brush);
  dc.SetFont(old_font);
  dc.SetTextForeground(old_text);
}

void route_toolbar_pi::DrawGlLabel(const wxString& label, int x, int y) const {
  const wxFont font = GetEblLabelFont();
  wxBitmap bitmap(512, 128, 32);
  if (!bitmap.IsOk()) return;
  wxMemoryDC memory_dc(bitmap);
  memory_dc.SetBackground(wxBrush(wxColour(255, 245, 145)));
  memory_dc.Clear();
  memory_dc.SetFont(font);
  memory_dc.SetTextForeground(*wxBLACK);
  const wxSize text = memory_dc.GetTextExtent(label);
  const int width = std::min(512, text.x + 10);
  const int height = std::min(128, text.y + 6);
  memory_dc.DrawText(label, 5, 3);
  memory_dc.SelectObject(wxNullBitmap);

  wxImage image = bitmap.ConvertToImage();
  if (!image.IsOk()) return;
  if (image.GetWidth() != width || image.GetHeight() != height)
    image = image.GetSubImage(wxRect(0, 0, width, height));
  if (!image.HasAlpha()) image.InitAlpha();
  unsigned char* alpha = image.GetAlpha();
  const unsigned char* rgb = image.GetData();
  if (!alpha || !rgb) return;
  for (int i = 0; i < width * height; ++i) {
    const int r = rgb[i * 3];
    const int g = rgb[i * 3 + 1];
    const int b = rgb[i * 3 + 2];
    alpha[i] = (r > 248 && g > 238 && b > 130) ? 235 : 255;
  }

  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, image.GetData());
  glDeleteTextures(1, &texture);
}

void route_toolbar_pi::DrawEblVrmGL(PlugIn_ViewPort* vp,
                                     const RouteEblVrm& item,
                                     bool preview) const {
  wxPoint start_px;
  wxPoint target_px;
  GetCanvasPixLL(vp, &start_px, item.start_lat, item.start_lon);
  GetCanvasPixLL(vp, &target_px, item.target_lat, item.target_lon);

  GLint previous_texture = 0;
  GLint previous_unpack = 4;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack);
  const GLboolean texture_was_enabled = glIsEnabled(GL_TEXTURE_2D);

  const wxColour ebl_colour = preview ? PreviewColour(m_ebl_colour) : m_ebl_colour;
  glColor4ub(ebl_colour.Red(), ebl_colour.Green(), ebl_colour.Blue(),
             preview ? 190 : 255);
  glDisable(GL_TEXTURE_2D);
  glLineWidth(preview ? 1.0f : 2.0f);
  glBegin(GL_LINES);
  glVertex2i(start_px.x, start_px.y);
  glVertex2i(target_px.x, target_px.y);
  glVertex2i(target_px.x - kTargetCrosshairRadiusPx, target_px.y);
  glVertex2i(target_px.x + kTargetCrosshairRadiusPx, target_px.y);
  glVertex2i(target_px.x, target_px.y - kTargetCrosshairRadiusPx);
  glVertex2i(target_px.x, target_px.y + kTargetCrosshairRadiusPx);
  glEnd();

  const int ring_radius = preview ? kTargetRingRadiusPx - 1 : kTargetRingRadiusPx;
  glBegin(GL_LINE_LOOP);
  for (int i = 0; i < 32; ++i) {
    const double angle = 2.0 * 3.14159265358979323846 * i / 32.0;
    glVertex2d(target_px.x + std::cos(angle) * ring_radius,
               target_px.y + std::sin(angle) * ring_radius);
  }
  glEnd();

  const double arc_start_bearing = item.bearing_true + 180.0 - item.arc_width_deg / 2.0;
  const double arc_end_bearing = item.bearing_true + 180.0 + item.arc_width_deg / 2.0;
  glBegin(GL_LINE_STRIP);
  for (int i = 0; i <= kArcSegments; ++i) {
    const double bearing = arc_start_bearing +
                           (arc_end_bearing - arc_start_bearing) * i / kArcSegments;
    double arc_lat = 0.0;
    double arc_lon = 0.0;
    PositionBearingDistanceMercator_Plugin(item.start_lat, item.start_lon,
                                           NormalizeBearing(bearing), item.range_nm,
                                           &arc_lat, &arc_lon);
    wxPoint point;
    GetCanvasPixLL(vp, &point, arc_lat, arc_lon);
    glVertex2i(point.x, point.y);
  }
  glEnd();

  // The OpenGL label renderer is intentionally kept compact here. Placement uses
  // the same away-from-start heuristic as wxDC rendering, while preserving the
  // caller's GL texture state.
  const wxString gl_label = (item.target_name.IsEmpty() ? _("Sight mark") : item.target_name) +
                            "  " + wxString::Format("%03.0f°T · %.2f NM", item.bearing_true,
                                                     item.range_nm);
  const int approx_w = std::clamp(static_cast<int>(gl_label.length()) *
                                      std::max(6, m_label_font_size / 2),
                                  60, 500);
  const int approx_h = std::max(20, m_label_font_size + 10);
  const wxPoint box = PlaceLabelBoxAwayFromStart(start_px, target_px, approx_w, approx_h,
                                                  vp->pix_width, vp->pix_height);
  DrawGlLabel(gl_label, box.x, box.y);

  glBindTexture(GL_TEXTURE_2D, previous_texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack);
  if (texture_was_enabled)
    glEnable(GL_TEXTURE_2D);
  else
    glDisable(GL_TEXTURE_2D);
}

double route_toolbar_pi::NormalizeBearing(double bearing) {
  bearing = std::fmod(bearing, 360.0);
  if (bearing < 0.0) bearing += 360.0;
  return bearing;
}
