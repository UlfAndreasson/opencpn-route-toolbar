// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_pi.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <wx/brush.h>
#include <wx/dc.h>
#include <wx/dcmemory.h>
#include <wx/image.h>
#include <wx/intl.h>
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
constexpr int kPluginVersionPatch = 2;
constexpr double kWaypointPickRadiusNm = 0.10;
constexpr double kDefaultArcWidthDeg = 30.0;
constexpr int kArcSegments = 30;

static const char* route_toolbar_xpm[] = {
    "16 16 3 1",
    "  c None",
    "M c #C000E8",
    "W c #FFFFFF",
    "                ",
    "   W            ",
    "  WWW           ",
    "   W            ",
    "   MMMMM        ",
    "       M        ",
    "       M   W    ",
    "      MMM WWW   ",
    "       M   W    ",
    "       MMMMM    ",
    "           M    ",
    "           M    ",
    "          MMM   ",
    "           M    ",
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

  m_toolbar_id = InsertPlugInTool(
      _("Route Toolbar"), &m_toolbar_bitmap, &m_toolbar_bitmap, wxITEM_CHECK,
      _("Route Toolbar"), _("Route planning tools: EBL/VRM, PIL and report"),
      nullptr, -1, 0, this);

  return WANTS_TOOLBAR_CALLBACK | INSTALLS_TOOLBAR_TOOL |
         WANTS_CURSOR_LATLON | WANTS_MOUSE_EVENTS | WANTS_OVERLAY_CALLBACK |
         WANTS_OPENGL_OVERLAY_CALLBACK;
}

bool route_toolbar_pi::DeInit() {
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

void route_toolbar_pi::BeginEblVrm() {
  if (!m_dialog_visible) ShowToolWindow();
  m_pending_start = RouteWaypointHit{};
  m_ebl_mode = EblMode::AwaitRoutePoint;
  SetStatus(_("Route EBL/VRM: click a route waypoint. The click snaps to the nearest route waypoint within 0.10 NM. Right-click on the chart to cancel."));
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::CancelInteractiveTool() {
  m_ebl_mode = EblMode::Idle;
  m_pending_start = RouteWaypointHit{};
  if (m_parent_window) RequestRefresh(m_parent_window);
}

void route_toolbar_pi::SetCursorLatLon(double lat, double lon) {
  m_cursor_lat = lat;
  m_cursor_lon = lon;
  m_has_cursor = true;

  if (m_ebl_mode == EblMode::AwaitTarget && m_parent_window)
    RequestRefresh(m_parent_window);
}

bool route_toolbar_pi::MouseEventHook(wxMouseEvent& event) {
  if (m_ebl_mode == EblMode::Idle) return false;

  if (event.RightDown()) {
    CancelInteractiveTool();
    SetStatus(_("Route EBL/VRM stopped. Existing EBL/VRM objects remain visible."));
    return true;
  }

  if (!event.LeftDown() || !m_has_cursor) return false;

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

  // OpenCPN's legacy DistanceBearingMercator API names its first coordinate
  // pair lat1/lon1 and its second pair lat0/lon0. To obtain the bearing from
  // the route point to the sight mark, pass target first and route point last.
  DistanceBearingMercator_Plugin(
      item.target_lat, item.target_lon, item.start_lat, item.start_lon,
      &item.bearing_true, &item.range_nm);
  item.bearing_true = NormalizeBearing(item.bearing_true);

  if (item.range_nm < 0.001) {
    SetStatus(_("Target is too close to the route point. Click the sight mark again."));
    return;
  }

  const wxString default_name = wxString::Format(
      _("Sight mark %lu"), static_cast<unsigned long>(m_ebl_vrms.size() + 1));
  item.target_name = default_name;

  // Freeze the selected geometry before opening the modal naming dialog. This
  // prevents the live preview from following the mouse to the dialog buttons.
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

  wxString waypoint_name = item.waypoint_name;
  if (waypoint_name.IsEmpty()) waypoint_name = _("Waypoint");
  SetStatus(waypoint_name + _(" → ") + item.target_name + _("  ") +
            wxString::Format(_("%03.0f°T  %.2f NM. Created with a 30° VRM arc. Click another route waypoint, or right-click to finish."),
                             item.bearing_true, item.range_nm));

  m_pending_start = RouteWaypointHit{};
  m_ebl_mode = EblMode::AwaitRoutePoint;
  if (m_parent_window) RequestRefresh(m_parent_window);
}

bool route_toolbar_pi::RenderOverlay(wxDC& dc, PlugIn_ViewPort* vp) {
  if (!vp || !vp->bValid) return false;

  bool rendered = false;
  for (const auto& item : m_ebl_vrms) {
    DrawEblVrm(dc, vp, item, false);
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

    DistanceBearingMercator_Plugin(
        preview.target_lat, preview.target_lon, preview.start_lat,
        preview.start_lon, &preview.bearing_true, &preview.range_nm);
    preview.bearing_true = NormalizeBearing(preview.bearing_true);

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

  for (const auto& item : m_ebl_vrms) {
    DrawEblVrmGL(vp, item, false);
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

    DistanceBearingMercator_Plugin(
        preview.target_lat, preview.target_lon, preview.start_lat,
        preview.start_lon, &preview.bearing_true, &preview.range_nm);
    preview.bearing_true = NormalizeBearing(preview.bearing_true);

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
  dc.DrawCircle(target_px, preview ? 3 : 4);

  wxString label;
  if (!preview && !item.target_name.IsEmpty()) label = item.target_name + _("  ");
  label += wxString::Format(_("%03.0f°T  %.2f NM"), item.bearing_true,
                            item.range_nm);

  const wxSize text_size = dc.GetTextExtent(label);
  const wxPoint text_pos(target_px.x + 8, target_px.y - text_size.y - 8);
  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.SetBrush(wxBrush(wxColour(255, 245, 150)));
  dc.DrawRectangle(text_pos.x - 3, text_pos.y - 2, text_size.x + 6,
                   text_size.y + 4);
  dc.SetTextForeground(*wxBLACK);
  dc.DrawText(label, text_pos);

  dc.SetPen(old_pen);
  dc.SetBrush(old_brush);
  dc.SetTextForeground(old_text);
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

  glPointSize(preview ? 6.0f : 8.0f);
  glBegin(GL_POINTS);
  glVertex2i(start_px.x, start_px.y);
  glVertex2i(target_px.x, target_px.y);
  glEnd();

  wxString label;
  if (!preview && !item.target_name.IsEmpty()) label = item.target_name + _("  ");
  label += wxString::Format(_("%03.0f°T  %.2f NM"), item.bearing_true,
                            item.range_nm);
  DrawGlLabel(label, target_px.x + 8, target_px.y - 28);
}

void route_toolbar_pi::DrawGlLabel(const wxString& label, int x, int y) const {
  if (label.IsEmpty()) return;

  const wxFont font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
  wxBitmap measure_bitmap(1, 1, 24);
  wxMemoryDC measure_dc(measure_bitmap);
  measure_dc.SetFont(font);
  const wxSize extent = measure_dc.GetTextExtent(label);
  measure_dc.SelectObject(wxNullBitmap);

  const int width = std::max(1, extent.x + 8);
  const int height = std::max(1, extent.y + 6);
  wxBitmap bitmap(width, height, 24);
  wxMemoryDC bitmap_dc(bitmap);
  bitmap_dc.SetBackground(wxBrush(wxColour(255, 245, 150)));
  bitmap_dc.Clear();
  bitmap_dc.SetFont(font);
  bitmap_dc.SetTextForeground(*wxBLACK);
  bitmap_dc.DrawText(label, 4, 3);
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
