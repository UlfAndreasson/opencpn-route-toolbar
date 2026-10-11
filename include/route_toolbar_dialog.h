// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <wx/dialog.h>

class wxStaticText;
class route_toolbar_pi;

class RouteToolbarDialog final : public wxDialog {
public:
  RouteToolbarDialog(wxWindow* parent, route_toolbar_pi* plugin);

private:
  void OnEblVrm(wxCommandEvent& event);
  void OnPil(wxCommandEvent& event);
  void OnReport(wxCommandEvent& event);
  void OnClose(wxCloseEvent& event);

  route_toolbar_pi* m_plugin{nullptr};
  wxStaticText* m_status{nullptr};
};
