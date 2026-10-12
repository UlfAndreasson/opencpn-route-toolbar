// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_pi.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#include <wx/brush.h>
#include <wx/button.h>
#include <wx/timer.h>
#include <wx/choicdlg.h>
#include <wx/dc.h>
#include <wx/dcmemory.h>
#include <wx/ffile.h>
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/font.h>
#include <wx/image.h>
#include <wx/listctrl.h>
#include <wx/intl.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
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
constexpr int kPluginVersionMinor = 5;
constexpr int kPluginVersionPatch = 3;
constexpr double kWaypointPickRadiusNm = 0.10;
constexpr double kTargetPickRadiusNm = 0.025;
constexpr double kTurnAnchorPickRadiusNm = 0.025;
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

  const wxString path = dir + wxFILE_SEP_PATH + "route-toolbar-v041.svg";
  static const char* svg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg"
     width="32" height="32" viewBox="0 0 32 32">

  <!-- dark background -->
  <rect x="1.8" y="1.8" width="28.4" height="28.4" rx="2.6"
        fill="#2b2d2f"
        stroke="#cfd3d6"
        stroke-width="0.9"/>

  <!-- route line -->
  <path d="M8.3 9.2 L15.8 20.0 L23.6 11.3"
        fill="none"
        stroke="#eef1f3"
        stroke-width="1.5"
        stroke-linecap="round"
        stroke-linejoin="round"/>

  <!-- waypoints -->
  <circle cx="8.3" cy="9.2" r="2.35"
          fill="none"
          stroke="#eef1f3"
          stroke-width="1.35"/>

  <circle cx="15.8" cy="20.0" r="2.35"
          fill="none"
          stroke="#eef1f3"
          stroke-width="1.35"/>

  <circle cx="23.6" cy="11.3" r="2.35"
          fill="none"
          stroke="#eef1f3"
          stroke-width="1.35"/>

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

wxPoint ClosestPointOnLabelBox(const wxPoint& target,
                               const wxPoint& box,
                               int box_width,
                               int box_height) {
  return wxPoint(
      std::clamp(target.x, box.x, box.x + box_width),
      std::clamp(target.y, box.y, box.y + box_height));
}

wxPoint PointOnTargetRingTowards(const wxPoint& target,
                                 const wxPoint& towards,
                                 int radius) {
  const double dx = static_cast<double>(towards.x - target.x);
  const double dy = static_cast<double>(towards.y - target.y);
  const double length = std::hypot(dx, dy);

  if (length < 0.5) return target;

  return wxPoint(
      target.x + static_cast<int>(std::lround(radius * dx / length)),
      target.y + static_cast<int>(std::lround(radius * dy / length)));
}


int ShowRouteToolbarActionButtons(wxWindow* parent,
                                  const wxString& title,
                                  const wxString& prompt,
                                  const wxArrayString& labels) {
  wxDialog dialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
                  wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP);
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
  dialog.CentreOnParent();
  const int result = dialog.ShowModal();
  return result > 0 ? result - 1 : -1;
}



class RouteToolbarDelayedLoadTimer final : public wxTimer {
public:
  explicit RouteToolbarDelayedLoadTimer(route_toolbar_pi* owner)
      : m_owner(owner) {}

  void Notify() override {
    if (m_owner) m_owner->OnDelayedStateLoad();
  }

private:
  route_toolbar_pi* m_owner{nullptr};
};

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
  // OpenCPN routes/marks are not guaranteed ready during plugin Init().
  // Load Route Toolbar state automatically after core startup has settled.
  m_delayed_state_load =
      new RouteToolbarDelayedLoadTimer(this);
  m_delayed_state_load->StartOnce(3000);
  // Persistent Route Toolbar objects are loaded later, after OpenCPN
  // has finished initializing routes/marks.

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

  // Context menu must not be registered synchronously during Init on macOS.
  return WANTS_TOOLBAR_CALLBACK | INSTALLS_TOOLBAR_TOOL |
         WANTS_CURSOR_LATLON | WANTS_MOUSE_EVENTS | WANTS_OVERLAY_CALLBACK |
         WANTS_OPENGL_OVERLAY_CALLBACK;
}

