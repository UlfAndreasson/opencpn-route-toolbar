// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <wx/dialog.h>

class wxSpinCtrl;
class wxStaticText;
class wxToggleButton;
class route_toolbar_pi;

class RouteToolbarDialog final : public wxDialog {
public:
  RouteToolbarDialog(wxWindow* parent, route_toolbar_pi* plugin);
  void SetStatus(const wxString& text);
  void SetToolButtons(bool ebl_active, bool pil_active);

private:
  void OnEblVrm(wxCommandEvent& event);
  void OnPil(wxCommandEvent& event);
  void OnReport(wxCommandEvent& event);
  void OnLabelSize(wxSpinEvent& event);
  void OnLabelSizeText(wxCommandEvent& event);
  void OnClose(wxCloseEvent& event);

  route_toolbar_pi* m_plugin{nullptr};
  wxToggleButton* m_ebl_button{nullptr};
  wxToggleButton* m_pil_button{nullptr};
  wxSpinCtrl* m_label_size{nullptr};
  wxStaticText* m_status{nullptr};
};
