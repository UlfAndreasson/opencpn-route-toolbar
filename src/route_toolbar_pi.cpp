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
constexpr int kPluginVersionPatch = 9;
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
  if (!wxFileName::DirExists(dir)) wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
  const wxString path = dir + wxFILE_SEP_PATH + "route-toolbar-v041.svg";
  static const char* svg = R"SVG(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32">
  <rect x="0.75" y="0.75" width="30.5" height="30.5" rx="3"
        fill="#2b2d2f" stroke="#cfd3d6" stroke-width="1.5"/>

  <path d="M7.2 22.7 L15.7 15.8 L23.6 11.3"
        fill="none"
        stroke="#989fa5"
        stroke-width="1.35"/>

  <circle cx="7.2" cy="22.7" r="2.35"
          fill="none"
          stroke="#989fa5"
          stroke-width="1.35"/>

  <circle cx="15.7" cy="15.8" r="2.35"
          fill="none"
          stroke="#989fa5"
          stroke-width="1.35"/>

  <circle cx="23.6" cy="11.3" r="2.35"
          fill="none"
          stroke="#989fa5"
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

int route_toolbar_pi::ShowActionButtons(const wxString& title,
                                        const wxString& prompt,
                                        const wxArrayString& labels) {
  return ShowRouteToolbarActionButtons(
      m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
      title, prompt, labels);
}

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
  m_dialog_visible = true;
  m_dialog->Show();
  m_dialog->Raise();
  m_dialog->Refresh();
  m_dialog->Update();
}

void route_toolbar_pi::HideToolWindow() {
  if (!m_dialog) {
    m_dialog_visible = false;
    return;
  }
  m_dialog->Hide();
  m_dialog_visible = false;
}

void route_toolbar_pi::SetCursorLatLon(double lat, double lon) {
  m_cursor_lat = lat;
  m_cursor_lon = lon;
}

void route_toolbar_pi::SetCurrentViewPort(PlugIn_ViewPort& vp) {
  m_current_viewport = vp;
}

void route_toolbar_pi::OnToolbarToolDownCallback(int) {}

void route_toolbar_pi::OnToolbarToolUpCallback(int) {}

void route_toolbar_pi::OnContextMenuItemCallback(int id) {
  if (!m_context_menu_ready) return;

  if (id == m_context_ebl_id) {
    StartEblVrmMode();
  } else if (id == m_context_pil_id) {
    StartPilMode();
  } else if (id == m_context_turn_id) {
    StartTurnMode();
  }
}

void route_toolbar_pi::SetCursorLatLon(int, double lat, double lon) {
  SetCursorLatLon(lat, lon);
}

void route_toolbar_pi::OnCursorLatLon(double lat, double lon) {
  SetCursorLatLon(lat, lon);
}

void route_toolbar_pi::OnToolbarToolCallback(int id, int) {
  OnToolbarToolCallback(id);
}

void route_toolbar_pi::OnToolbarToolDownCallback(int id, int) {
  OnToolbarToolDownCallback(id);
}

void route_toolbar_pi::OnToolbarToolUpCallback(int id, int) {
  OnToolbarToolUpCallback(id);
}

void route_toolbar_pi::ShowPreferencesDialog(wxWindow*) {}

void route_toolbar_pi::SetDefaults() {}

void route_toolbar_pi::SetNMEASentence(wxString&) {}

void route_toolbar_pi::SetPositionFix(PlugIn_Position_Fix&) {}

void route_toolbar_pi::SetPositionFixEx(PlugIn_Position_Fix_Ex&) {}

void route_toolbar_pi::SetPluginMessage(wxString&, wxString&) {}

void route_toolbar_pi::OnAISMessage(wxString&) {}

void route_toolbar_pi::OnN2KMessage(const std::vector<unsigned char>&) {}

void route_toolbar_pi::OnAisTargetList(Plugin_AIS_Targets&) {}

void route_toolbar_pi::OnToolbarToolCallback(int id, wxString&) {
  OnToolbarToolCallback(id);
}

void route_toolbar_pi::SetCurrentViewPort(PlugIn_ViewPort* vp) {
  if (vp) SetCurrentViewPort(*vp);
}