bool route_toolbar_pi::DeInit() {
  if (m_delayed_state_load) {
    delete m_delayed_state_load;
    m_delayed_state_load = nullptr;
  }
  if (m_state_loaded) SavePersistentState();
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

void route_toolbar_pi::OnDelayedStateLoad() {
  if (m_state_loaded) return;

  if (m_delayed_state_load && m_delayed_state_load->IsRunning())
    m_delayed_state_load->Stop();

  LoadPersistentState();
  m_state_loaded = true;

  // Register chart context-menu entries only after OpenCPN startup.
  EnsureContextMenu();

  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::ShowToolWindow() {
  // Fallback if user opens Route Toolbar before the delayed load fires.
  if (!m_state_loaded) OnDelayedStateLoad();

  EnsureContextMenu();

  if (!m_dialog)
    m_dialog = new RouteToolbarDialog(m_parent_window, this);

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
  m_turn_mode = TurnMode::Idle;
  m_pil_mode = PilMode::Idle;
  m_pending_leg = RouteLegHit{};
  m_pending_start = RouteWaypointHit{};
  m_edit_item_index = -1;
  m_edit_pil_index = -1;
  m_ebl_mode = EblMode::AwaitRoutePoint;
  SetStatus(_("Route EBL/VRM: click exactly where the EBL/VRM shall start. Hold Shift while clicking to snap to a nearby TURN point, route waypoint or OpenCPN mark. Then click the sight mark. Right-click to cancel."));
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::CancelInteractiveTool() {
  m_ebl_mode = EblMode::Idle;
  m_pending_start = RouteWaypointHit{};
  m_edit_item_index = -1;
  m_pil_mode = PilMode::Idle;
  m_pending_leg = RouteLegHit{};
  m_edit_pil_index = -1;
  m_turn_mode = TurnMode::Idle;
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::SetCursorLatLon(double lat, double lon) {
  m_cursor_lat = lat;
  m_cursor_lon = lon;
  m_has_cursor = true;
  if ((m_ebl_mode == EblMode::AwaitTarget ||
       m_ebl_mode == EblMode::MoveExistingStart ||
       m_ebl_mode == EblMode::MoveExistingTarget ||
       m_ebl_mode == EblMode::MoveExistingLabel ||
       m_pil_mode == PilMode::AwaitOffsetPoint) && m_parent_window)
    RequestRefresh(m_parent_window);
}

bool route_toolbar_pi::MouseEventHook(wxMouseEvent& event) {

  if (m_turn_mode != TurnMode::Idle) {
    if (event.RightDown()) {
      m_turn_mode = TurnMode::Idle;
      SetStatus(_("Route TURN stopped. Existing turns remain visible."));
      if (m_parent_window) RequestRefresh(m_parent_window);
      return true;
    }

    if (!event.LeftDown() || !m_has_cursor) return false;

    const RouteWaypointHit hit = FindNearestInteriorRouteWaypoint(
        m_cursor_lat, m_cursor_lon, kWaypointPickRadiusNm);

    if (!hit.found) {
      SetStatus(_("No interior route waypoint found within 0.10 NM. "
                  "TURN requires a waypoint with both an inbound and an "
                  "outbound route leg."));
      return true;
    }

    CreateOrEditTurn(hit);
    return true;
  }

  if (m_ebl_mode == EblMode::MoveExistingLabel) {
    if (event.RightDown()) {
      m_ebl_mode = EblMode::Idle;
      m_edit_item_index = -1;
      SetStatus(_("Label move cancelled."));
      if (m_parent_window) RequestRefresh(m_parent_window);
      return true;
    }

    if (event.LeftDown())
      return CommitMoveEblLabel();

    return false;
  }

  if (m_ebl_mode == EblMode::MoveExistingStart) {
    if (event.RightDown()) {
      m_ebl_mode = EblMode::Idle;
      m_edit_item_index = -1;
      SetStatus(_("Move cancelled. The original EBL/VRM start point was kept."));
      if (m_parent_window) RequestRefresh(m_parent_window);
      return true;
    }
    if (event.LeftDown() && m_has_cursor)
      return CommitMoveEblStart(m_cursor_lat, m_cursor_lon);
    return false;
  }

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

  // Existing-object editing is only automatic while the EBL tool is idle.
  // When Route EBL/VRM is active, a click must always be allowed to create a
  // new EBL/VRM, even if it happens close to existing EBL geometry.
  if (m_dialog_visible && m_pil_mode == PilMode::Idle &&
      m_ebl_mode == EblMode::Idle) {
    const int existing = FindNearestEblGeometry(m_cursor_lat, m_cursor_lon, 0.05);
    if (existing >= 0) {
      m_ebl_mode = EblMode::Idle;
      m_pending_start = RouteWaypointHit{};
      ShowEblTargetActions(existing);
      return true;
    }
  }

  if (m_ebl_mode == EblMode::Idle) return false;
  if (m_ebl_mode == EblMode::AwaitRoutePoint) {
    RouteWaypointHit hit;

    if (event.ShiftDown()) {
      hit = FindNearestEblStart(
          m_cursor_lat, m_cursor_lon, kWaypointPickRadiusNm);

      if (!hit.found) {
        SetStatus(_("Shift-snap: no TURN point, route waypoint or OpenCPN mark was found close enough. Click normally for an exact free start, or Shift-click closer to the object."));
        return true;
      }
    } else {
      // Default behaviour: the EBL starts exactly where the user clicked.
      // It is intentionally not live-linked to a WP, Mark or TURN object.
      hit.found = true;
      hit.is_free_point = true;
      hit.lat = m_cursor_lat;
      hit.lon = m_cursor_lon;
      hit.waypoint_name = _("Free point");

      // Keep a nearby route association only as package metadata.
      const RouteLegHit nearby_leg =
          FindNearestRouteLeg(m_cursor_lat, m_cursor_lon, 0.50);
      if (nearby_leg.found) {
        hit.route_guid = nearby_leg.route_guid;
        hit.route_name = nearby_leg.route_name;
      }
    }

    m_pending_start = hit;
    m_ebl_mode = EblMode::AwaitTarget;

    wxString start_name = hit.waypoint_name;
    if (start_name.IsEmpty())
      start_name = hit.is_mark ? _("Mark") : _("Waypoint");

    if (!event.ShiftDown()) {
      SetStatus(wxString::Format(
          _("Start selected: exact free point %.5f, %.5f. "
            "Now click the lighthouse, buoy, beacon or other sight mark."),
          hit.lat, hit.lon));
    } else if (hit.is_turn_anchor) {
      const wxString anchor =
          hit.turn_anchor_is_end ? _("TURN END") : _("TURN START / WO");
      SetStatus(_("Snapped start: ") + start_name + _(" / ") + anchor +
                _(". The start is fixed at this coordinate; it will not move if the TURN changes. Now click the sight mark."));
    } else if (hit.is_mark) {
      SetStatus(_("Snapped start: mark '") + start_name +
                _("'. The start is fixed at this coordinate. Now click the sight mark."));
    } else {
      wxString route_name = hit.route_name;
      if (route_name.IsEmpty()) route_name = _("Unnamed route");
      SetStatus(_("Snapped start: ") + route_name + _(" / ") + start_name +
                _(". The start is fixed at this coordinate. Now click the sight mark."));
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

route_toolbar_pi::RouteWaypointHit route_toolbar_pi::FindNearestRouteWaypoint(
    double lat, double lon, double max_distance_nm) const {
  RouteWaypointHit best;
  best.distance_nm = std::numeric_limits<double>::max();
  const wxArrayString route_guids = GetRouteGUIDArray();
  for (size_t i = 0; i < route_guids.GetCount(); ++i) {
    auto route = GetRoute_Plugin(route_guids[i]);
    if (!route || !route->pWaypointList) continue;
    for (auto node = route->pWaypointList->GetFirst(); node; node = node->GetNext()) {
      PlugIn_Waypoint* waypoint = node->GetData();
      if (!waypoint) continue;
      double distance_nm = 0.0;
      DistanceBearingMercator_Plugin(lat, lon, waypoint->m_lat, waypoint->m_lon,
                                     nullptr, &distance_nm);
      if (distance_nm > max_distance_nm || distance_nm >= best.distance_nm) continue;
      best.found = true;
      best.is_mark = false;
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

route_toolbar_pi::RouteWaypointHit route_toolbar_pi::FindNearestStandaloneMark(
    double lat, double lon, double max_distance_nm) const {
  RouteWaypointHit best;
  best.distance_nm = std::numeric_limits<double>::max();
  std::set<wxString> route_waypoint_guids;
  const wxArrayString route_guids = GetRouteGUIDArray();
  for (size_t i = 0; i < route_guids.GetCount(); ++i) {
    auto route = GetRoute_Plugin(route_guids[i]);
    if (!route || !route->pWaypointList) continue;
    for (auto node = route->pWaypointList->GetFirst(); node; node = node->GetNext()) {
      PlugIn_Waypoint* wp = node->GetData();
      if (wp && !wp->m_GUID.IsEmpty()) route_waypoint_guids.insert(wp->m_GUID);
    }
  }
  const wxArrayString waypoint_guids = GetWaypointGUIDArray();
  for (size_t i = 0; i < waypoint_guids.GetCount(); ++i) {
    const wxString guid = waypoint_guids[i];
    if (guid.IsEmpty() || route_waypoint_guids.count(guid) != 0) continue;
    PlugIn_Waypoint mark;
    if (!GetSingleWaypoint(guid, &mark)) continue;
    double distance_nm = 0.0;
    DistanceBearingMercator_Plugin(lat, lon, mark.m_lat, mark.m_lon,
                                   nullptr, &distance_nm);
    if (distance_nm > max_distance_nm || distance_nm >= best.distance_nm) continue;
    best.found = true;
    best.is_mark = true;
    best.lat = mark.m_lat;
    best.lon = mark.m_lon;
    best.distance_nm = distance_nm;
    best.waypoint_guid = guid;
    best.waypoint_name = mark.m_MarkName;
  }
  return best;
}

route_toolbar_pi::RouteWaypointHit route_toolbar_pi::FindNearestTurnAnchor(
    double lat, double lon, double max_distance_nm) const {
  RouteWaypointHit best;
  best.distance_nm = std::numeric_limits<double>::max();

  for (const RouteTurn& turn : m_turns) {
    struct Candidate {
      double lat;
      double lon;
      bool is_end;
    };

    const Candidate candidates[] = {
        {turn.start_lat, turn.start_lon, false},
        {turn.end_lat, turn.end_lon, true},
    };

    for (const Candidate& c : candidates) {
      double distance_nm = 0.0;
      DistanceBearingMercator_Plugin(
          lat, lon, c.lat, c.lon, nullptr, &distance_nm);

      if (distance_nm > max_distance_nm ||
          distance_nm >= best.distance_nm)
        continue;

      best.found = true;
      best.is_turn_anchor = true;
      best.turn_anchor_is_end = c.is_end;
      best.lat = c.lat;
      best.lon = c.lon;
      best.distance_nm = distance_nm;
      best.route_guid = turn.route_guid;
      best.route_name = turn.route_name;
      best.waypoint_guid = turn.waypoint_guid;
      best.waypoint_name = turn.waypoint_name;
    }
  }

  return best;
}

route_toolbar_pi::RouteWaypointHit route_toolbar_pi::FindNearestEblStart(
    double lat, double lon, double max_distance_nm) const {
  // TURN START/END gets explicit priority when clicked closely.
  RouteWaypointHit turn =
      FindNearestTurnAnchor(lat, lon, kTurnAnchorPickRadiusNm);
  if (turn.found) return turn;

  RouteWaypointHit route_wp =
      FindNearestRouteWaypoint(lat, lon, max_distance_nm);
  RouteWaypointHit mark =
      FindNearestStandaloneMark(lat, lon, max_distance_nm);

  if (!route_wp.found) return mark;
  if (!mark.found) return route_wp;
  return mark.distance_nm < route_wp.distance_nm ? mark : route_wp;
}

bool route_toolbar_pi::RefreshEblStartFromMark(RouteEblVrm& item) const {
  if (!item.start_is_mark || item.start_mark_guid.IsEmpty()) return false;
  PlugIn_Waypoint mark;
  if (!GetSingleWaypoint(item.start_mark_guid, &mark)) return false;
  item.start_lat = mark.m_lat;
  item.start_lon = mark.m_lon;
  if (!mark.m_MarkName.IsEmpty()) item.start_mark_name = mark.m_MarkName;
  return true;
}

bool route_toolbar_pi::RefreshEblStartFromTurn(RouteEblVrm& item) const {
  if (!item.start_is_turn ||
      item.start_turn_route_guid.IsEmpty() ||
      item.start_turn_waypoint_guid.IsEmpty())
    return false;

  for (const RouteTurn& turn : m_turns) {
    if (turn.route_guid != item.start_turn_route_guid ||
        turn.waypoint_guid != item.start_turn_waypoint_guid)
      continue;

    if (item.start_turn_end) {
      item.start_lat = turn.end_lat;
      item.start_lon = turn.end_lon;
    } else {
      item.start_lat = turn.start_lat;
      item.start_lon = turn.start_lon;
    }

    if (!turn.waypoint_name.IsEmpty())
      item.start_turn_waypoint_name = turn.waypoint_name;

    // Keep the general route/waypoint identity aligned as well.
    item.route_guid = turn.route_guid;
    item.route_name = turn.route_name;
    item.waypoint_guid = turn.waypoint_guid;
    item.waypoint_name = turn.waypoint_name;
    return true;
  }

  return false;
}

int route_toolbar_pi::FindNearestEblTarget(double lat, double lon,
                                           double max_distance_nm) const {
  int best_index = -1;
  double best_distance = std::numeric_limits<double>::max();
  for (size_t i = 0; i < m_ebl_vrms.size(); ++i) {
    if (!m_ebl_vrms[i].visible) continue;

    double distance_nm = 0.0;
    DistanceBearingMercator_Plugin(lat, lon, m_ebl_vrms[i].target_lat,
                                   m_ebl_vrms[i].target_lon, nullptr, &distance_nm);
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
  wxString name = item.target_name;
  if (name.IsEmpty()) name = _("Unnamed sight mark");

  wxArrayString actions;
  actions.Add(_("Move start point"));
  actions.Add(_("Move target"));
  actions.Add(_("Move label"));
  actions.Add(_("Reset label position"));
  actions.Add(_("Rename"));
  actions.Add(_("Delete EBL/VRM"));

  const int selected = ShowRouteToolbarActionButtons(
      m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
      _("Route EBL / VRM"),
      _("What do you want to do with ") + name + _("?"),
      actions);

  // wxWidgets/macOS can leave the modeless Route Toolbar visually disabled
  // after a modal action dialog closes. Explicitly restore and repaint it
  // before entering an interactive move operation.
  if (m_dialog) {
    m_dialog->Enable(true);
    m_dialog->Show();
    m_dialog->Raise();
    m_dialog->Refresh();
    m_dialog->Update();
  }

  switch (selected) {
    case 0: BeginMoveEblStart(index); break;
    case 1: BeginMoveEblTarget(index); break;
    case 2: BeginMoveEblLabel(index); break;
    case 3: ResetEblLabel(index); break;
    case 4: RenameEblTarget(index); break;
    case 5: DeleteEblTarget(index); break;
    default: break;
  }
}

void route_toolbar_pi::BeginMoveEblStart(int index) {
  if (index < 0 || static_cast<size_t>(index) >= m_ebl_vrms.size()) return;
  m_edit_item_index = index;
  m_ebl_mode = EblMode::MoveExistingStart;
  SetStatus(_("Move EBL/VRM start point — click the new start position. Right-click to cancel."));
  if (m_parent_window) RequestRefresh(m_parent_window);
}

bool route_toolbar_pi::CommitMoveEblStart(double start_lat, double start_lon) {
  if (m_edit_item_index < 0 || static_cast<size_t>(m_edit_item_index) >= m_ebl_vrms.size()) {
    m_ebl_mode = EblMode::Idle;
    m_edit_item_index = -1;
    return true;
  }
  RouteEblVrm moved = m_ebl_vrms[static_cast<size_t>(m_edit_item_index)];
  moved.start_lat = start_lat;
  moved.start_lon = start_lon;
  moved.start_is_mark = false;
  moved.start_mark_guid.clear();
  moved.start_mark_name.clear();
  moved.start_is_turn = false;
  moved.start_turn_end = false;
  moved.start_turn_route_guid.clear();
  moved.start_turn_waypoint_guid.clear();
  moved.start_turn_waypoint_name.clear();
  moved.start_is_free = true;
  moved.route_guid.clear();
  moved.route_name.clear();
  moved.waypoint_guid.clear();
  moved.waypoint_name.clear();
  RecalculateEbl(moved);
  if (moved.range_nm < 0.001) {
    SetStatus(_("Start point is too close to the target. Click another position or right-click to cancel."));
    return true;
  }
  m_ebl_vrms[static_cast<size_t>(m_edit_item_index)] = moved;
  SavePersistentState();
  SetStatus(wxString::Format(_("EBL/VRM start moved — %03.0f°T · %.2f NM"),
                             moved.bearing_true, moved.range_nm));
  m_ebl_mode = EblMode::Idle;
  m_edit_item_index = -1;
  if (m_parent_window) RequestRefresh(m_parent_window);
  return true;
}

void route_toolbar_pi::BeginMoveEblTarget(int index) {
  if (index < 0 || static_cast<size_t>(index) >= m_ebl_vrms.size()) return;
  m_edit_item_index = index;
  m_ebl_mode = EblMode::MoveExistingTarget;
  wxString name = m_ebl_vrms[static_cast<size_t>(index)].target_name;
  if (name.IsEmpty()) name = _("sight mark");
  SetStatus(_("Move ") + name + _(" — click the new target position. Right-click to cancel."));
  if (m_parent_window) RequestRefresh(m_parent_window);
}

bool route_toolbar_pi::CommitMoveEblTarget(double target_lat, double target_lon) {
  if (m_edit_item_index < 0 || static_cast<size_t>(m_edit_item_index) >= m_ebl_vrms.size()) {
    m_ebl_mode = EblMode::Idle;
    m_edit_item_index = -1;
    return true;
  }
  RouteEblVrm moved = m_ebl_vrms[static_cast<size_t>(m_edit_item_index)];
  moved.target_lat = target_lat;
  moved.target_lon = target_lon;
  RecalculateEbl(moved);
  if (moved.range_nm < 0.001) {
    SetStatus(_("Target is too close to the start point. Click another position or right-click to cancel."));
    return true;
  }
  m_ebl_vrms[static_cast<size_t>(m_edit_item_index)] = moved;
  SavePersistentState();
  wxString name = moved.target_name;
  if (name.IsEmpty()) name = _("Sight mark");
  SetStatus(name + _(" moved — ") + wxString::Format(_("%03.0f°T · %.2f NM"),
                                                      moved.bearing_true, moved.range_nm));
  m_ebl_mode = EblMode::Idle;
  m_edit_item_index = -1;
  if (m_parent_window) RequestRefresh(m_parent_window);
  return true;
}


void route_toolbar_pi::BeginMoveEblLabel(int index) {
  if (index < 0 ||
      static_cast<size_t>(index) >= m_ebl_vrms.size())
    return;

  m_edit_item_index = index;
  m_ebl_mode = EblMode::MoveExistingLabel;

  const RouteEblVrm& item =
      m_ebl_vrms[static_cast<size_t>(index)];

  m_pending_label_dx_px =
      item.label_manual ? item.label_dx_px : 18;
  m_pending_label_dy_px =
      item.label_manual ? item.label_dy_px : -36;

  SetStatus(
      _("Move the yellow EBL/VRM label with the mouse. "
        "Click to save, right-click to cancel."));

  if (m_parent_window)
    RequestRefresh(m_parent_window);
}

bool route_toolbar_pi::CommitMoveEblLabel() {
  if (m_edit_item_index < 0 ||
      static_cast<size_t>(m_edit_item_index) >= m_ebl_vrms.size()) {
    m_ebl_mode = EblMode::Idle;
    m_edit_item_index = -1;
    return true;
  }

  RouteEblVrm& item =
      m_ebl_vrms[static_cast<size_t>(m_edit_item_index)];

  item.label_manual = true;
  item.label_dx_px = m_pending_label_dx_px;
  item.label_dy_px = m_pending_label_dy_px;

  SavePersistentState();

  m_ebl_mode = EblMode::Idle;
  m_edit_item_index = -1;

  SetStatus(_("EBL/VRM label position saved."));

  if (m_parent_window)
    RequestRefresh(m_parent_window);

  return true;
}

void route_toolbar_pi::ResetEblLabel(int index) {
  if (index < 0 ||
      static_cast<size_t>(index) >= m_ebl_vrms.size())
    return;

  RouteEblVrm& item =
      m_ebl_vrms[static_cast<size_t>(index)];

  item.label_manual = false;
  item.label_dx_px = 0;
  item.label_dy_px = 0;

  SavePersistentState();

  SetStatus(_("EBL/VRM label returned to automatic position."));

  if (m_parent_window)
    RequestRefresh(m_parent_window);
}

void route_toolbar_pi::RenameEblTarget(int index) {
  if (index < 0 || static_cast<size_t>(index) >= m_ebl_vrms.size()) return;
  RouteEblVrm& item = m_ebl_vrms[static_cast<size_t>(index)];
  wxTextEntryDialog dialog(m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
                           _("Enter a new sight-mark / target name."),
                           _("Route EBL / VRM"), item.target_name);
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

  const RouteEblVrm selected = m_ebl_vrms[static_cast<size_t>(index)];
  wxString name = selected.target_name;
  if (name.IsEmpty()) name = _("Unnamed sight mark");

  size_t same_target_count = 0;
  for (const RouteEblVrm& item : m_ebl_vrms) {
    if (std::fabs(item.target_lat - selected.target_lat) < 1e-7 &&
        std::fabs(item.target_lon - selected.target_lon) < 1e-7 &&
        item.target_name == selected.target_name)
      ++same_target_count;
  }

  bool delete_all = false;
  if (same_target_count > 1) {
    wxMessageDialog choice(
        m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
        wxString::Format(
            _("%lu EBL/VRM objects use '%s' at the same target position."),
            static_cast<unsigned long>(same_target_count), name),
        _("Delete EBL/VRM"),
        wxYES_NO | wxCANCEL | wxICON_QUESTION);
    choice.SetYesNoLabels(_("Delete all"), _("Delete selected only"));
    const int answer = choice.ShowModal();
    if (answer == wxID_CANCEL) return;
    delete_all = answer == wxID_YES;
  } else {
    wxMessageDialog confirm(
        m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
        _("Delete Route EBL/VRM target '") + name + _("'?"),
        _("Route EBL / VRM"),
        wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
    if (confirm.ShowModal() != wxID_YES) return;
  }

  if (delete_all) {
    m_ebl_vrms.erase(
        std::remove_if(
            m_ebl_vrms.begin(), m_ebl_vrms.end(),
            [&](const RouteEblVrm& item) {
              return std::fabs(item.target_lat - selected.target_lat) < 1e-7 &&
                     std::fabs(item.target_lon - selected.target_lon) < 1e-7 &&
                     item.target_name == selected.target_name;
            }),
        m_ebl_vrms.end());
  } else {
    m_ebl_vrms.erase(m_ebl_vrms.begin() + index);
  }

  m_edit_item_index = -1;
  m_ebl_mode = EblMode::Idle;
  SavePersistentState();
  SetStatus(_("Route EBL/VRM deleted."));
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::RecalculateEbl(RouteEblVrm& item) const {
  // EBL/VRM starts are static coordinates. Metadata may remember that the
  // point was Shift-snapped to a Mark/WP/TURN, but it is never live-linked.
  DistanceBearingMercator_Plugin(item.target_lat, item.target_lon,
                                 item.start_lat, item.start_lon,
                                 &item.bearing_true, &item.range_nm);
  item.bearing_true = NormalizeBearing(item.bearing_true);
}

void route_toolbar_pi::CompleteEblVrm(double target_lat, double target_lon) {
  if (!m_pending_start.found) {
    m_ebl_mode = EblMode::AwaitRoutePoint;
    SetStatus(_("EBL/VRM start point was lost. Click a route waypoint or mark again."));
    return;
  }
  RouteEblVrm item;
  item.start_lat = m_pending_start.lat;
  item.start_lon = m_pending_start.lon;
  item.target_lat = target_lat;
  item.target_lon = target_lon;
  item.arc_width_deg = kDefaultArcWidthDeg;
  item.start_is_mark = m_pending_start.is_mark;
  item.start_is_turn = m_pending_start.is_turn_anchor;
  item.start_turn_end = m_pending_start.turn_anchor_is_end;
  item.start_is_free = m_pending_start.is_free_point;

  if (item.start_is_mark) {
    item.start_mark_guid = m_pending_start.waypoint_guid;
    item.start_mark_name = m_pending_start.waypoint_name;
  } else {
    // Route association is retained for route-WP, TURN and nearby free starts.
    item.route_guid = m_pending_start.route_guid;
    item.route_name = m_pending_start.route_name;
    item.waypoint_guid = m_pending_start.waypoint_guid;
    item.waypoint_name = m_pending_start.waypoint_name;

    if (item.start_is_turn) {
      item.start_turn_route_guid = m_pending_start.route_guid;
      item.start_turn_waypoint_guid = m_pending_start.waypoint_guid;
      item.start_turn_waypoint_name = m_pending_start.waypoint_name;
    }
  }
  RecalculateEbl(item);
  if (item.range_nm < 0.001) {
    SetStatus(_("Target is too close to the start point. Click the sight mark again."));
    return;
  }
  const wxString default_name = wxString::Format(
      _("Sight mark %lu"), static_cast<unsigned long>(m_ebl_vrms.size() + 1));
  item.target_name = default_name;
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
  wxString start_name;
  if (item.start_is_turn) {
    start_name = item.start_turn_waypoint_name;
    if (start_name.IsEmpty()) start_name = _("Waypoint");
    start_name += item.start_turn_end ? _(" / TURN END (fixed)") : _(" / TURN START (fixed)");
  } else if (item.start_is_free) {
    start_name = _("Free point");
  } else {
    start_name = item.start_is_mark ? item.start_mark_name : item.waypoint_name;
    if (start_name.IsEmpty())
      start_name = item.start_is_mark ? _("Mark") : _("Waypoint");
  }
  SetStatus(start_name + _(" → ") + item.target_name + _("  ") +
            wxString::Format(_("%03.0f°T  %.2f NM. Saved. Click another route waypoint/mark, click an existing target to edit, or right-click to finish."),
                             item.bearing_true, item.range_nm));
  m_pending_start = RouteWaypointHit{};
  m_ebl_mode = EblMode::AwaitRoutePoint;
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::LoadPersistentState() {
  m_ebl_vrms.clear();
  m_pils.clear();
  m_turns.clear();
  wxFileConfig* config = GetOCPNConfigObject();
  if (!config) return;
  const wxString old_path = config->GetPath();
  config->SetPath(wxString::FromUTF8(kConfigPath));
  long font_size = kDefaultLabelFontSize;
  config->Read("LabelFontSize", &font_size, static_cast<long>(kDefaultLabelFontSize));
  m_label_font_size = std::clamp(static_cast<int>(font_size), kMinLabelFontSize,
                                 kMaxLabelFontSize);
  config->Read("NotesVisible", &m_notes_visible, true);
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
  colour_text.clear();
  if (config->Read("TurnColour", &colour_text) && !colour_text.IsEmpty()) {
    wxColour parsed;
    if (parsed.Set(colour_text)) m_turn_colour = parsed;
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
    config->Read(prefix + "Visible", &item.visible, true);
    config->Read(prefix + "ArcWidthDeg", &item.arc_width_deg, kDefaultArcWidthDeg);
    config->Read(prefix + "RouteGuid", &item.route_guid, wxEmptyString);
    config->Read(prefix + "RouteName", &item.route_name, wxEmptyString);
    config->Read(prefix + "WaypointGuid", &item.waypoint_guid, wxEmptyString);
    config->Read(prefix + "WaypointName", &item.waypoint_name, wxEmptyString);
    config->Read(prefix + "StartIsMark", &item.start_is_mark, false);
    config->Read(prefix + "StartMarkGuid", &item.start_mark_guid, wxEmptyString);
    config->Read(prefix + "StartMarkName", &item.start_mark_name, wxEmptyString);
    config->Read(prefix + "StartIsTurn", &item.start_is_turn, false);
    config->Read(prefix + "StartTurnEnd", &item.start_turn_end, false);
    config->Read(prefix + "StartIsFree", &item.start_is_free, false);
    config->Read(prefix + "StartTurnRouteGuid",
                 &item.start_turn_route_guid, wxEmptyString);
    config->Read(prefix + "StartTurnWaypointGuid",
                 &item.start_turn_waypoint_guid, wxEmptyString);
    config->Read(prefix + "StartTurnWaypointName",
                 &item.start_turn_waypoint_name, wxEmptyString);
    config->Read(prefix + "TargetName", &item.target_name, wxEmptyString);
    config->Read(prefix + "LabelManual", &item.label_manual, false);

    long label_dx = 0;
    long label_dy = 0;

    config->Read(prefix + "LabelDxPx", &label_dx, 0L);
    config->Read(prefix + "LabelDyPx", &label_dy, 0L);

    item.label_dx_px = static_cast<int>(label_dx);
    item.label_dy_px = static_cast<int>(label_dy);
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
  LoadTurnPersistentState(config);
  config->SetPath(old_path);
}

void route_toolbar_pi::SavePersistentState() const {
  wxFileConfig* config = GetOCPNConfigObject();
  if (!config) return;
  const wxString old_path = config->GetPath();
  config->SetPath(wxString::FromUTF8(kConfigPath));
  config->Write("LabelFontSize", static_cast<long>(m_label_font_size));
  config->Write("NotesVisible", m_notes_visible);
  config->Write("EblColour", m_ebl_colour.GetAsString(wxC2S_HTML_SYNTAX));
  config->Write("PilColour", m_pil_colour.GetAsString(wxC2S_HTML_SYNTAX));
  config->Write("TurnColour", m_turn_colour.GetAsString(wxC2S_HTML_SYNTAX));
  config->DeleteGroup("EBLVRM");
  config->SetPath("EBLVRM");
  config->Write("Count", static_cast<long>(m_ebl_vrms.size()));
  for (size_t i = 0; i < m_ebl_vrms.size(); ++i) {

    const wxString prefix = wxString::Format("Item%03lu/", static_cast<unsigned long>(i));
    const RouteEblVrm& item = m_ebl_vrms[i];
    config->Write(prefix + "Visible", item.visible);
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
    config->Write(prefix + "StartIsTurn", item.start_is_turn);
    config->Write(prefix + "StartTurnEnd", item.start_turn_end);
    config->Write(prefix + "StartIsFree", item.start_is_free);
    config->Write(prefix + "StartTurnRouteGuid", item.start_turn_route_guid);
    config->Write(prefix + "StartTurnWaypointGuid", item.start_turn_waypoint_guid);
    config->Write(prefix + "StartTurnWaypointName", item.start_turn_waypoint_name);
    config->Write(prefix + "TargetName", item.target_name);
    config->Write(prefix + "LabelManual", item.label_manual);
    config->Write(prefix + "LabelDxPx",
                  static_cast<long>(item.label_dx_px));
    config->Write(prefix + "LabelDyPx",
                  static_cast<long>(item.label_dy_px));
  }
  SavePilPersistentState(config);
  SaveTurnPersistentState(config);
  config->SetPath(old_path);
  config->Flush();
}

bool route_toolbar_pi::RenderOverlay(wxDC& dc, PlugIn_ViewPort* vp) {
  if (!vp || !vp->bValid) return false;
  bool rendered = false;
  DrawRouteLegLabels(dc, vp);
  rendered = true;
  for (const RouteTurn& turn : m_turns) {
    if (!turn.visible) continue;
    DrawTurn(dc, vp, turn);
    rendered = true;
  }
  for (size_t i = 0; i < m_pils.size(); ++i) {
    if (!m_pils[i].visible) continue;
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
    if (!m_ebl_vrms[i].visible) continue;
    if (m_ebl_mode == EblMode::MoveExistingLabel &&
        static_cast<int>(i) == m_edit_item_index &&
        m_has_cursor) {
      RouteEblVrm preview = m_ebl_vrms[i];

      wxPoint target_px;
      wxPoint cursor_px;

      GetCanvasPixLL(vp, &target_px,
                     preview.target_lat, preview.target_lon);
      GetCanvasPixLL(vp, &cursor_px,
                     m_cursor_lat, m_cursor_lon);

      preview.label_manual = true;
      preview.label_dx_px = cursor_px.x - target_px.x;
      preview.label_dy_px = cursor_px.y - target_px.y;

      m_pending_label_dx_px = preview.label_dx_px;
      m_pending_label_dy_px = preview.label_dy_px;

      DrawEblVrm(dc, vp, preview, false);
      rendered = true;
      continue;
    }
    if (m_ebl_mode == EblMode::MoveExistingTarget && static_cast<int>(i) == m_edit_item_index)
      continue;
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
    if (preview.range_nm >= 0.001) {
      DrawEblVrm(dc, vp, preview, true);
      rendered = true;
    }
  }
  return rendered;
}

bool route_toolbar_pi::RenderGLOverlay(wxGLContext*, PlugIn_ViewPort* vp) {
  if (!vp || !vp->bValid || vp->pix_width <= 0 || vp->pix_height <= 0) return false;
  bool rendered = false;
  GLint previous_matrix_mode = GL_MODELVIEW;
  glGetIntegerv(GL_MATRIX_MODE, &previous_matrix_mode);
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0.0, static_cast<double>(vp->pix_width), static_cast<double>(vp->pix_height), 0.0, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  DrawRouteLegLabelsGL(vp);
  rendered = true;

  for (const RouteTurn& turn : m_turns) {
    if (!turn.visible) continue;
    DrawTurnGL(vp, turn);
    rendered = true;
  }

  for (size_t i = 0; i < m_pils.size(); ++i) {
    if (!m_pils[i].visible) continue;
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
    if (!m_ebl_vrms[i].visible) continue;
    if (m_ebl_mode == EblMode::MoveExistingLabel &&
        static_cast<int>(i) == m_edit_item_index &&
        m_has_cursor) {
      RouteEblVrm preview = m_ebl_vrms[i];

      wxPoint target_px;
      wxPoint cursor_px;

      GetCanvasPixLL(vp, &target_px,
                     preview.target_lat, preview.target_lon);
      GetCanvasPixLL(vp, &cursor_px,
                     m_cursor_lat, m_cursor_lon);

      preview.label_manual = true;
      preview.label_dx_px = cursor_px.x - target_px.x;
      preview.label_dy_px = cursor_px.y - target_px.y;

      m_pending_label_dx_px = preview.label_dx_px;
      m_pending_label_dy_px = preview.label_dy_px;

      DrawEblVrmGL(vp, preview, false);
      rendered = true;
      continue;
    }
    if (m_ebl_mode == EblMode::MoveExistingTarget && static_cast<int>(i) == m_edit_item_index)
      continue;
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
  glPopClientAttrib();
  glPopAttrib();
  return rendered;
}


wxPoint route_toolbar_pi::FindSafeLabelBox(
    PlugIn_ViewPort* vp,
    const RouteEblVrm& item,
    const wxPoint& target_px,
    int box_width,
    int box_height) const {

  if (item.label_manual) {
    const int margin = 4;

    wxPoint p(target_px.x + item.label_dx_px,
              target_px.y + item.label_dy_px);

    p.x = std::clamp(
        p.x, margin,
        std::max(margin, vp->pix_width - box_width - margin));

    p.y = std::clamp(
        p.y, margin,
        std::max(margin, vp->pix_height - box_height - margin));

    return p;
  }

  const int margin = 6;
  const int gap = 20;

  std::vector<wxPoint> obstacles;
  std::set<wxString> seen_guids;

  auto add_ll = [&](double lat, double lon) {
    if (!std::isfinite(lat) || !std::isfinite(lon)) return;
    wxPoint p;
    GetCanvasPixLL(vp, &p, lat, lon);
    obstacles.push_back(p);
  };

  // All route waypoints.
  const wxArrayString routes = GetRouteGUIDArray();
  for (size_t i = 0; i < routes.GetCount(); ++i) {
    auto route = GetRoute_Plugin(routes[i]);
    if (!route || !route->pWaypointList) continue;

    for (auto node = route->pWaypointList->GetFirst();
         node;
         node = node->GetNext()) {
      PlugIn_Waypoint* wp = node->GetData();
      if (!wp) continue;

      add_ll(wp->m_lat, wp->m_lon);
      if (!wp->m_GUID.IsEmpty())
        seen_guids.insert(wp->m_GUID);
    }
  }

  // Standalone OpenCPN marks.
  const wxArrayString waypoints = GetWaypointGUIDArray();
  for (size_t i = 0; i < waypoints.GetCount(); ++i) {
    const wxString guid = waypoints[i];
    if (guid.IsEmpty() || seen_guids.count(guid)) continue;

    PlugIn_Waypoint mark;
    if (GetSingleWaypoint(guid, &mark))
      add_ll(mark.m_lat, mark.m_lon);
  }

  // Other Route Toolbar geometry should also remain readable.
  for (const auto& e : m_ebl_vrms) {
    add_ll(e.start_lat, e.start_lon);
    add_ll(e.target_lat, e.target_lon);
  }

  for (const auto& p : m_pils)
    add_ll(p.anchor_lat, p.anchor_lon);

  std::vector<wxPoint> candidates = {
      wxPoint(target_px.x + gap,
              target_px.y - box_height - gap),       // NE
      wxPoint(target_px.x - box_width - gap,
              target_px.y - box_height - gap),       // NW
      wxPoint(target_px.x + gap,
              target_px.y + gap),                    // SE
      wxPoint(target_px.x - box_width - gap,
              target_px.y + gap),                    // SW
      wxPoint(target_px.x + gap,
              target_px.y - box_height / 2),         // E
      wxPoint(target_px.x - box_width - gap,
              target_px.y - box_height / 2),         // W
      wxPoint(target_px.x - box_width / 2,
              target_px.y - box_height - gap),       // N
      wxPoint(target_px.x - box_width / 2,
              target_px.y + gap)                     // S
  };

  wxPoint start_px;
  GetCanvasPixLL(vp, &start_px, item.start_lat, item.start_lon);

  long long best_score = std::numeric_limits<long long>::max();
  wxPoint best = candidates.front();

  for (wxPoint candidate : candidates) {
    const wxPoint original = candidate;

    candidate.x = std::clamp(
        candidate.x, margin,
        std::max(margin, vp->pix_width - box_width - margin));

    candidate.y = std::clamp(
        candidate.y, margin,
        std::max(margin, vp->pix_height - box_height - margin));

    long long score = 0;

    // Prefer not to clamp against chart edge.
    score += 1000LL *
             (std::abs(candidate.x - original.x) +
              std::abs(candidate.y - original.y));

    // Keep a safety zone around all known navigation objects.
    const int clearance = 18;
    const int left   = candidate.x - clearance;
    const int right  = candidate.x + box_width + clearance;
    const int top    = candidate.y - clearance;
    const int bottom = candidate.y + box_height + clearance;

    for (const wxPoint& p : obstacles) {
      int dx = 0;
      int dy = 0;

      if (p.x < left) dx = left - p.x;
      else if (p.x > right) dx = p.x - right;

      if (p.y < top) dy = top - p.y;
      else if (p.y > bottom) dy = p.y - bottom;

      if (dx == 0 && dy == 0) {
        // Never choose a box which covers/approaches a WP or Mark.
        score += 100000000LL;
      } else {
        const long long d2 =
            static_cast<long long>(dx) * dx +
            static_cast<long long>(dy) * dy;

        if (d2 < 2500)
          score += (2500 - d2) * 20;
      }
    }

    // Prefer the side away from the EBL start point.
    const long long cx =
        candidate.x + box_width / 2 - target_px.x;
    const long long cy =
        candidate.y + box_height / 2 - target_px.y;
    const long long sx = start_px.x - target_px.x;
    const long long sy = start_px.y - target_px.y;

    if (cx * sx + cy * sy > 0)
      score += 10000;

    if (score < best_score) {
      best_score = score;
      best = candidate;
    }
  }

  return best;
}

void route_toolbar_pi::DrawEblVrm(wxDC& dc, PlugIn_ViewPort* vp,
                                  const RouteEblVrm& item, bool preview) const {
  wxPoint start_px, target_px;
  GetCanvasPixLL(vp, &start_px, item.start_lat, item.start_lon);
  GetCanvasPixLL(vp, &target_px, item.target_lat, item.target_lon);
  const wxPen old_pen = dc.GetPen();
  const wxBrush old_brush = dc.GetBrush();
  const wxColour old_text = dc.GetTextForeground();
  const wxFont old_font = dc.GetFont();
  const wxColour line_colour = preview ? PreviewColour(m_ebl_colour) : m_ebl_colour;
  dc.SetPen(wxPen(line_colour, preview ? 1 : 2,
                  preview ? wxPENSTYLE_SHORT_DASH : wxPENSTYLE_SOLID));
  dc.SetBrush(*wxTRANSPARENT_BRUSH);
  dc.DrawLine(start_px, target_px);
  const double half_arc = item.arc_width_deg / 2.0;
  wxPoint previous;
  bool have_previous = false;
  for (int i = 0; i <= kArcSegments; ++i) {
    const double fraction = static_cast<double>(i) / kArcSegments;
    const double arc_bearing = NormalizeBearing(item.bearing_true - half_arc + fraction * item.arc_width_deg);
    double arc_lat = 0.0, arc_lon = 0.0;
    PositionBearingDistanceMercator_Plugin(item.start_lat, item.start_lon, arc_bearing,
                                           item.range_nm, &arc_lat, &arc_lon);
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
  dc.DrawLine(target_px.x - cross, target_px.y, target_px.x + cross, target_px.y);
  dc.DrawLine(target_px.x, target_px.y - cross, target_px.x, target_px.y + cross);

  if (!m_notes_visible) {
    dc.SetPen(old_pen);
    dc.SetBrush(old_brush);
    dc.SetTextForeground(old_text);
    dc.SetFont(old_font);
    return;
  }

  // The EBL geometry may cross the visible viewport even when the
  // actual sight mark is outside it. Draw the geometry, but do not
  // clamp an off-screen target's yellow label to a chart edge.
  constexpr int kLabelVisibilityMarginPx = 12;

  const bool label_target_visible =
      target_px.x >= -kLabelVisibilityMarginPx &&
      target_px.x <= vp->pix_width + kLabelVisibilityMarginPx &&
      target_px.y >= -kLabelVisibilityMarginPx &&
      target_px.y <= vp->pix_height + kLabelVisibilityMarginPx;

  if (!label_target_visible) {
    dc.SetPen(old_pen);
    dc.SetBrush(old_brush);
    dc.SetTextForeground(old_text);
    dc.SetFont(old_font);
    return;
  }

  const wxString bearing_range = wxString::Format(_("%03.0f°T · %.2f NM"),
                                                   item.bearing_true, item.range_nm);
  wxString first_line;
  if (!preview && !item.target_name.IsEmpty()) first_line = item.target_name;
  const wxFont font = GetEblLabelFont();
  dc.SetFont(font);
  const wxSize first_size = first_line.IsEmpty() ? wxSize(0, 0) : dc.GetTextExtent(first_line);
  const wxSize second_size = dc.GetTextExtent(bearing_range);
  const int line_gap = first_line.IsEmpty() ? 0 : 2;
  const int text_width = std::max(first_size.x, second_size.x);
  const int text_height = first_size.y + line_gap + second_size.y;
  const int box_width = text_width + 8;
  const int box_height = text_height + 6;

  wxPoint box_pos =
      FindSafeLabelBox(vp, item, target_px, box_width, box_height);

  if (item.label_manual) {
    const int max_x =
        std::max(4, vp->pix_width - box_width - 4);
    const int max_y =
        std::max(4, vp->pix_height - box_height - 4);

    box_pos.x = std::clamp(
        target_px.x + item.label_dx_px, 4, max_x);
    box_pos.y = std::clamp(
        target_px.y + item.label_dy_px, 4, max_y);
  }

  const wxPoint text_pos(box_pos.x + 4, box_pos.y + 3);

  const wxPoint leader_end =
      ClosestPointOnLabelBox(target_px, box_pos, box_width, box_height);

  const wxPoint leader_start =
      PointOnTargetRingTowards(
          target_px, leader_end,
          preview ? 5 : kTargetRingRadiusPx);

  dc.SetPen(wxPen(line_colour, 1, wxPENSTYLE_SOLID));
  dc.DrawLine(leader_start, leader_end);

  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.SetBrush(wxBrush(wxColour(255, 245, 150)));
  dc.DrawRectangle(box_pos.x, box_pos.y, box_width, box_height);
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
                                    const RouteEblVrm& item, bool preview) const {
  wxPoint start_px, target_px;
  GetCanvasPixLL(vp, &start_px, item.start_lat, item.start_lon);
  GetCanvasPixLL(vp, &target_px, item.target_lat, item.target_lon);
  const wxColour colour = preview ? PreviewColour(m_ebl_colour) : m_ebl_colour;
  glColor4ub(colour.Red(), colour.Green(), colour.Blue(), preview ? 190 : 255);
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
    const double arc_bearing = NormalizeBearing(item.bearing_true - half_arc + fraction * item.arc_width_deg);
    double arc_lat = 0.0, arc_lon = 0.0;
    PositionBearingDistanceMercator_Plugin(item.start_lat, item.start_lon, arc_bearing,
                                           item.range_nm, &arc_lat, &arc_lon);
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

  if (!m_notes_visible) return;

  // Do not pin labels belonging to off-screen sight marks to
  // the edge of the visible chart.
  constexpr int kLabelVisibilityMarginPx = 12;

  const bool label_target_visible =
      target_px.x >= -kLabelVisibilityMarginPx &&
      target_px.x <= vp->pix_width + kLabelVisibilityMarginPx &&
      target_px.y >= -kLabelVisibilityMarginPx &&
      target_px.y <= vp->pix_height + kLabelVisibilityMarginPx;

  if (!label_target_visible)
    return;

  const wxString bearing_range = wxString::Format(_("%03.0f°T · %.2f NM"),
                                                   item.bearing_true, item.range_nm);
  wxString label = bearing_range;
  if (!preview && !item.target_name.IsEmpty()) label = item.target_name + _("\n") + bearing_range;
  const wxFont font = GetEblLabelFont();
  const int newline = label.Find('\n');

  const wxString first_line =
      newline == wxNOT_FOUND
          ? label
          : label.Left(static_cast<size_t>(newline));

  const wxString second_line =
      newline == wxNOT_FOUND
          ? wxString()
          : label.Mid(static_cast<size_t>(newline + 1));

  wxBitmap measure_bitmap(1, 1, 24);
  wxMemoryDC measure_dc(measure_bitmap);
  measure_dc.SetFont(font);

  const wxSize first_extent =
      measure_dc.GetTextExtent(first_line);

  const wxSize second_extent =
      second_line.IsEmpty()
          ? wxSize(0, 0)
          : measure_dc.GetTextExtent(second_line);

  measure_dc.SelectObject(wxNullBitmap);

  const int line_gap = second_line.IsEmpty() ? 0 : 2;
  const int text_width =
      std::max(first_extent.x, second_extent.x);
  const int text_height =
      first_extent.y + line_gap + second_extent.y;

  const int box_width = std::max(1, text_width + 8);
  const int box_height = std::max(1, text_height + 6);

  wxPoint box_pos =
      FindSafeLabelBox(vp, item, target_px, box_width, box_height);

  if (item.label_manual) {
    const int max_x =
        std::max(4, vp->pix_width - box_width - 4);
    const int max_y =
        std::max(4, vp->pix_height - box_height - 4);

    box_pos.x = std::clamp(
        target_px.x + item.label_dx_px, 4, max_x);
    box_pos.y = std::clamp(
        target_px.y + item.label_dy_px, 4, max_y);
  }

  const wxPoint leader_end =
      ClosestPointOnLabelBox(
          target_px, box_pos, box_width, box_height);

  const wxPoint leader_start =
      PointOnTargetRingTowards(target_px, leader_end, ring);

  glColor4ub(colour.Red(), colour.Green(), colour.Blue(),
             preview ? 190 : 255);

  glBegin(GL_LINES);
  glVertex2i(leader_start.x, leader_start.y);
  glVertex2i(leader_end.x, leader_end.y);
  glEnd();

  DrawGlLabel(label, box_pos.x, box_pos.y);
}

void route_toolbar_pi::DrawGlLabel(const wxString& label, int x, int y) const {
  if (label.IsEmpty()) return;
  const wxFont font = GetEblLabelFont();
  const int newline = label.Find('\n');
  const wxString first_line = newline == wxNOT_FOUND ? label : label.Left(static_cast<size_t>(newline));
  const wxString second_line = newline == wxNOT_FOUND ? wxString() : label.Mid(static_cast<size_t>(newline + 1));
  wxBitmap measure_bitmap(1, 1, 24);
  wxMemoryDC measure_dc(measure_bitmap);
  measure_dc.SetFont(font);
  const wxSize first_extent = measure_dc.GetTextExtent(first_line);
  const wxSize second_extent = second_line.IsEmpty() ? wxSize(0, 0) : measure_dc.GetTextExtent(second_line);
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
  if (!second_line.IsEmpty()) bitmap_dc.DrawText(second_line, 4, 3 + first_extent.y + line_gap);
  bitmap_dc.SelectObject(wxNullBitmap);
  wxImage image = bitmap.ConvertToImage();
  if (!image.IsOk() || !image.GetData()) return;
  GLint previous_texture = 0;
  GLint previous_unpack_alignment = 4;
  const GLboolean texture_was_enabled = glIsEnabled(GL_TEXTURE_2D);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
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
  glTexCoord2f(0.0f, 0.0f); glVertex2i(x, y);
  glTexCoord2f(1.0f, 0.0f); glVertex2i(x + width, y);
  glTexCoord2f(1.0f, 1.0f); glVertex2i(x + width, y + height);
  glTexCoord2f(0.0f, 1.0f); glVertex2i(x, y + height);
  glEnd();
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
  glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack_alignment);
  if (texture_was_enabled) glEnable(GL_TEXTURE_2D); else glDisable(GL_TEXTURE_2D);
  glDeleteTextures(1, &texture);
}

double route_toolbar_pi::NormalizeBearing(double bearing) {
  bearing = std::fmod(bearing, 360.0);
  if (bearing < 0.0) bearing += 360.0;
  return bearing;
}


void route_toolbar_pi::ShowObjectManager() {
  wxDialog dialog(
      m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
      wxID_ANY, _("Route Objects"), wxDefaultPosition, wxSize(860, 470),
      wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxSTAY_ON_TOP);

  auto* top = new wxBoxSizer(wxVERTICAL);
  auto* intro = new wxStaticText(
      &dialog, wxID_ANY,
      _("Route Toolbar objects. Hide/show changes only the chart overlay; "
        "the objects remain saved and are still included in GPX/report data."));
  intro->Wrap(820);
  top->Add(intro, 0, wxEXPAND | wxALL, 10);

  auto* list = new wxListCtrl(
      &dialog, wxID_ANY, wxDefaultPosition, wxDefaultSize,
      wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_SUNKEN);
  list->AppendColumn(_("Shown"), wxLIST_FORMAT_LEFT, 70);
  list->AppendColumn(_("Type"), wxLIST_FORMAT_LEFT, 90);
  list->AppendColumn(_("Route / start"), wxLIST_FORMAT_LEFT, 180);
  list->AppendColumn(_("Target / waypoint"), wxLIST_FORMAT_LEFT, 190);
  list->AppendColumn(_("Details"), wxLIST_FORMAT_LEFT, 270);
  top->Add(list, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

  auto populate = [&]() {
    list->DeleteAllItems();

    for (size_t i = 0; i < m_ebl_vrms.size(); ++i) {
      const RouteEblVrm& item = m_ebl_vrms[i];

      wxString start;
      if (item.start_is_turn) {
        start = item.start_turn_waypoint_name;
        if (start.IsEmpty()) start = _("WP");
        start += item.start_turn_end ? _(" / TURN END snap") : _(" / WO snap");
      } else if (item.start_is_free) {
        start = _("Free point");
      } else if (item.start_is_mark) {
        start = item.start_mark_name.IsEmpty() ? _("Mark snap") : item.start_mark_name;
      } else {
        start = item.waypoint_name.IsEmpty() ? _("Waypoint snap") : item.waypoint_name;
      }

      const long row =
          list->InsertItem(list->GetItemCount(), item.visible ? _("Yes") : _("No"));
      list->SetItem(row, 1, _("EBL/VRM"));
      list->SetItem(row, 2, start);
      list->SetItem(row, 3, item.target_name);
      list->SetItem(row, 4,
                    wxString::Format("%03.0f°T · %.2f NM",
                                     item.bearing_true, item.range_nm));
      list->SetItemData(row, 1000000L + static_cast<long>(i));
    }

    for (size_t i = 0; i < m_pils.size(); ++i) {
      const RoutePil& item = m_pils[i];
      const long row =
          list->InsertItem(list->GetItemCount(), item.visible ? _("Yes") : _("No"));
      list->SetItem(row, 1, _("PIL"));
      list->SetItem(row, 2, item.route_name);
      list->SetItem(row, 3, item.from_name + _(" → ") + item.to_name);
      list->SetItem(row, 4,
                    wxString::Format("%+.2f NM %s",
                                     item.offset_nm,
                                     item.offset_nm >= 0.0 ? _("Stbd") : _("Port")));
      list->SetItemData(row, 2000000L + static_cast<long>(i));
    }

    for (size_t i = 0; i < m_turns.size(); ++i) {
      const RouteTurn& item = m_turns[i];
      const long row =
          list->InsertItem(list->GetItemCount(), item.visible ? _("Yes") : _("No"));
      list->SetItem(row, 1, _("TURN"));
      list->SetItem(row, 2, item.route_name);
      list->SetItem(row, 3, item.waypoint_name);

      wxString details =
          wxString::Format("ΔK %.0f° %s · R %.2f NM",
                           std::fabs(item.turn_angle_deg),
                           item.turn_angle_deg >= 0.0 ? _("SB") : _("BB"),
                           item.radius_nm);
      if (item.rudder_angle_deg > 0.0)
        details += wxString::Format(" · Rudder %.0f° %s",
                                    item.rudder_angle_deg,
                                    item.turn_angle_deg >= 0.0 ? _("SB") : _("BB"));

      list->SetItem(row, 4, details);
      list->SetItemData(row, 3000000L + static_cast<long>(i));
    }
  };

  auto set_selected_visibility = [&](bool visible) {
    const long row =
        list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row < 0) return;

    const long code = list->GetItemData(row);
    const int type = static_cast<int>(code / 1000000L);
    const size_t index = static_cast<size_t>(code % 1000000L);

    if (type == 1 && index < m_ebl_vrms.size())
      m_ebl_vrms[index].visible = visible;
    else if (type == 2 && index < m_pils.size())
      m_pils[index].visible = visible;
    else if (type == 3 && index < m_turns.size())
      m_turns[index].visible = visible;
    else
      return;

    SavePersistentState();
    populate();
    if (m_parent_window) RequestRefresh(m_parent_window);
  };

  auto set_all_visibility = [&](bool visible) {
    for (RouteEblVrm& item : m_ebl_vrms) item.visible = visible;
    for (RoutePil& item : m_pils) item.visible = visible;
    for (RouteTurn& item : m_turns) item.visible = visible;

    SavePersistentState();
    populate();
    if (m_parent_window) RequestRefresh(m_parent_window);
  };

  auto* row1 = new wxBoxSizer(wxHORIZONTAL);
  auto* show_selected = new wxButton(&dialog, wxID_ANY, _("Show selected"));
  auto* hide_selected = new wxButton(&dialog, wxID_ANY, _("Hide selected"));
  auto* show_all = new wxButton(&dialog, wxID_ANY, _("Show all"));
  auto* hide_all = new wxButton(&dialog, wxID_ANY, _("Hide all"));

  row1->Add(show_selected, 0, wxRIGHT, 6);
  row1->Add(hide_selected, 0, wxRIGHT, 14);
  row1->Add(show_all, 0, wxRIGHT, 6);
  row1->Add(hide_all, 0);

  top->Add(row1, 0, wxLEFT | wxRIGHT | wxTOP, 10);

  auto* row2 = new wxBoxSizer(wxHORIZONTAL);
  auto* del = new wxButton(&dialog, wxID_DELETE, _("Delete selected"));
  auto* close = new wxButton(&dialog, wxID_CLOSE, _("Close"));
  row2->Add(del, 0);
  row2->AddStretchSpacer();
  row2->Add(close, 0);
  top->Add(row2, 0, wxEXPAND | wxALL, 10);

  show_selected->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
    set_selected_visibility(true);
  });
  hide_selected->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
    set_selected_visibility(false);
  });
  show_all->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
    set_all_visibility(true);
  });
  hide_all->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
    set_all_visibility(false);
  });

  list->Bind(wxEVT_LIST_ITEM_ACTIVATED, [&](wxListEvent&) {
    const long row =
        list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row < 0) return;

    const long code = list->GetItemData(row);
    const int type = static_cast<int>(code / 1000000L);
    const size_t index = static_cast<size_t>(code % 1000000L);

    bool current = true;
    if (type == 1 && index < m_ebl_vrms.size())
      current = m_ebl_vrms[index].visible;
    else if (type == 2 && index < m_pils.size())
      current = m_pils[index].visible;
    else if (type == 3 && index < m_turns.size())
      current = m_turns[index].visible;
    else
      return;

    set_selected_visibility(!current);
  });

  del->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
    const long row =
        list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row < 0) return;

    const long code = list->GetItemData(row);
    const int type = static_cast<int>(code / 1000000L);
    const size_t index = static_cast<size_t>(code % 1000000L);

    if (wxMessageBox(_("Delete selected object?"), _("Route Objects"),
                     wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
                     &dialog) != wxYES)
      return;

    if (type == 1 && index < m_ebl_vrms.size())
      m_ebl_vrms.erase(m_ebl_vrms.begin() + index);
    else if (type == 2 && index < m_pils.size())
      m_pils.erase(m_pils.begin() + index);
    else if (type == 3 && index < m_turns.size())
      m_turns.erase(m_turns.begin() + index);
    else
      return;

    SavePersistentState();
    populate();
    if (m_parent_window) RequestRefresh(m_parent_window);
  });

  close->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
    dialog.EndModal(wxID_OK);
  });

  populate();
  dialog.SetSizer(top);
  dialog.CentreOnParent();
  dialog.ShowModal();
}


void route_toolbar_pi::SetStatus(const wxString& text) {
  if (m_dialog) m_dialog->SetStatus(text);
}

void route_toolbar_pi::OnDialogHidden() {
  CancelInteractiveTool();
  m_dialog_visible = false;
}
