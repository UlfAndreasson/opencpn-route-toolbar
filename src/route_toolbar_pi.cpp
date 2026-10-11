// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_pi.h"

#include <wx/intl.h>

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
constexpr int kPluginVersionMinor = 1;
constexpr int kPluginVersionPatch = 0;

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

  return WANTS_TOOLBAR_CALLBACK | INSTALLS_TOOLBAR_TOOL;
}

bool route_toolbar_pi::DeInit() {
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
  if (m_dialog) m_dialog->Hide();
  m_dialog_visible = false;
  SetToolbarItemState(m_toolbar_id, false);
}

void route_toolbar_pi::OnDialogHidden() {
  m_dialog_visible = false;
  if (m_toolbar_id >= 0) SetToolbarItemState(m_toolbar_id, false);
}