void route_toolbar_pi::SetCursorLatLon(PlugIn_Position_Fix& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::SetCursorLatLon(PlugIn_Position_Fix_Ex& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::SetCursorLatLon(const PlugIn_Position_Fix& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::SetCursorLatLon(const PlugIn_Position_Fix_Ex& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::SetCurrentViewPort(const PlugIn_ViewPort& vp) {
  m_current_viewport = vp;
}

void route_toolbar_pi::OnToolbarToolDownCallback(int id, wxMouseEvent&) {
  OnToolbarToolDownCallback(id);
}

void route_toolbar_pi::OnToolbarToolUpCallback(int id, wxMouseEvent&) {
  OnToolbarToolUpCallback(id);
}

void route_toolbar_pi::OnContextMenuItemCallback(int id, wxString&) {
  OnContextMenuItemCallback(id);
}

void route_toolbar_pi::OnContextMenuItemCallback(int id, wxMouseEvent&) {
  OnContextMenuItemCallback(id);
}

void route_toolbar_pi::OnCursorLatLon(int, double lat, double lon) {
  OnCursorLatLon(lat, lon);
}

void route_toolbar_pi::OnCurrentViewPort(PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(const PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(PlugIn_ViewPort* vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::OnCursorLatLon(const PlugIn_Position_Fix& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::OnCursorLatLon(const PlugIn_Position_Fix_Ex& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::OnToolbarToolDownCallback(int id, wxString&) {
  OnToolbarToolDownCallback(id);
}

void route_toolbar_pi::OnToolbarToolUpCallback(int id, wxString&) {
  OnToolbarToolUpCallback(id);
}

void route_toolbar_pi::OnContextMenuItemCallback(int id, int) {
  OnContextMenuItemCallback(id);
}

void route_toolbar_pi::OnToolbarToolCallback(int id, wxMouseEvent&) {
  OnToolbarToolCallback(id);
}

void route_toolbar_pi::OnToolbarToolDownCallback(int id, PlugIn_Position_Fix&) {
  OnToolbarToolDownCallback(id);
}

void route_toolbar_pi::OnToolbarToolUpCallback(int id, PlugIn_Position_Fix&) {
  OnToolbarToolUpCallback(id);
}

void route_toolbar_pi::OnContextMenuItemCallback(int id, PlugIn_Position_Fix&) {
  OnContextMenuItemCallback(id);
}

void route_toolbar_pi::OnCursorLatLon(const wxPoint&) {}

void route_toolbar_pi::OnToolbarToolCallback(int id, const wxPoint&) {
  OnToolbarToolCallback(id);
}

void route_toolbar_pi::OnToolbarToolDownCallback(int id, const wxPoint&) {
  OnToolbarToolDownCallback(id);
}

void route_toolbar_pi::OnToolbarToolUpCallback(int id, const wxPoint&) {
  OnToolbarToolUpCallback(id);
}

void route_toolbar_pi::OnContextMenuItemCallback(int id, const wxPoint&) {
  OnContextMenuItemCallback(id);
}

void route_toolbar_pi::SetCursorLatLon(const wxPoint&) {}

void route_toolbar_pi::SetCurrentViewPort(int) {}

void route_toolbar_pi::SetCurrentViewPort(const wxString&) {}

void route_toolbar_pi::SetCursorLatLon(const wxString&) {}

void route_toolbar_pi::OnCurrentViewPort(int) {}

void route_toolbar_pi::OnCurrentViewPort(const wxString&) {}

void route_toolbar_pi::OnCursorLatLon(const wxString&) {}

void route_toolbar_pi::OnContextMenuItemCallback(int id, const wxString&) {
  OnContextMenuItemCallback(id);
}

void route_toolbar_pi::OnToolbarToolCallback(int id, const wxString&) {
  OnToolbarToolCallback(id);
}

void route_toolbar_pi::OnToolbarToolDownCallback(int id, const wxString&) {
  OnToolbarToolDownCallback(id);
}

void route_toolbar_pi::OnToolbarToolUpCallback(int id, const wxString&) {
  OnToolbarToolUpCallback(id);
}

void route_toolbar_pi::SetCursorLatLon(const wxString&, double lat, double lon) {
  SetCursorLatLon(lat, lon);
}

void route_toolbar_pi::SetCurrentViewPort(const wxString&, PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCurrentViewPort(const wxString&, const PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCurrentViewPort(const wxString&, PlugIn_ViewPort* vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCursorLatLon(const wxString&, const PlugIn_Position_Fix& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::SetCursorLatLon(const wxString&, const PlugIn_Position_Fix_Ex& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::OnCursorLatLon(const wxString&, double lat, double lon) {
  OnCursorLatLon(lat, lon);
}

void route_toolbar_pi::OnCurrentViewPort(const wxString&, PlugIn_ViewPort& vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(const wxString&, const PlugIn_ViewPort& vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(const wxString&, PlugIn_ViewPort* vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCursorLatLon(const wxString&, const PlugIn_Position_Fix& pfix) {
  OnCursorLatLon(pfix);
}

void route_toolbar_pi::OnCursorLatLon(const wxString&, const PlugIn_Position_Fix_Ex& pfix) {
  OnCursorLatLon(pfix);
}

void route_toolbar_pi::OnToolbarToolCallback(const wxString&, int id) {
  OnToolbarToolCallback(id);
}

void route_toolbar_pi::OnToolbarToolDownCallback(const wxString&, int id) {
  OnToolbarToolDownCallback(id);
}

void route_toolbar_pi::OnToolbarToolUpCallback(const wxString&, int id) {
  OnToolbarToolUpCallback(id);
}

void route_toolbar_pi::OnContextMenuItemCallback(const wxString&, int id) {
  OnContextMenuItemCallback(id);
}

void route_toolbar_pi::SetCursorLatLon(const wxString&, int, double lat, double lon) {
  SetCursorLatLon(lat, lon);
}

void route_toolbar_pi::SetCurrentViewPort(const wxString&, int, PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCurrentViewPort(const wxString&, int, const PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCurrentViewPort(const wxString&, int, PlugIn_ViewPort* vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCursorLatLon(const wxString&, int, const PlugIn_Position_Fix& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::SetCursorLatLon(const wxString&, int, const PlugIn_Position_Fix_Ex& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::OnCursorLatLon(const wxString&, int, double lat, double lon) {
  OnCursorLatLon(lat, lon);
}

void route_toolbar_pi::OnCurrentViewPort(const wxString&, int, PlugIn_ViewPort& vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(const wxString&, int, const PlugIn_ViewPort& vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(const wxString&, int, PlugIn_ViewPort* vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCursorLatLon(const wxString&, int, const PlugIn_Position_Fix& pfix) {
  OnCursorLatLon(pfix);
}

void route_toolbar_pi::OnCursorLatLon(const wxString&, int, const PlugIn_Position_Fix_Ex& pfix) {
  OnCursorLatLon(pfix);
}

void route_toolbar_pi::OnToolbarToolCallback(int, int, int) {}

void route_toolbar_pi::OnToolbarToolDownCallback(int, int, int) {}

void route_toolbar_pi::OnToolbarToolUpCallback(int, int, int) {}

void route_toolbar_pi::OnContextMenuItemCallback(int, int, int) {}

void route_toolbar_pi::SetCursorLatLon(int, int, double lat, double lon) {
  SetCursorLatLon(lat, lon);
}

void route_toolbar_pi::SetCurrentViewPort(int, int, PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCurrentViewPort(int, int, const PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCurrentViewPort(int, int, PlugIn_ViewPort* vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCursorLatLon(int, int, const PlugIn_Position_Fix& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::SetCursorLatLon(int, int, const PlugIn_Position_Fix_Ex& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::OnCursorLatLon(int, int, double lat, double lon) {
  OnCursorLatLon(lat, lon);
}

void route_toolbar_pi::OnCurrentViewPort(int, int, PlugIn_ViewPort& vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(int, int, const PlugIn_ViewPort& vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(int, int, PlugIn_ViewPort* vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCursorLatLon(int, int, const PlugIn_Position_Fix& pfix) {
  OnCursorLatLon(pfix);
}

void route_toolbar_pi::OnCursorLatLon(int, int, const PlugIn_Position_Fix_Ex& pfix) {
  OnCursorLatLon(pfix);
}

void route_toolbar_pi::OnToolbarToolCallback(int, const wxString&, int) {}

void route_toolbar_pi::OnToolbarToolDownCallback(int, const wxString&, int) {}

void route_toolbar_pi::OnToolbarToolUpCallback(int, const wxString&, int) {}

void route_toolbar_pi::OnContextMenuItemCallback(int, const wxString&, int) {}

void route_toolbar_pi::SetCursorLatLon(int, const wxString&, double lat, double lon) {
  SetCursorLatLon(lat, lon);
}

void route_toolbar_pi::SetCurrentViewPort(int, const wxString&, PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCurrentViewPort(int, const wxString&, const PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCurrentViewPort(int, const wxString&, PlugIn_ViewPort* vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCursorLatLon(int, const wxString&, const PlugIn_Position_Fix& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::SetCursorLatLon(int, const wxString&, const PlugIn_Position_Fix_Ex& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::OnCursorLatLon(int, const wxString&, double lat, double lon) {
  OnCursorLatLon(lat, lon);
}

void route_toolbar_pi::OnCurrentViewPort(int, const wxString&, PlugIn_ViewPort& vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(int, const wxString&, const PlugIn_ViewPort& vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(int, const wxString&, PlugIn_ViewPort* vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCursorLatLon(int, const wxString&, const PlugIn_Position_Fix& pfix) {
  OnCursorLatLon(pfix);
}

void route_toolbar_pi::OnCursorLatLon(int, const wxString&, const PlugIn_Position_Fix_Ex& pfix) {
  OnCursorLatLon(pfix);
}

void route_toolbar_pi::OnToolbarToolCallback(const wxString&, const wxString&, int) {}

void route_toolbar_pi::OnToolbarToolDownCallback(const wxString&, const wxString&, int) {}

void route_toolbar_pi::OnToolbarToolUpCallback(const wxString&, const wxString&, int) {}

void route_toolbar_pi::OnContextMenuItemCallback(const wxString&, const wxString&, int) {}

void route_toolbar_pi::SetCursorLatLon(const wxString&, const wxString&, double lat, double lon) {
  SetCursorLatLon(lat, lon);
}

void route_toolbar_pi::SetCurrentViewPort(const wxString&, const wxString&, PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCurrentViewPort(const wxString&, const wxString&, const PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCurrentViewPort(const wxString&, const wxString&, PlugIn_ViewPort* vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCursorLatLon(const wxString&, const wxString&, const PlugIn_Position_Fix& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::SetCursorLatLon(const wxString&, const wxString&, const PlugIn_Position_Fix_Ex& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::OnCursorLatLon(const wxString&, const wxString&, double lat, double lon) {
  OnCursorLatLon(lat, lon);
}

void route_toolbar_pi::OnCurrentViewPort(const wxString&, const wxString&, PlugIn_ViewPort& vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(const wxString&, const wxString&, const PlugIn_ViewPort& vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(const wxString&, const wxString&, PlugIn_ViewPort* vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCursorLatLon(const wxString&, const wxString&, const PlugIn_Position_Fix& pfix) {
  OnCursorLatLon(pfix);
}

void route_toolbar_pi::OnCursorLatLon(const wxString&, const wxString&, const PlugIn_Position_Fix_Ex& pfix) {
  OnCursorLatLon(pfix);
}

void route_toolbar_pi::OnToolbarToolCallback(const wxString&, int, int) {}

void route_toolbar_pi::OnToolbarToolDownCallback(const wxString&, int, int) {}

void route_toolbar_pi::OnToolbarToolUpCallback(const wxString&, int, int) {}

void route_toolbar_pi::OnContextMenuItemCallback(const wxString&, int, int) {}

void route_toolbar_pi::SetCursorLatLon(const wxString&, int, int, double lat, double lon) {
  SetCursorLatLon(lat, lon);
}

void route_toolbar_pi::SetCurrentViewPort(const wxString&, int, int, PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCurrentViewPort(const wxString&, int, int, const PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCurrentViewPort(const wxString&, int, int, PlugIn_ViewPort* vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCursorLatLon(const wxString&, int, int, const PlugIn_Position_Fix& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::SetCursorLatLon(const wxString&, int, int, const PlugIn_Position_Fix_Ex& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::OnCursorLatLon(const wxString&, int, int, double lat, double lon) {
  OnCursorLatLon(lat, lon);
}

void route_toolbar_pi::OnCurrentViewPort(const wxString&, int, int, PlugIn_ViewPort& vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(const wxString&, int, int, const PlugIn_ViewPort& vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(const wxString&, int, int, PlugIn_ViewPort* vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCursorLatLon(const wxString&, int, int, const PlugIn_Position_Fix& pfix) {
  OnCursorLatLon(pfix);
}

void route_toolbar_pi::OnCursorLatLon(const wxString&, int, int, const PlugIn_Position_Fix_Ex& pfix) {
  OnCursorLatLon(pfix);
}

void route_toolbar_pi::OnToolbarToolCallback(int, const wxString&, const wxString&) {}

void route_toolbar_pi::OnToolbarToolDownCallback(int, const wxString&, const wxString&) {}

void route_toolbar_pi::OnToolbarToolUpCallback(int, const wxString&, const wxString&) {}

void route_toolbar_pi::OnContextMenuItemCallback(int, const wxString&, const wxString&) {}

void route_toolbar_pi::SetCursorLatLon(int, const wxString&, const wxString&, double lat, double lon) {
  SetCursorLatLon(lat, lon);
}

void route_toolbar_pi::SetCurrentViewPort(int, const wxString&, const wxString&, PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCurrentViewPort(int, const wxString&, const wxString&, const PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCurrentViewPort(int, const wxString&, const wxString&, PlugIn_ViewPort* vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCursorLatLon(int, const wxString&, const wxString&, const PlugIn_Position_Fix& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::SetCursorLatLon(int, const wxString&, const wxString&, const PlugIn_Position_Fix_Ex& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::OnCursorLatLon(int, const wxString&, const wxString&, double lat, double lon) {
  OnCursorLatLon(lat, lon);
}

void route_toolbar_pi::OnCurrentViewPort(int, const wxString&, const wxString&, PlugIn_ViewPort& vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(int, const wxString&, const wxString&, const PlugIn_ViewPort& vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(int, const wxString&, const wxString&, PlugIn_ViewPort* vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCursorLatLon(int, const wxString&, const wxString&, const PlugIn_Position_Fix& pfix) {
  OnCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::OnCursorLatLon(int, const wxString&, const wxString&, const PlugIn_Position_Fix_Ex& pfix) {
  OnCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::OnToolbarToolCallback(const wxString&, const wxString&, const wxString&) {}

void route_toolbar_pi::OnToolbarToolDownCallback(const wxString&, const wxString&, const wxString&) {}

void route_toolbar_pi::OnToolbarToolUpCallback(const wxString&, const wxString&, const wxString&) {}

void route_toolbar_pi::OnContextMenuItemCallback(const wxString&, const wxString&, const wxString&) {}

void route_toolbar_pi::SetCursorLatLon(const wxString&, const wxString&, const wxString&, double lat, double lon) {
  SetCursorLatLon(lat, lon);
}

void route_toolbar_pi::SetCurrentViewPort(const wxString&, const wxString&, const wxString&, PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCurrentViewPort(const wxString&, const wxString&, const wxString&, const PlugIn_ViewPort& vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCurrentViewPort(const wxString&, const wxString&, const wxString&, PlugIn_ViewPort* vp) {
  SetCurrentViewPort(vp);
}

void route_toolbar_pi::SetCursorLatLon(const wxString&, const wxString&, const wxString&, const PlugIn_Position_Fix& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::SetCursorLatLon(const wxString&, const wxString&, const wxString&, const PlugIn_Position_Fix_Ex& pfix) {
  SetCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::OnCursorLatLon(const wxString&, const wxString&, const wxString&, double lat, double lon) {
  OnCursorLatLon(lat, lon);
}

void route_toolbar_pi::OnCurrentViewPort(const wxString&, const wxString&, const wxString&, PlugIn_ViewPort& vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(const wxString&, const wxString&, const wxString&, const PlugIn_ViewPort& vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCurrentViewPort(const wxString&, const wxString&, const wxString&, PlugIn_ViewPort* vp) {
  OnCurrentViewPort(vp);
}

void route_toolbar_pi::OnCursorLatLon(const wxString&, const wxString&, const wxString&, const PlugIn_Position_Fix& pfix) {
  OnCursorLatLon(pfix.Lat, pfix.Lon);
}

void route_toolbar_pi::OnCursorLatLon(const wxString&, const wxString&, const wxString&, const PlugIn_Position_Fix_Ex& pfix) {
  OnCursorLatLon(pfix.Lat, pfix.Lon);
}

// remaining implementation is split across route_*.cpp files
