// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_dialog.h"

#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>

#include "route_toolbar_pi.h"

namespace {
enum : int {
  ID_ROUTE_EBL_VRM = wxID_HIGHEST + 410,
  ID_ROUTE_PIL,
  ID_ROUTE_REPORT,
  ID_LABEL_SIZE
};
}

RouteToolbarDialog::RouteToolbarDialog(wxWindow* parent, route_toolbar_pi* plugin)
    : wxDialog(parent, wxID_ANY, _("Route Toolbar"), wxDefaultPosition,
               wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_plugin(plugin) {
  auto* top = new wxBoxSizer(wxVERTICAL);

  auto* intro = new wxStaticText(
      this, wxID_ANY,
      _("Passage-planning tools linked to an OpenCPN route."));
  top->Add(intro, 0, wxALL, 10);
  top->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

  auto* ebl = new wxButton(this, ID_ROUTE_EBL_VRM, _("Route EBL / VRM"));
  auto* pil = new wxButton(this, ID_ROUTE_PIL, _("Route PIL"));
  auto* report = new wxButton(this, ID_ROUTE_REPORT, _("Route Report"));

  top->Add(ebl, 0, wxEXPAND | wxALL, 10);
  top->Add(pil, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
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

  m_status = new wxStaticText(
      this, wxID_ANY,
      _("v0.2: Route EBL/VRM is ready. Select the tool, then click a route waypoint."));
  m_status->Wrap(360);
  top->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
  top->Add(m_status, 0, wxEXPAND | wxALL, 10);

  SetSizerAndFit(top);
  SetMinSize(wxSize(390, GetSize().GetHeight()));

  Bind(wxEVT_BUTTON, &RouteToolbarDialog::OnEblVrm, this, ID_ROUTE_EBL_VRM);
  Bind(wxEVT_BUTTON, &RouteToolbarDialog::OnPil, this, ID_ROUTE_PIL);
  Bind(wxEVT_BUTTON, &RouteToolbarDialog::OnReport, this, ID_ROUTE_REPORT);
  Bind(wxEVT_SPINCTRL, &RouteToolbarDialog::OnLabelSize, this, ID_LABEL_SIZE);
  Bind(wxEVT_TEXT, &RouteToolbarDialog::OnLabelSizeText, this, ID_LABEL_SIZE);
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

void RouteToolbarDialog::OnEblVrm(wxCommandEvent&) {
  if (m_plugin) m_plugin->BeginEblVrm();
}

void RouteToolbarDialog::OnPil(wxCommandEvent&) {
  if (m_plugin) m_plugin->CancelInteractiveTool();
  SetStatus(_("Route PIL selected — route-leg selection is the next implementation step."));
}

void RouteToolbarDialog::OnReport(wxCommandEvent&) {
  if (m_plugin) m_plugin->CancelInteractiveTool();
  SetStatus(_("Route Report selected — CSV/HTML export follows after persistent route data."));
}

void RouteToolbarDialog::OnLabelSize(wxSpinEvent& event) {
  if (m_plugin) m_plugin->SetLabelFontSize(event.GetValue());
}

void RouteToolbarDialog::OnLabelSizeText(wxCommandEvent&) {
  if (m_plugin && m_label_size)
    m_plugin->SetLabelFontSize(m_label_size->GetValue());
}

void RouteToolbarDialog::OnClose(wxCloseEvent&) {
  if (m_plugin) m_plugin->CancelInteractiveTool();
  Hide();
  if (m_plugin) m_plugin->OnDialogHidden();
}
