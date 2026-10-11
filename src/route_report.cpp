// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_pi.h"
#include "route_toolbar_dialog.h"

#include <cmath>
#include <string>
#include <vector>

#include <wx/choicdlg.h>
#include <wx/datetime.h>
#include <wx/ffile.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/utils.h>

namespace {

wxString CsvField(wxString value) {
  value.Replace("\r\n", "\n");
  value.Replace("\r", "\n");
  const bool quote = value.Find(';') != wxNOT_FOUND ||
                     value.Find('"') != wxNOT_FOUND ||
                     value.Find('\n') != wxNOT_FOUND;
  value.Replace("\"", "\"\"");
  return quote ? "\"" + value + "\"" : value;
}

wxString CsvNumber(double value, int decimals) {
  if (!std::isfinite(value)) return wxEmptyString;
  wxString text = wxString::Format("%.*f", decimals, value);
  text.Replace(".", ",");
  return text;
}

wxString FormatCoordinate(double value, bool latitude) {
  const double absolute = std::fabs(value);
  const int degrees = static_cast<int>(std::floor(absolute));
  const double minutes = (absolute - degrees) * 60.0;
  const wxChar hemisphere = latitude ? (value >= 0.0 ? 'N' : 'S')
                                      : (value >= 0.0 ? 'E' : 'W');
  return latitude
             ? wxString::Format("%02d° %06.3f' %c", degrees, minutes,
                                hemisphere)
             : wxString::Format("%03d° %06.3f' %c", degrees, minutes,
                                hemisphere);
}

wxString SafeFilename(wxString name) {
  if (name.IsEmpty()) name = "route";
  static const wxString invalid = "\\/:*?\"<>|";
  for (wxUniChar ch : invalid) name.Replace(wxString(ch), "-");
  name.Trim(true).Trim(false);
  if (name.IsEmpty()) name = "route";
  return name;
}

struct ReportWaypoint {
  double lat{0.0};
  double lon{0.0};
  wxString guid;
  wxString name;
  double course_to_next{NAN};
  double distance_to_next{NAN};
};

}  // namespace

void route_toolbar_pi::ExportRouteReport() {
  CancelInteractiveTool();
  wxWindow* parent = m_dialog ? static_cast<wxWindow*>(m_dialog)
                              : m_parent_window;

  wxArrayString formats;
  formats.Add(_("PDF - A4 landscape"));
  formats.Add(_("CSV - spreadsheet/data"));
  wxSingleChoiceDialog dialog(
      parent, _("Choose report format."), _("Route Report"), formats);
  if (dialog.ShowModal() != wxID_OK) return;

  if (dialog.GetSelection() == 0)
    ExportRoutePdfReport();
  else
    ExportRouteCsvReport();
}

