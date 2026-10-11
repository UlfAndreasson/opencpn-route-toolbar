// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_pi.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <wx/brush.h>
#include <wx/choicdlg.h>
#include <wx/dc.h>
#include <wx/dcmemory.h>
#include <wx/fileconf.h>
#include <wx/image.h>
#include <wx/intl.h>
#include <wx/msgdlg.h>
#include <wx/pen.h>
#include <wx/settings.h>
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
constexpr int kPluginVersionMinor = 2;
constexpr int kPluginVersionPatch = 5;
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

// Deliberately grayscale: three route waypoints joined by a route line.
static const char* route_toolbar_xpm[] = {
    "16 16 3 1",
    "  c None",
    "D c #4A4A4A",
    "L c #A0A0A0",
    "                ",
    "  DDD           ",
    "  DDD           ",
    "    LL          ",
    "     LL         ",
    "      DDD       ",
    "      DDD       ",
    "         LL     ",
    "          LL    ",
    "           DDD  ",
    "           DDD  ",
    "                ",
    "                ",
    "                ",
    "                ",
    "                "};
}  // namespace

extern "C" DECL_EXP opencpn_plugin* create_pi(void* ppimgr) {
  return new route_toolbar_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* plugin) { delete plugin; }

route_toolbar_pi::route_toolbar_pi(void* ppimgr)
    : opencpn_plugin_121(ppimgr), m_toolbar_bitmap(route_toolbar_xpm) {}

route_toolbar_pi::~route_toolbar_pi() = default;

int route_toolbar_pi::Init() {
  m_parent_window = GetOCPNCanvasWindow();
  LoadPersistentState();

  m_toolbar_id = InsertPlugInTool(
      _("Route Toolbar"), &m_toolbar_bitmap, &m_toolbar_bitmap, wxITEM_CHECK,
      _("Route Toolbar"), _("Route planning tools: EBL/VRM, PIL and report"),
      nullptr, -1, 0, this);

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

  if (m_dialog_visible)
    HideToolWindow();
  else
    ShowToolWindow();
}

void route_toolbar_pi::ShowToolWindow() {
  if (!m_dialog) m_dialog = new RouteToolbarDialog(m_parent_window, this);

  m_dialog->Show();
  m_dialog->Raise();
  m_dialog_visible = true;
  SetToolbarItemState(m_toolbar_id, true);
}

void route_toolbar_pi::HideToolWindow() {
  CancelInteractiveTool();
  if (m_dialog) m_dialog->Hide();
  m_dialog_visible = false;
  SetToolbarItemState(m_toolbar_id, false);
}

void route_toolbar_pi::SetLabelFontSize(int point_size) {
  const int clamped = std::clamp(point_size, kMinLabelFontSize,
                                 kMaxLabelFontSize);
  if (clamped == m_label_font_size) return;

  m_label_font_size = clamped;
  SavePersistentState();
  SetStatus(wxString::Format(_("EBL label size set to %d pt."),
                             m_label_font_size));
  if (m_parent_window) RequestRefresh(m_parent_window);
}

wxFont route_toolbar_pi::GetEblLabelFont() const {
  wxFont font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
  if (!font.IsOk()) font = *wxNORMAL_FONT;
  font.SetPointSize(m_label_font_size);
  return font;
}

