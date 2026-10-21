// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_pi.h"
#include "route_toolbar_dialog.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <wx/choicdlg.h>
#include <wx/datetime.h>
#include <wx/ffile.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/utils.h>

namespace {

constexpr double kReportWaypointToleranceNm = 0.03;
constexpr double kReportLegToleranceNm = 0.05;

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

wxString TurnSummary(double inbound, double outbound, double angle,
                     double radius, double tangent, double rudder) {
  wxString text = wxString::Format("%03.0f→%03.0f · Δ%.0f° %s · R %.2f · WO %.3f",
                                   inbound, outbound, std::fabs(angle),
                                   angle >= 0.0 ? "SB" : "BB", radius, tangent);
  if (rudder > 0.0)
    text += wxString::Format(" · Rudder %.0f° %s", rudder,
                             angle >= 0.0 ? "SB" : "BB");
  return text;
}

struct ReportWaypoint {
  double lat{0.0};
  double lon{0.0};
  wxString guid;
  wxString name;
  double course_to_next{NAN};
  double distance_to_next{NAN};
  std::vector<wxString> turns;
  std::vector<wxString> pils;
};

}  // namespace

void route_toolbar_pi::ExportRouteReport() {
  CancelInteractiveTool();

  wxArrayString actions;
  actions.Add(_("Export PDF - A4 landscape"));
  actions.Add(_("Export CSV - spreadsheet/data"));

  const int selected = ShowActionButtons(
      _("Route Report"), _("What do you want to create?"), actions);
  if (selected < 0) return;

  if (selected == 0)
    ExportRoutePdfReport();
  else
    ExportRouteCsvReport();
}

