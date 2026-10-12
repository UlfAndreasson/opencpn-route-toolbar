// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <wx/dialog.h>

class wxButton;
class wxColourPickerCtrl;
class wxColourPickerEvent;
class wxSpinCtrl;
class wxStaticText;
class wxToggleButton;
class route_toolbar_pi;

class RouteToolbarDialog final : public wxDialog {
public:
  RouteToolbarDialog(wxWindow* parent, route_toolbar_pi* plugin);
  void SetStatus(const wxString& text);
  void SetToolButtons(bool ebl_active, bool pil_active, bool turn_active);

private:
  void OnEblVrm(wxCommandEvent& event);
  void OnPil(wxCommandEvent& event);
  void OnTurn(wxCommandEvent& event);
  void OnObjects(wxCommandEvent& event);
  void OnReport(wxCommandEvent& event);
  void OnGpx(wxCommandEvent& event);
  void OnLabelSize(wxSpinEvent& event);
  void OnLabelSizeText(wxCommandEvent& event);
  void OnEblColour(wxColourPickerEvent& event);
  void OnPilColour(wxColourPickerEvent& event);
  void OnTurnColour(wxColourPickerEvent& event);
  void OnNotesVisibility(wxCommandEvent& event);
  void OnClose(wxCloseEvent& event);

  route_toolbar_pi* m_plugin{nullptr};
  wxToggleButton* m_ebl_button{nullptr};
  wxToggleButton* m_pil_button{nullptr};
  wxToggleButton* m_turn_button{nullptr};
  wxSpinCtrl* m_label_size{nullptr};
  wxColourPickerCtrl* m_ebl_colour{nullptr};
  wxColourPickerCtrl* m_pil_colour{nullptr};
  wxColourPickerCtrl* m_turn_colour{nullptr};
  wxButton* m_notes_button{nullptr};
  wxStaticText* m_status{nullptr};
};