void route_toolbar_pi::ExportRouteCsvReport() {
  CancelInteractiveTool();

  const wxArrayString route_guids = GetRouteGUIDArray();
  if (route_guids.IsEmpty()) {
    wxMessageBox(_("There are no OpenCPN routes to export."),
                 _("Route Report"), wxOK | wxICON_INFORMATION,
                 m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window);
    SetStatus(_("Route Report: no routes found."));
    return;
  }

  wxArrayString choices;
  std::vector<size_t> choice_to_route_index;
  for (size_t i = 0; i < route_guids.GetCount(); ++i) {
    auto route = GetRoute_Plugin(route_guids[i]);
    if (!route || !route->pWaypointList) continue;

    wxString name = route->m_NameString;
    if (name.IsEmpty()) name = _("Unnamed route");

    size_t count = 0;
    for (auto node = route->pWaypointList->GetFirst(); node;
         node = node->GetNext())
      ++count;

    choices.Add(wxString::Format("%s  (%lu WP)", name,
                                 static_cast<unsigned long>(count)));
    choice_to_route_index.push_back(i);
  }

  if (choices.IsEmpty()) {
    SetStatus(_("Route Report: no readable routes found."));
    return;
  }

  wxSingleChoiceDialog route_dialog(
      m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
      _("Select the OpenCPN route to include in the report."),
      _("Route Report - CSV"), choices);
  if (route_dialog.ShowModal() != wxID_OK) {
    SetStatus(_("Route Report cancelled."));
    return;
  }

  const int selected = route_dialog.GetSelection();
  if (selected < 0 ||
      static_cast<size_t>(selected) >= choice_to_route_index.size())
    return;

  const size_t route_index = choice_to_route_index[static_cast<size_t>(selected)];
  const wxString route_guid = route_guids[route_index];
  auto route = GetRoute_Plugin(route_guid);
  if (!route || !route->pWaypointList) {
    SetStatus(_("Route Report: the selected route could not be read."));
    return;
  }

  wxString route_name = route->m_NameString;
  if (route_name.IsEmpty()) route_name = _("Unnamed route");

  std::vector<ReportWaypoint> waypoints;
  for (auto node = route->pWaypointList->GetFirst(); node;
       node = node->GetNext()) {
    PlugIn_Waypoint* waypoint = node->GetData();
    if (!waypoint) continue;

    ReportWaypoint row;
    row.lat = waypoint->m_lat;
    row.lon = waypoint->m_lon;
    row.guid = waypoint->m_GUID;
    row.name = waypoint->m_MarkName;
    waypoints.push_back(row);
  }

  if (waypoints.empty()) {
    wxMessageBox(_("The selected route contains no waypoints."),
                 _("Route Report"), wxOK | wxICON_INFORMATION,
                 m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window);
    return;
  }

  double total_distance_nm = 0.0;
  for (size_t i = 0; i + 1 < waypoints.size(); ++i) {
    double bearing = 0.0;
    double distance = 0.0;
    DistanceBearingMercator_Plugin(
        waypoints[i + 1].lat, waypoints[i + 1].lon,
        waypoints[i].lat, waypoints[i].lon, &bearing, &distance);
    waypoints[i].course_to_next = NormalizeBearing(bearing);
    waypoints[i].distance_to_next = distance;
    total_distance_nm += distance;
  }

  std::vector<const RoutePil*> route_pils;
  for (const RoutePil& pil : m_pils) {
    const bool same_route =
        pil.route_guid == route_guid ||
        (pil.route_guid.IsEmpty() && pil.route_name == route_name);
    if (same_route) route_pils.push_back(&pil);
  }

  std::vector<RouteEblVrm> mark_ebls;
  for (const RouteEblVrm& stored : m_ebl_vrms) {
    if (!stored.start_is_mark) continue;
    RouteEblVrm item = stored;
    RecalculateEbl(item);
    mark_ebls.push_back(item);
  }

  wxString csv;
  csv << CsvField(_("Route Report")) << ";" << CsvField(route_name)
      << "\r\n";
  csv << CsvField(_("Generated")) << ";"
      << CsvField(wxDateTime::Now().FormatISOCombined(' ')) << "\r\n";
  csv << CsvField(_("Route distance (NM)")) << ";"
      << CsvNumber(total_distance_nm, 2) << "\r\n\r\n";

  csv << CsvField(_("WP")) << ";"
      << CsvField(_("Waypoint")) << ";"
      << CsvField(_("Latitude")) << ";"
      << CsvField(_("Longitude")) << ";"
      << CsvField(_("Course to next (°T)")) << ";"
      << CsvField(_("Distance to next (NM)")) << ";"
      << CsvField(_("PIL side")) << ";"
      << CsvField(_("PIL offset (NM)")) << ";"
      << CsvField(_("Sight mark")) << ";"
      << CsvField(_("EBL bearing (°T)")) << ";"
      << CsvField(_("EBL range (NM)")) << ";"
      << CsvField(_("Sight mark latitude")) << ";"
      << CsvField(_("Sight mark longitude")) << "\r\n";

  size_t exported_route_ebls = 0;
  for (size_t i = 0; i < waypoints.size(); ++i) {
    const ReportWaypoint& wp = waypoints[i];
    std::vector<const RouteEblVrm*> matches;

    for (const RouteEblVrm& ebl : m_ebl_vrms) {
      if (ebl.start_is_mark) continue;
      const bool same_route =
          ebl.route_guid == route_guid ||
          (ebl.route_guid.IsEmpty() && ebl.route_name == route_name);
      if (!same_route) continue;

      bool same_waypoint = !wp.guid.IsEmpty() && ebl.waypoint_guid == wp.guid;
      if (!same_waypoint && ebl.waypoint_guid.IsEmpty()) {
        double start_distance = 0.0;
        DistanceBearingMercator_Plugin(
            wp.lat, wp.lon, ebl.start_lat, ebl.start_lon, nullptr,
            &start_distance);
        same_waypoint = start_distance <= 0.002;
      }
      if (same_waypoint) matches.push_back(&ebl);
    }

    wxString pil_side;
    wxString pil_offset;
    for (const RoutePil* pil : route_pils) {
      if (pil->leg_index != static_cast<int>(i + 1)) continue;
      if (!pil_side.IsEmpty()) {
        pil_side += " / ";
        pil_offset += " / ";
      }
      pil_side += pil->offset_nm >= 0.0 ? _("Stbd") : _("Port");
      pil_offset += CsvNumber(std::fabs(pil->offset_nm), 2);
    }

    const auto append_base_columns = [&]() {
      csv << (i + 1) << ";"
          << CsvField(wp.name) << ";"
          << CsvField(FormatCoordinate(wp.lat, true)) << ";"
          << CsvField(FormatCoordinate(wp.lon, false)) << ";";

      if (std::isfinite(wp.course_to_next))
        csv << wxString::Format("%03.0f", wp.course_to_next);
      csv << ";";
      if (std::isfinite(wp.distance_to_next))
        csv << CsvNumber(wp.distance_to_next, 2);
      csv << ";"
          << CsvField(pil_side) << ";"
          << CsvField(pil_offset) << ";";
    };

    if (matches.empty()) {
      append_base_columns();
      csv << ";;;;;\r\n";
      continue;
    }

    for (const RouteEblVrm* ebl : matches) {
      append_base_columns();
      csv << CsvField(ebl->target_name) << ";"
          << wxString::Format("%03.0f", ebl->bearing_true) << ";"
          << CsvNumber(ebl->range_nm, 2) << ";"
          << CsvField(FormatCoordinate(ebl->target_lat, true)) << ";"
          << CsvField(FormatCoordinate(ebl->target_lon, false)) << "\r\n";
      ++exported_route_ebls;
    }
  }

  if (!mark_ebls.empty()) {
    csv << "\r\n" << CsvField(_("EBL/VRM from Marks")) << "\r\n";
    csv << CsvField(_("Start mark")) << ";"
        << CsvField(_("Start latitude")) << ";"
        << CsvField(_("Start longitude")) << ";"
        << CsvField(_("Sight mark")) << ";"
        << CsvField(_("EBL bearing (°T)")) << ";"
        << CsvField(_("EBL range (NM)")) << ";"
        << CsvField(_("Sight mark latitude")) << ";"
        << CsvField(_("Sight mark longitude")) << "\r\n";

    for (const RouteEblVrm& ebl : mark_ebls) {
      wxString start_name = ebl.start_mark_name;
      if (start_name.IsEmpty()) start_name = _("Mark");
      csv << CsvField(start_name) << ";"
          << CsvField(FormatCoordinate(ebl.start_lat, true)) << ";"
          << CsvField(FormatCoordinate(ebl.start_lon, false)) << ";"
          << CsvField(ebl.target_name) << ";"
          << wxString::Format("%03.0f", ebl.bearing_true) << ";"
          << CsvNumber(ebl.range_nm, 2) << ";"
          << CsvField(FormatCoordinate(ebl.target_lat, true)) << ";"
          << CsvField(FormatCoordinate(ebl.target_lon, false)) << "\r\n";
    }
  }

  const size_t total_ebls = exported_route_ebls + mark_ebls.size();
  csv << "\r\n";
  csv << CsvField(_("Summary")) << ";"
      << CsvField(wxString::Format(_("%lu waypoints"),
                                   static_cast<unsigned long>(waypoints.size())))
      << ";"
      << CsvField(wxString::Format(_("%lu EBL/VRM sight marks"),
                                   static_cast<unsigned long>(total_ebls)))
      << ";"
      << CsvField(wxString::Format(_("%lu PIL"),
                                   static_cast<unsigned long>(route_pils.size())))
      << ";"
      << CsvField(_("Total distance")) << ";"
      << CsvNumber(total_distance_nm, 2) << " NM\r\n";

  const wxString default_name = SafeFilename(route_name) + "-route-report.csv";
  wxFileDialog save_dialog(
      m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
      _("Save Route Report CSV"), wxEmptyString, default_name,
      _("CSV files (*.csv)|*.csv"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (save_dialog.ShowModal() != wxID_OK) {
    SetStatus(_("Route Report cancelled."));
    return;
  }

  wxFFile output(save_dialog.GetPath(), "wb");
  if (!output.IsOpened()) {
    wxMessageBox(_("Could not create the report file."), _("Route Report"),
                 wxOK | wxICON_ERROR,
                 m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window);
    return;
  }

  static const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
  output.Write(bom, sizeof(bom));
  const std::string utf8 = csv.ToStdString(wxConvUTF8);
  output.Write(utf8.data(), utf8.size());
  output.Close();

  SetStatus(wxString::Format(
      _("CSV Route Report saved: %lu WP, %lu EBL/VRM, %lu PIL, %.2f NM."),
      static_cast<unsigned long>(waypoints.size()),
      static_cast<unsigned long>(total_ebls),
      static_cast<unsigned long>(route_pils.size()), total_distance_nm));

  wxMessageDialog open_dialog(
      m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
      _("The CSV Route Report has been saved. Open it now?"),
      _("Route Report"), wxYES_NO | wxYES_DEFAULT | wxICON_INFORMATION);
  if (open_dialog.ShowModal() == wxID_YES)
    wxLaunchDefaultApplication(save_dialog.GetPath());
}