void route_toolbar_pi::BeginEblVrm() {
  if (!m_dialog_visible) ShowToolWindow();
  m_pending_start = RouteWaypointHit{};
  m_edit_item_index = -1;
  m_ebl_mode = EblMode::AwaitRoutePoint;
  SetStatus(_("Route EBL/VRM: click a route waypoint. The click snaps to the nearest route waypoint within 0.10 NM. Click an existing target anchor to edit it. Right-click on the chart to cancel."));
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::CancelInteractiveTool() {
  m_ebl_mode = EblMode::Idle;
  m_pending_start = RouteWaypointHit{};
  m_edit_item_index = -1;
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::SetCursorLatLon(double lat, double lon) {
  m_cursor_lat = lat;
  m_cursor_lon = lon;
  m_has_cursor = true;

  if ((m_ebl_mode == EblMode::AwaitTarget ||
       m_ebl_mode == EblMode::MoveExistingTarget) &&
      m_parent_window)
    RequestRefresh(m_parent_window);
}

bool route_toolbar_pi::MouseEventHook(wxMouseEvent& event) {
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

  // Existing targets stay editable whenever the Route Toolbar window is open.
  if (m_dialog_visible &&
      (m_ebl_mode == EblMode::Idle ||
       m_ebl_mode == EblMode::AwaitRoutePoint)) {
    const int existing = FindNearestEblTarget(
        m_cursor_lat, m_cursor_lon, kTargetPickRadiusNm);
    if (existing >= 0) {
      m_ebl_mode = EblMode::Idle;
      m_pending_start = RouteWaypointHit{};
      ShowEblTargetActions(existing);
      return true;
    }
  }

  if (m_ebl_mode == EblMode::Idle) return false;

  if (m_ebl_mode == EblMode::AwaitRoutePoint) {
    const auto hit = FindNearestRouteWaypoint(m_cursor_lat, m_cursor_lon,
                                              kWaypointPickRadiusNm);
    if (!hit.found) {
      SetStatus(_("No route waypoint found within 0.10 NM. Click closer to a waypoint on the route."));
      return true;
    }

    m_pending_start = hit;
    m_ebl_mode = EblMode::AwaitTarget;

    wxString route_name = hit.route_name;
    if (route_name.IsEmpty()) route_name = _("Unnamed route");
    wxString waypoint_name = hit.waypoint_name;
    if (waypoint_name.IsEmpty()) waypoint_name = _("Waypoint");

    SetStatus(_("Start selected: ") + route_name + _(" / ") + waypoint_name +
              _(". Now click the lighthouse, buoy, beacon or other sight mark."));
    if (m_parent_window) RequestRefresh(m_parent_window);
    return true;
  }

  if (m_ebl_mode == EblMode::AwaitTarget) {
    CompleteEblVrm(m_cursor_lat, m_cursor_lon);
    return true;
  }

  return false;
}

route_toolbar_pi::RouteWaypointHit route_toolbar_pi::FindNearestRouteWaypoint(
    double lat, double lon, double max_distance_nm) const {
  RouteWaypointHit best;
  best.distance_nm = std::numeric_limits<double>::max();

  const wxArrayString route_guids = GetRouteGUIDArray();
  for (size_t i = 0; i < route_guids.GetCount(); ++i) {
    auto route = GetRoute_Plugin(route_guids[i]);
    if (!route || !route->pWaypointList) continue;

    for (auto node = route->pWaypointList->GetFirst(); node;
         node = node->GetNext()) {
      PlugIn_Waypoint* waypoint = node->GetData();
      if (!waypoint) continue;

      double distance_nm = 0.0;
      DistanceBearingMercator_Plugin(lat, lon, waypoint->m_lat, waypoint->m_lon,
                                     nullptr, &distance_nm);
      if (distance_nm > max_distance_nm || distance_nm >= best.distance_nm)
        continue;

      best.found = true;
      best.lat = waypoint->m_lat;
      best.lon = waypoint->m_lon;
      best.distance_nm = distance_nm;
      best.route_guid = route_guids[i];
      best.route_name = route->m_NameString;
      best.waypoint_guid = waypoint->m_GUID;
      best.waypoint_name = waypoint->m_MarkName;
    }
  }

  return best;
}

int route_toolbar_pi::FindNearestEblTarget(double lat, double lon,
                                           double max_distance_nm) const {
  int best_index = -1;
  double best_distance = std::numeric_limits<double>::max();

  for (size_t i = 0; i < m_ebl_vrms.size(); ++i) {
    double distance_nm = 0.0;
    DistanceBearingMercator_Plugin(
        lat, lon, m_ebl_vrms[i].target_lat, m_ebl_vrms[i].target_lon,
        nullptr, &distance_nm);
    if (distance_nm <= max_distance_nm && distance_nm < best_distance) {
      best_index = static_cast<int>(i);
      best_distance = distance_nm;
    }
  }

  return best_index;
}

void route_toolbar_pi::ShowEblTargetActions(int index) {
  if (index < 0 || static_cast<size_t>(index) >= m_ebl_vrms.size()) return;

  const RouteEblVrm& item = m_ebl_vrms[static_cast<size_t>(index)];
  wxArrayString choices;
  choices.Add(_("Move target"));
  choices.Add(_("Rename"));
  choices.Add(_("Delete"));

  wxString target_name = item.target_name;
  if (target_name.IsEmpty()) target_name = _("Unnamed sight mark");

  wxSingleChoiceDialog dialog(
      m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
      _("What do you want to do with ") + target_name + _("?"),
      _("Route EBL / VRM"), choices);
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

void route_toolbar_pi::BeginMoveEblTarget(int index) {
  if (index < 0 || static_cast<size_t>(index) >= m_ebl_vrms.size()) return;
  m_edit_item_index = index;
  m_ebl_mode = EblMode::MoveExistingTarget;

  wxString name = m_ebl_vrms[static_cast<size_t>(index)].target_name;
  if (name.IsEmpty()) name = _("sight mark");
  SetStatus(_("Move ") + name +
            _(" — click the new target position. Right-click to cancel."));
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

  RouteEblVrm moved = m_ebl_vrms[static_cast<size_t>(m_edit_item_index)];
  moved.target_lat = target_lat;
  moved.target_lon = target_lon;
  RecalculateEbl(moved);

  if (moved.range_nm < 0.001) {
    SetStatus(_("Target is too close to the route point. Click another position or right-click to cancel."));
    return true;
  }

  m_ebl_vrms[static_cast<size_t>(m_edit_item_index)] = moved;
  SavePersistentState();

  wxString name = moved.target_name;
  if (name.IsEmpty()) name = _("Sight mark");
  SetStatus(name + _(" moved — ") +
            wxString::Format(_("%03.0f°T · %.2f NM"), moved.bearing_true,
                             moved.range_nm));

  m_ebl_mode = EblMode::Idle;
  m_edit_item_index = -1;
  if (m_parent_window) RequestRefresh(m_parent_window);
  return true;
}

void route_toolbar_pi::RenameEblTarget(int index) {
  if (index < 0 || static_cast<size_t>(index) >= m_ebl_vrms.size()) return;
  RouteEblVrm& item = m_ebl_vrms[static_cast<size_t>(index)];

  wxTextEntryDialog dialog(
      m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
      _("Enter a new sight-mark / target name."), _("Route EBL / VRM"),
      item.target_name);
  if (dialog.ShowModal() != wxID_OK) return;

  wxString entered = dialog.GetValue();
  entered.Trim(true).Trim(false);
  if (entered.IsEmpty()) return;

  item.target_name = entered;
  SavePersistentState();
  SetStatus(_("Renamed Route EBL/VRM target to ") + entered + _("."));
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::DeleteEblTarget(int index) {
  if (index < 0 || static_cast<size_t>(index) >= m_ebl_vrms.size()) return;
  const wxString name = m_ebl_vrms[static_cast<size_t>(index)].target_name;

  wxMessageDialog confirm(
      m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
      _("Delete Route EBL/VRM target '") + name + _("'?"),
      _("Route EBL / VRM"), wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
  if (confirm.ShowModal() != wxID_YES) return;

  m_ebl_vrms.erase(m_ebl_vrms.begin() + index);
  m_edit_item_index = -1;
  m_ebl_mode = EblMode::Idle;
  SavePersistentState();
  SetStatus(_("Route EBL/VRM target deleted."));
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::RecalculateEbl(RouteEblVrm& item) const {
  DistanceBearingMercator_Plugin(
      item.target_lat, item.target_lon, item.start_lat, item.start_lon,
      &item.bearing_true, &item.range_nm);
  item.bearing_true = NormalizeBearing(item.bearing_true);
}

void route_toolbar_pi::CompleteEblVrm(double target_lat, double target_lon) {
  if (!m_pending_start.found) {
    m_ebl_mode = EblMode::AwaitRoutePoint;
    SetStatus(_("Route start point was lost. Click a route waypoint again."));
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
  item.waypoint_guid = m_pending_start.waypoint_guid;
  item.waypoint_name = m_pending_start.waypoint_name;

  RecalculateEbl(item);

  if (item.range_nm < 0.001) {
    SetStatus(_("Target is too close to the route point. Click the sight mark again."));
    return;
  }

  const wxString default_name = wxString::Format(
      _("Sight mark %lu"), static_cast<unsigned long>(m_ebl_vrms.size() + 1));
  item.target_name = default_name;

  // Freeze the selected geometry before opening the modal naming dialog.
  m_ebl_mode = EblMode::Idle;
  m_ebl_vrms.push_back(item);
  if (m_parent_window) RequestRefresh(m_parent_window);

  wxTextEntryDialog name_dialog(
      m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
      _("Name this sight mark / target. Examples: Rivö fyr, No 28, Gulldisken."),
      _("Route EBL / VRM"), default_name);
  if (name_dialog.ShowModal() == wxID_OK) {
    wxString entered = name_dialog.GetValue();
    entered.Trim(true).Trim(false);
    if (!entered.IsEmpty()) {
      item.target_name = entered;
      m_ebl_vrms.back().target_name = entered;
    }
  }

  SavePersistentState();

  wxString waypoint_name = item.waypoint_name;
  if (waypoint_name.IsEmpty()) waypoint_name = _("Waypoint");
  SetStatus(waypoint_name + _(" → ") + item.target_name + _("  ") +
            wxString::Format(_("%03.0f°T  %.2f NM. Saved. Click another route waypoint, click an existing target to edit, or right-click to finish."),
                             item.bearing_true, item.range_nm));

  m_pending_start = RouteWaypointHit{};
  m_ebl_mode = EblMode::AwaitRoutePoint;
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::LoadPersistentState() {
  m_ebl_vrms.clear();

  wxFileConfig* config = GetOCPNConfigObject();
  if (!config) return;

  const wxString old_path = config->GetPath();
  config->SetPath(wxString::FromUTF8(kConfigPath));

  long font_size = kDefaultLabelFontSize;
  config->Read("LabelFontSize", &font_size,
               static_cast<long>(kDefaultLabelFontSize));
  m_label_font_size = std::clamp(static_cast<int>(font_size),
                                 kMinLabelFontSize, kMaxLabelFontSize);

  config->SetPath("EBLVRM");
  long count = 0;
  config->Read("Count", &count, 0L);
  count = std::clamp(count, 0L, 1000L);

  for (long i = 0; i < count; ++i) {
    const wxString prefix = wxString::Format("Item%03ld/", i);
    RouteEblVrm item;

    const bool have_start_lat =
        config->Read(prefix + "StartLat", &item.start_lat);
    const bool have_start_lon =
        config->Read(prefix + "StartLon", &item.start_lon);
    const bool have_target_lat =
        config->Read(prefix + "TargetLat", &item.target_lat);
    const bool have_target_lon =
        config->Read(prefix + "TargetLon", &item.target_lon);

    if (!(have_start_lat && have_start_lon && have_target_lat &&
          have_target_lon))
      continue;

    config->Read(prefix + "ArcWidthDeg", &item.arc_width_deg,
                 kDefaultArcWidthDeg);
    config->Read(prefix + "RouteGuid", &item.route_guid, wxEmptyString);
    config->Read(prefix + "RouteName", &item.route_name, wxEmptyString);
    config->Read(prefix + "WaypointGuid", &item.waypoint_guid, wxEmptyString);
    config->Read(prefix + "WaypointName", &item.waypoint_name, wxEmptyString);
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

  config->SetPath(old_path);
}

void route_toolbar_pi::SavePersistentState() const {
  wxFileConfig* config = GetOCPNConfigObject();
  if (!config) return;

  const wxString old_path = config->GetPath();
  config->SetPath(wxString::FromUTF8(kConfigPath));
  config->Write("LabelFontSize", static_cast<long>(m_label_font_size));

  config->DeleteGroup("EBLVRM");
  config->SetPath("EBLVRM");
  config->Write("Count", static_cast<long>(m_ebl_vrms.size()));

  for (size_t i = 0; i < m_ebl_vrms.size(); ++i) {
    const wxString prefix = wxString::Format(
        "Item%03lu/", static_cast<unsigned long>(i));
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
    config->Write(prefix + "TargetName", item.target_name);
  }

  config->SetPath(old_path);
  config->Flush();
}

bool route_toolbar_pi::RenderOverlay(wxDC& dc, PlugIn_ViewPort* vp) {
  if (!vp || !vp->bValid) return false;

  bool rendered = false;
  for (size_t i = 0; i < m_ebl_vrms.size(); ++i) {
    if (m_ebl_mode == EblMode::MoveExistingTarget &&
        static_cast<int>(i) == m_edit_item_index)
      continue;
    DrawEblVrm(dc, vp, m_ebl_vrms[i], false);
    rendered = true;
  }

  if (m_ebl_mode == EblMode::AwaitTarget && m_pending_start.found &&
      m_has_cursor) {
    RouteEblVrm preview;
    preview.start_lat = m_pending_start.lat;
    preview.start_lon = m_pending_start.lon;
    preview.target_lat = m_cursor_lat;
    preview.target_lon = m_cursor_lon;
    preview.arc_width_deg = kDefaultArcWidthDeg;
    preview.waypoint_name = m_pending_start.waypoint_name;
    RecalculateEbl(preview);

    if (preview.range_nm >= 0.001) {
      DrawEblVrm(dc, vp, preview, true);
      rendered = true;
    }
  }

  if (m_ebl_mode == EblMode::MoveExistingTarget && m_has_cursor &&
      m_edit_item_index >= 0 &&
      static_cast<size_t>(m_edit_item_index) < m_ebl_vrms.size()) {
    RouteEblVrm preview = m_ebl_vrms[static_cast<size_t>(m_edit_item_index)];
    preview.target_lat = m_cursor_lat;
    preview.target_lon = m_cursor_lon;
    RecalculateEbl(preview);
    if (preview.range_nm >= 0.001) {
      DrawEblVrm(dc, vp, preview, true);
      rendered = true;
    }
  }

  return rendered;
}

bool route_toolbar_pi::RenderGLOverlay(wxGLContext*, PlugIn_ViewPort* vp) {
  if (!vp || !vp->bValid || vp->pix_width <= 0 || vp->pix_height <= 0)
    return false;

  bool rendered = false;

  GLint previous_matrix_mode = GL_MODELVIEW;
  glGetIntegerv(GL_MATRIX_MODE, &previous_matrix_mode);
  glPushAttrib(GL_ALL_ATTRIB_BITS);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0, static_cast<double>(vp->pix_width),
          static_cast<double>(vp->pix_height), 0.0, -1.0, 1.0);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  for (size_t i = 0; i < m_ebl_vrms.size(); ++i) {
    if (m_ebl_mode == EblMode::MoveExistingTarget &&
        static_cast<int>(i) == m_edit_item_index)
      continue;
    DrawEblVrmGL(vp, m_ebl_vrms[i], false);
    rendered = true;
  }

  if (m_ebl_mode == EblMode::AwaitTarget && m_pending_start.found &&
      m_has_cursor) {
    RouteEblVrm preview;
    preview.start_lat = m_pending_start.lat;
    preview.start_lon = m_pending_start.lon;
    preview.target_lat = m_cursor_lat;
    preview.target_lon = m_cursor_lon;
    preview.arc_width_deg = kDefaultArcWidthDeg;
    preview.waypoint_name = m_pending_start.waypoint_name;
    RecalculateEbl(preview);

    if (preview.range_nm >= 0.001) {
      DrawEblVrmGL(vp, preview, true);
      rendered = true;
    }
  }

  if (m_ebl_mode == EblMode::MoveExistingTarget && m_has_cursor &&
      m_edit_item_index >= 0 &&
      static_cast<size_t>(m_edit_item_index) < m_ebl_vrms.size()) {
    RouteEblVrm preview = m_ebl_vrms[static_cast<size_t>(m_edit_item_index)];
    preview.target_lat = m_cursor_lat;
    preview.target_lon = m_cursor_lon;
    RecalculateEbl(preview);
    if (preview.range_nm >= 0.001) {
      DrawEblVrmGL(vp, preview, true);
      rendered = true;
    }
  }

  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(previous_matrix_mode);
  glPopAttrib();

  return rendered;
}

void route_toolbar_pi::DrawEblVrm(wxDC& dc, PlugIn_ViewPort* vp,
                                  const RouteEblVrm& item,
                                  bool preview) const {
  wxPoint start_px;
  wxPoint target_px;
  GetCanvasPixLL(vp, &start_px, item.start_lat, item.start_lon);
  GetCanvasPixLL(vp, &target_px, item.target_lat, item.target_lon);

  const wxPen old_pen = dc.GetPen();
  const wxBrush old_brush = dc.GetBrush();
  const wxColour old_text = dc.GetTextForeground();
  const wxFont old_font = dc.GetFont();
  const wxColour line_colour = preview ? wxColour(150, 0, 170)
                                       : wxColour(210, 0, 220);

  dc.SetPen(wxPen(line_colour, preview ? 1 : 2,
                  preview ? wxPENSTYLE_SHORT_DASH : wxPENSTYLE_SOLID));
  dc.SetBrush(*wxTRANSPARENT_BRUSH);
  dc.DrawLine(start_px, target_px);

  const double half_arc = item.arc_width_deg / 2.0;
  wxPoint previous;
  bool have_previous = false;
  for (int i = 0; i <= kArcSegments; ++i) {
    const double fraction = static_cast<double>(i) / kArcSegments;
    const double arc_bearing = NormalizeBearing(
        item.bearing_true - half_arc + fraction * item.arc_width_deg);
    double arc_lat = 0.0;
    double arc_lon = 0.0;
    PositionBearingDistanceMercator_Plugin(
        item.start_lat, item.start_lon, arc_bearing, item.range_nm,
        &arc_lat, &arc_lon);
    wxPoint arc_px;
    GetCanvasPixLL(vp, &arc_px, arc_lat, arc_lon);
    if (have_previous) dc.DrawLine(previous, arc_px);
    previous = arc_px;
    have_previous = true;
  }

  dc.SetBrush(wxBrush(line_colour));
  dc.DrawCircle(start_px, preview ? 2 : 3);
  dc.SetBrush(*wxTRANSPARENT_BRUSH);
  dc.DrawCircle(target_px, preview ? 5 : kTargetRingRadiusPx);
  const int cross = preview ? 8 : kTargetCrosshairRadiusPx;
  dc.DrawLine(target_px.x - cross, target_px.y, target_px.x + cross,
              target_px.y);
  dc.DrawLine(target_px.x, target_px.y - cross, target_px.x,
              target_px.y + cross);

  const wxString bearing_range = wxString::Format(
      _("%03.0f°T · %.2f NM"), item.bearing_true, item.range_nm);
  wxString first_line;
  if (!preview && !item.target_name.IsEmpty()) first_line = item.target_name;

  const wxFont font = GetEblLabelFont();
  dc.SetFont(font);
  const wxSize first_size = first_line.IsEmpty() ? wxSize(0, 0)
                                                  : dc.GetTextExtent(first_line);
  const wxSize second_size = dc.GetTextExtent(bearing_range);
  const int line_gap = first_line.IsEmpty() ? 0 : 2;
  const int text_width = std::max(first_size.x, second_size.x);
  const int text_height = first_size.y + line_gap + second_size.y;
  const wxPoint text_pos(target_px.x + kLabelOffsetXPx,
                         target_px.y - text_height - kLabelOffsetYPx);

  dc.SetPen(wxPen(line_colour, 1, wxPENSTYLE_SOLID));
  dc.DrawLine(target_px.x + kTargetRingRadiusPx,
              target_px.y - kTargetRingRadiusPx,
              text_pos.x - 4, text_pos.y + text_height / 2);

  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.SetBrush(wxBrush(wxColour(255, 245, 150)));
  dc.DrawRectangle(text_pos.x - 4, text_pos.y - 3, text_width + 8,
                   text_height + 6);
  dc.SetTextForeground(*wxBLACK);
  int text_y = text_pos.y;
  if (!first_line.IsEmpty()) {
    dc.DrawText(first_line, text_pos.x, text_y);
    text_y += first_size.y + line_gap;
  }
  dc.DrawText(bearing_range, text_pos.x, text_y);

  dc.SetPen(old_pen);
  dc.SetBrush(old_brush);
  dc.SetTextForeground(old_text);
  dc.SetFont(old_font);
}

void route_toolbar_pi::DrawEblVrmGL(PlugIn_ViewPort* vp,
                                    const RouteEblVrm& item,
                                    bool preview) const {
  wxPoint start_px;
  wxPoint target_px;
  GetCanvasPixLL(vp, &start_px, item.start_lat, item.start_lon);
  GetCanvasPixLL(vp, &target_px, item.target_lat, item.target_lon);

  if (preview)
    glColor4ub(150, 0, 170, 190);
  else
    glColor4ub(210, 0, 220, 255);

  glDisable(GL_TEXTURE_2D);
  glLineWidth(preview ? 1.0f : 2.0f);

  glBegin(GL_LINES);
  glVertex2i(start_px.x, start_px.y);
  glVertex2i(target_px.x, target_px.y);
  glEnd();

  const double half_arc = item.arc_width_deg / 2.0;
  glBegin(GL_LINE_STRIP);
  for (int i = 0; i <= kArcSegments; ++i) {
    const double fraction = static_cast<double>(i) / kArcSegments;
    const double arc_bearing = NormalizeBearing(
        item.bearing_true - half_arc + fraction * item.arc_width_deg);
    double arc_lat = 0.0;
    double arc_lon = 0.0;
    PositionBearingDistanceMercator_Plugin(
        item.start_lat, item.start_lon, arc_bearing, item.range_nm,
        &arc_lat, &arc_lon);
    wxPoint arc_px;
    GetCanvasPixLL(vp, &arc_px, arc_lat, arc_lon);
    glVertex2i(arc_px.x, arc_px.y);
  }
  glEnd();

  glPointSize(preview ? 5.0f : 7.0f);
  glBegin(GL_POINTS);
  glVertex2i(start_px.x, start_px.y);
  glEnd();

  const int ring = preview ? 5 : kTargetRingRadiusPx;
  glBegin(GL_LINE_LOOP);
  for (int i = 0; i < 24; ++i) {
    const double angle = (2.0 * M_PI * i) / 24.0;
    glVertex2d(target_px.x + std::cos(angle) * ring,
               target_px.y + std::sin(angle) * ring);
  }
  glEnd();
  const int cross = preview ? 8 : kTargetCrosshairRadiusPx;
  glBegin(GL_LINES);
  glVertex2i(target_px.x - cross, target_px.y);
  glVertex2i(target_px.x + cross, target_px.y);
  glVertex2i(target_px.x, target_px.y - cross);
  glVertex2i(target_px.x, target_px.y + cross);
  glEnd();

  const wxString bearing_range = wxString::Format(
      _("%03.0f°T · %.2f NM"), item.bearing_true, item.range_nm);
  wxString label = bearing_range;
  if (!preview && !item.target_name.IsEmpty())
    label = item.target_name + _("\n") + bearing_range;

  const wxFont font = GetEblLabelFont();
  const int approximate_height = std::max(18, font.GetPixelSize().y);
  const int label_x = target_px.x + kLabelOffsetXPx;
  const int label_y = target_px.y -
                      (preview ? approximate_height + kLabelOffsetYPx
                               : 2 * approximate_height + kLabelOffsetYPx);

  glBegin(GL_LINES);
  glVertex2i(target_px.x + ring, target_px.y - ring);
  glVertex2i(label_x - 4, label_y + approximate_height);
  glEnd();

  DrawGlLabel(label, label_x, label_y);
}

void route_toolbar_pi::DrawGlLabel(const wxString& label, int x, int y) const {
  if (label.IsEmpty()) return;

  const wxFont font = GetEblLabelFont();
  const int newline = label.Find('\n');
  const wxString first_line =
      newline == wxNOT_FOUND ? label : label.Left(static_cast<size_t>(newline));
  const wxString second_line = newline == wxNOT_FOUND
                                   ? wxString()
                                   : label.Mid(static_cast<size_t>(newline + 1));

  wxBitmap measure_bitmap(1, 1, 24);
  wxMemoryDC measure_dc(measure_bitmap);
  measure_dc.SetFont(font);
  const wxSize first_extent = measure_dc.GetTextExtent(first_line);
  const wxSize second_extent = second_line.IsEmpty()
                                   ? wxSize(0, 0)
                                   : measure_dc.GetTextExtent(second_line);
  measure_dc.SelectObject(wxNullBitmap);

  const int line_gap = second_line.IsEmpty() ? 0 : 2;
  const int text_width = std::max(first_extent.x, second_extent.x);
  const int text_height = first_extent.y + line_gap + second_extent.y;
  const int width = std::max(1, text_width + 8);
  const int height = std::max(1, text_height + 6);
  wxBitmap bitmap(width, height, 24);
  wxMemoryDC bitmap_dc(bitmap);
  bitmap_dc.SetBackground(wxBrush(wxColour(255, 245, 150)));
  bitmap_dc.Clear();
  bitmap_dc.SetFont(font);
  bitmap_dc.SetTextForeground(*wxBLACK);
  bitmap_dc.DrawText(first_line, 4, 3);
  if (!second_line.IsEmpty())
    bitmap_dc.DrawText(second_line, 4, 3 + first_extent.y + line_gap);
  bitmap_dc.SelectObject(wxNullBitmap);

  wxImage image = bitmap.ConvertToImage();
  if (!image.IsOk() || !image.GetData()) return;

  GLuint texture = 0;
  glGenTextures(1, &texture);
  if (!texture) return;

  glBindTexture(GL_TEXTURE_2D, texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.GetWidth(), image.GetHeight(),
               0, GL_RGB, GL_UNSIGNED_BYTE, image.GetData());

  glEnable(GL_TEXTURE_2D);
  glColor4ub(255, 255, 255, 255);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2i(x, y);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2i(x + width, y);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2i(x + width, y + height);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2i(x, y + height);
  glEnd();
  glDisable(GL_TEXTURE_2D);

  glBindTexture(GL_TEXTURE_2D, 0);
  glDeleteTextures(1, &texture);
}

double route_toolbar_pi::NormalizeBearing(double bearing) {
  bearing = std::fmod(bearing, 360.0);
  if (bearing < 0.0) bearing += 360.0;
  return bearing;
}

void route_toolbar_pi::SetStatus(const wxString& text) {
  if (m_dialog) m_dialog->SetStatus(text);
}

void route_toolbar_pi::OnDialogHidden() {
  CancelInteractiveTool();
  m_dialog_visible = false;
  if (m_toolbar_id >= 0) SetToolbarItemState(m_toolbar_id, false);
}
