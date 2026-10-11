// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_dialog.h"

#include <wx/button.h>
#include <wx/clrpicker.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/tglbtn.h>

#include "route_toolbar_pi.h"

namespace {
enum : int {
  ID_ROUTE_EBL_VRM = wxID_HIGHEST + 410,
  ID_ROUTE_PIL,
  ID_ROUTE_REPORT,
  ID_LABEL_SIZE,
  ID_EBL_COLOUR,
  ID_PIL_COLOUR
};
}

RouteToolbarDialog::RouteToolbarDialog(wxWindow* parent, route_toolbar_pi* plugin)
    : wxDialog(parent, wxID_ANY, _("Route Toolbar"), wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_plugin(plugin) {
  if (m_plugin) m_plugin->EnsureContextMenu();

  auto* top = new wxBoxSizer(wxVERTICAL);

  auto* intro = new wxStaticText(
      this, wxID_ANY,
      _("Passage-planning tools linked to an OpenCPN route."));
  top->Add(intro, 0, wxALL, 10);
  top->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

  m_ebl_button =
      new wxToggleButton(this, ID_ROUTE_EBL_VRM, _("Route EBL / VRM"));
  m_pil_button = new wxToggleButton(this, ID_ROUTE_PIL, _("Route PIL"));
  auto* report = new wxButton(this, ID_ROUTE_REPORT, _("Route Report"));

  top->Add(m_ebl_button, 0, wxEXPAND | wxALL, 10);
  top->Add(m_pil_button, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
  top->Add(report, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

  auto* label_row = new wxBoxSizer(wxHORIZONTAL);
  label_row->Add(new wxStaticText(this, wxID_ANY, _("EBL label size:")),
                 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  m_label_size = new wxSpinCtrl(this, ID_LABEL_SIZE, wxEmptyString,
                                wxDefaultPosition, wxSize(75, -1),
                                wxSP_ARROW_KEYS, 8, 30,
                                m_plugin ? m_plugin->GetLabelFontSize() : 14);
  label_row->Add(m_label_size, 0, wxALIGN_CENTER_VERTICAL);
  label_row->Add(new wxStaticText(this, wxID_ANY, _("pt")),
                 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
  top->Add(label_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

  auto* colour_row = new wxBoxSizer(wxHORIZONTAL);
  colour_row->Add(new wxStaticText(this, wxID_ANY, _("EBL/VRM:")),
                  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
  m_ebl_colour = new wxColourPickerCtrl(
      this, ID_EBL_COLOUR,
      m_plugin ? m_plugin->GetEblColour() : wxColour(210, 0, 220),
      wxDefaultPosition, wxSize(55, -1));
  colour_row->Add(m_ebl_colour, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 18);

  colour_row->Add(new wxStaticText(this, wxID_ANY, _("PIL:")),
                  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
  m_pil_colour = new wxColourPickerCtrl(
      this, ID_PIL_COLOUR,
      m_plugin ? m_plugin->GetPilColour() : wxColour(0, 105, 125),
      wxDefaultPosition, wxSize(55, -1));
  colour_row->Add(m_pil_colour, 0, wxALIGN_CENTER_VERTICAL);
  top->Add(colour_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

  m_status = new wxStaticText(
      this, wxID_ANY,
      _("Route EBL/VRM, Route PIL and Route Report are ready."));
  m_status->Wrap(360);
  top->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
  top->Add(m_status, 0, wxEXPAND | wxALL, 10);

  SetSizerAndFit(top);
  SetMinSize(wxSize(390, GetSize().GetHeight()));

  Bind(wxEVT_TOGGLEBUTTON, &RouteToolbarDialog::OnEblVrm, this,
       ID_ROUTE_EBL_VRM);
  Bind(wxEVT_TOGGLEBUTTON, &RouteToolbarDialog::OnPil, this, ID_ROUTE_PIL);
  Bind(wxEVT_BUTTON, &RouteToolbarDialog::OnReport, this, ID_ROUTE_REPORT);
  Bind(wxEVT_SPINCTRL, &RouteToolbarDialog::OnLabelSize, this, ID_LABEL_SIZE);
  Bind(wxEVT_TEXT, &RouteToolbarDialog::OnLabelSizeText, this, ID_LABEL_SIZE);
  Bind(wxEVT_COLOURPICKER_CHANGED, &RouteToolbarDialog::OnEblColour, this,
       ID_EBL_COLOUR);
  Bind(wxEVT_COLOURPICKER_CHANGED, &RouteToolbarDialog::OnPilColour, this,
       ID_PIL_COLOUR);
  Bind(wxEVT_CLOSE_WINDOW, &RouteToolbarDialog::OnClose, this);
}

void RouteToolbarDialog::SetStatus(const wxString& text) {
  if (!m_status) return;
  m_status->SetLabel(text);
  m_status->Wrap(360);
  GetSizer()->Fit(this);
  SetMinSize(wxSize(390, GetSize().GetHeight()));
  Layout();
}

void RouteToolbarDialog::SetToolButtons(bool ebl_active, bool pil_active) {
  if (m_ebl_button) m_ebl_button->SetValue(ebl_active);
  if (m_pil_button) m_pil_button->SetValue(pil_active);
}

void RouteToolbarDialog::OnEblVrm(wxCommandEvent& event) {
  if (!m_plugin) return;
  if (event.IsChecked()) {
    SetToolButtons(true, false);
    m_plugin->BeginEblVrm();
  } else {
    SetToolButtons(false, false);
    m_plugin->CancelInteractiveTool();
  }
}

void RouteToolbarDialog::OnPil(wxCommandEvent& event) {
  if (!m_plugin) return;
  if (event.IsChecked()) {
    SetToolButtons(false, true);
    m_plugin->BeginPil();
  } else {
    SetToolButtons(false, false);
    m_plugin->CancelInteractiveTool();
  }
}

void RouteToolbarDialog::OnReport(wxCommandEvent&) {
  if (!m_plugin) return;
  SetToolButtons(false, false);
  m_plugin->CancelInteractiveTool();
  m_plugin->ExportRouteReport();
}

void RouteToolbarDialog::OnLabelSize(wxSpinEvent& event) {
  if (m_plugin) m_plugin->SetLabelFontSize(event.GetValue());
}

void RouteToolbarDialog::OnLabelSizeText(wxCommandEvent&) {
  if (m_plugin && m_label_size)
    m_plugin->SetLabelFontSize(m_label_size->GetValue());
}

void RouteToolbarDialog::OnEblColour(wxColourPickerEvent& event) {
  if (m_plugin) m_plugin->SetEblColour(event.GetColour());
}

void RouteToolbarDialog::OnPilColour(wxColourPickerEvent& event) {
  if (m_plugin) m_plugin->SetPilColour(event.GetColour());
}

void RouteToolbarDialog::OnClose(wxCloseEvent&) {
  if (m_plugin) m_plugin->CancelInteractiveTool();
  Hide();
  if (m_plugin) m_plugin->OnDialogHidden();
}