void route_toolbar_pi::ExportRouteCsvReport() {
  CancelInteractiveTool();

  const wxArrayString route_guids = GetRouteGUIDArray();
  if (route_guids.IsEmpty()) {
    wxMessageBox(_("There are no OpenCPN routes to export."), _("Route Report"),
                 wxOK | wxICON_INFORMATION,
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
    for (auto node = route->pWaypointList->GetFirst(); node; node = node->GetNext())
      ++count;
    choices.Add(wxString::Format("%s  (%lu WP)", name,
                                 static_cast<unsigned long>(count)));
    choice_to_route_index.push_back(i);
  }

  if (choices.IsEmpty()) {
    SetStatus(_("Route Report: no readable routes found."));
    return;
  }

  const int selected = ShowActionButtons(
      _("Route Report - CSV"),
      _("Select the OpenCPN route to include in the report."), choices);
  if (selected < 0) {
    SetStatus(_("Route Report cancelled."));
    return;
  }
  if (static_cast<size_t>(selected) >= choice_to_route_index.size()) return;

  const wxString route_guid =
      route_guids[choice_to_route_index[static_cast<size_t>(selected)]];
  auto route = GetRoute_Plugin(route_guid);
  if (!route || !route->pWaypointList) {
    SetStatus(_("Route Report: the selected route could not be read."));
    return;
  }

  wxString route_name = route->m_NameString;
  if (route_name.IsEmpty()) route_name = _("Unnamed route");

  std::vector<ReportWaypoint> waypoints;
  for (auto node = route->pWaypointList->GetFirst(); node; node = node->GetNext()) {
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
    wxMessageBox(_("The selected route contains no waypoints."), _("Route Report"),
                 wxOK | wxICON_INFORMATION,
                 m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window);
    return;
  }

  double total_distance_nm = 0.0;
  for (size_t i = 0; i + 1 < waypoints.size(); ++i) {
    double bearing = 0.0;
    double distance = 0.0;
    DistanceBearingMercator_Plugin(waypoints[i + 1].lat, waypoints[i + 1].lon,
                                   waypoints[i].lat, waypoints[i].lon,
                                   &bearing, &distance);
    waypoints[i].course_to_next = NormalizeBearing(bearing);
    waypoints[i].distance_to_next = distance;
    total_distance_nm += distance;
  }

  auto row_for_guid = [&](const wxString& guid) -> int {
    if (guid.IsEmpty()) return -1;
    for (size_t i = 0; i < waypoints.size(); ++i)
      if (waypoints[i].guid == guid) return static_cast<int>(i);
    return -1;
  };

  std::vector<const RouteTurn*> route_turns;
  for (const RouteTurn& turn : m_turns) {
    const bool same_route = turn.route_guid == route_guid ||
        (turn.route_guid.IsEmpty() && turn.route_name == route_name);
    if (!same_route) continue;
    route_turns.push_back(&turn);
    const int row = row_for_guid(turn.waypoint_guid);
    if (row >= 0) {
      waypoints[static_cast<size_t>(row)].turns.push_back(
          TurnSummary(turn.inbound_course_true, turn.outbound_course_true,
                      turn.turn_angle_deg, turn.radius_nm, turn.tangent_nm,
                      turn.rudder_angle_deg));
    }
  }

  std::vector<const RoutePil*> route_pils;
  for (const RoutePil& pil : m_pils) {
    const bool same_route = pil.route_guid == route_guid ||
        (pil.route_guid.IsEmpty() && pil.route_name == route_name);
    if (!same_route) continue;
    route_pils.push_back(&pil);
    if (pil.leg_index > 0 && static_cast<size_t>(pil.leg_index) <= waypoints.size()) {
      waypoints[static_cast<size_t>(pil.leg_index - 1)].pils.push_back(
          wxString::Format("%s: %s NM", pil.offset_nm >= 0.0 ? "SB" : "BB",
                           CsvNumber(std::fabs(pil.offset_nm), 2)));
    }
  }

  std::vector<std::vector<const RouteEblVrm*>> route_ebls(waypoints.size());
  size_t exported_route_ebls = 0;

  auto ebl_row = [&](const RouteEblVrm& ebl) -> int {
    if (ebl.start_is_turn) {
      const int row = row_for_guid(ebl.start_turn_waypoint_guid);
      if (row >= 0) return row;
    }
    {
      const int row = row_for_guid(ebl.waypoint_guid);
      if (row >= 0) return row;
    }

    // Free-point EBL/VRM objects created on a turn are associated with the
    // turn waypoint when they are on/near the actual turn geometry. This
    // covers tangent-in, the arc, tangent-out and a small safety margin.
    for (const RouteTurn* turn : route_turns) {
      const double tol = std::clamp(turn->radius_nm * 0.25, 0.02, 0.05);
      double ds = 0.0, de = 0.0, dc = 0.0, dw = 0.0;
      DistanceBearingMercator_Plugin(turn->start_lat, turn->start_lon,
                                     ebl.start_lat, ebl.start_lon, nullptr, &ds);
      DistanceBearingMercator_Plugin(turn->end_lat, turn->end_lon,
                                     ebl.start_lat, ebl.start_lon, nullptr, &de);
      DistanceBearingMercator_Plugin(turn->center_lat, turn->center_lon,
                                     ebl.start_lat, ebl.start_lon, nullptr, &dc);
      DistanceBearingMercator_Plugin(turn->waypoint_lat, turn->waypoint_lon,
                                     ebl.start_lat, ebl.start_lon, nullptr, &dw);
      const bool in_turn_zone = ds <= tol || de <= tol ||
          std::fabs(dc - turn->radius_nm) <= tol ||
          dw <= turn->tangent_nm + tol;
      if (in_turn_zone) {
        const int row = row_for_guid(turn->waypoint_guid);
        if (row >= 0) return row;
      }
    }

    const RouteWaypointHit wp_hit =
        FindNearestRouteWaypoint(ebl.start_lat, ebl.start_lon,
                                 kReportWaypointToleranceNm);
    if (wp_hit.found && wp_hit.route_guid == route_guid) {
      const int row = row_for_guid(wp_hit.waypoint_guid);
      if (row >= 0) return row;
    }

    const RouteLegHit leg_hit =
        FindNearestRouteLeg(ebl.start_lat, ebl.start_lon, kReportLegToleranceNm);
    if (leg_hit.found && leg_hit.route_guid == route_guid && leg_hit.leg_index > 0 &&
        static_cast<size_t>(leg_hit.leg_index) <= waypoints.size())
      return leg_hit.leg_index - 1;

    return -1;
  };

  std::vector<RouteEblVrm> mark_ebls;
  for (const RouteEblVrm& stored : m_ebl_vrms) {
    if (stored.start_is_mark) {
      RouteEblVrm item = stored;
      RecalculateEbl(item);
      mark_ebls.push_back(item);
      continue;
    }
    const bool same_route = stored.route_guid == route_guid ||
        (stored.route_guid.IsEmpty() && stored.route_name == route_name);
    if (!same_route) continue;
    const int row = ebl_row(stored);
    if (row >= 0) {
      route_ebls[static_cast<size_t>(row)].push_back(&stored);
      ++exported_route_ebls;
    }
  }

  wxString csv;
  csv << CsvField(_("Route Report")) << ";" << CsvField(route_name) << "\r\n";
  csv << CsvField(_("Generated")) << ";"
      << CsvField(wxDateTime::Now().FormatISOCombined(' ')) << "\r\n";
  csv << CsvField(_("Route distance (NM)")) << ";"
      << CsvNumber(total_distance_nm, 2) << "\r\n\r\n";

  csv << CsvField(_("WP")) << ";" << CsvField(_("Waypoint")) << ";"
      << CsvField(_("Latitude")) << ";" << CsvField(_("Longitude")) << ";"
      << CsvField(_("Course to next (°T)")) << ";"
      << CsvField(_("Distance to next (NM)")) << ";"
      << CsvField(_("TURN")) << ";" << CsvField(_("PIL")) << ";"
      << CsvField(_("Sight mark")) << ";" << CsvField(_("EBL bearing (°T)")) << ";"
      << CsvField(_("EBL range (NM)")) << ";"
      << CsvField(_("Sight mark latitude")) << ";"
      << CsvField(_("Sight mark longitude")) << "\r\n";

  for (size_t i = 0; i < waypoints.size(); ++i) {
    const ReportWaypoint& wp = waypoints[i];
    const auto& sights = route_ebls[i];
    const size_t detail_rows = std::max<size_t>(
        1, std::max(sights.size(), std::max(wp.turns.size(), wp.pils.size())));

    for (size_t r = 0; r < detail_rows; ++r) {
      csv << (i + 1) << ";" << CsvField(wp.name) << ";"
          << CsvField(FormatCoordinate(wp.lat, true)) << ";"
          << CsvField(FormatCoordinate(wp.lon, false)) << ";";
      if (std::isfinite(wp.course_to_next))
        csv << wxString::Format("%03.0f", wp.course_to_next);
      csv << ";";
      if (std::isfinite(wp.distance_to_next))
        csv << CsvNumber(wp.distance_to_next, 2);
      csv << ";";
      if (r < wp.turns.size()) csv << CsvField(wp.turns[r]);
      csv << ";";
      if (r < wp.pils.size()) csv << CsvField(wp.pils[r]);
      csv << ";";

      if (r < sights.size()) {
        const RouteEblVrm* ebl = sights[r];
        csv << CsvField(ebl->target_name) << ";"
            << wxString::Format("%03.0f", ebl->bearing_true) << ";"
            << CsvNumber(ebl->range_nm, 2) << ";"
            << CsvField(FormatCoordinate(ebl->target_lat, true)) << ";"
            << CsvField(FormatCoordinate(ebl->target_lon, false)) << "\r\n";
      } else {
        csv << ";;;;\r\n";
      }
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
  csv << "\r\n" << CsvField(_("Summary")) << ";"
      << CsvField(wxString::Format(_("%lu waypoints"),
                                   static_cast<unsigned long>(waypoints.size()))) << ";"
      << CsvField(wxString::Format(_("%lu TURN"),
                                   static_cast<unsigned long>(route_turns.size()))) << ";"
      << CsvField(wxString::Format(_("%lu EBL/VRM sight marks"),
                                   static_cast<unsigned long>(total_ebls))) << ";"
      << CsvField(wxString::Format(_("%lu PIL"),
                                   static_cast<unsigned long>(route_pils.size()))) << ";"
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
      _("CSV Route Report saved: %lu WP, %lu TURN, %lu EBL/VRM, %lu PIL, %.2f NM."),
      static_cast<unsigned long>(waypoints.size()),
      static_cast<unsigned long>(route_turns.size()),
      static_cast<unsigned long>(total_ebls),
      static_cast<unsigned long>(route_pils.size()), total_distance_nm));

  wxMessageDialog open_dialog(
      m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
      _("The CSV Route Report has been saved. Open it now?"),
      _("Route Report"), wxYES_NO | wxYES_DEFAULT | wxICON_INFORMATION);
  if (open_dialog.ShowModal() == wxID_YES)
    wxLaunchDefaultApplication(save_dialog.GetPath());
}
