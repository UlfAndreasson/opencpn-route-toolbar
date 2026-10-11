// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_pi.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <wx/choicdlg.h>
#include <wx/datetime.h>
#include <wx/ffile.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/utils.h>

namespace {
constexpr double kPageWidth = 842.0;   // A4 landscape, points
constexpr double kPageHeight = 595.0;
constexpr double kMargin = 24.0;
constexpr double kBottom = 560.0;
constexpr double kRowHeight = 18.0;
constexpr double kHeaderHeight = 22.0;

struct PdfSight {
  wxString name;
  double bearing{0.0};
  double range_nm{0.0};
  double lat{0.0};
  double lon{0.0};
};

struct PdfWaypoint {
  double lat{0.0};
  double lon{0.0};
  wxString guid;
  wxString name;
  double course_to_next{NAN};
  double distance_to_next{NAN};
  std::vector<PdfSight> sights;
};

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

std::string PdfEscape(const wxString& text) {
  std::string out;
  out.reserve(text.length() + 8);
  for (wxUniChar ch : text) {
    unsigned long value = ch.GetValue();
    char c = value <= 255 ? static_cast<char>(value) : '?';
    if (c == '\\' || c == '(' || c == ')') out.push_back('\\');
    out.push_back(c);
  }
  return out;
}

wxString FitText(wxString text, double width, double font_size) {
  const int max_chars = std::max(1, static_cast<int>((width - 5.0) /
                                                      (font_size * 0.52)));
  if (static_cast<int>(text.length()) <= max_chars) return text;
  if (max_chars <= 3) return text.Left(static_cast<size_t>(max_chars));
  return text.Left(static_cast<size_t>(max_chars - 3)) + "...";
}

struct PdfCanvas {
  std::ostringstream stream;

  static double Y(double top) { return kPageHeight - top; }

  void Line(double x1, double y1, double x2, double y2,
            double gray = 0.55, double width = 0.5) {
    stream << std::fixed << std::setprecision(2) << gray << " G " << width
           << " w " << x1 << ' ' << Y(y1) << " m " << x2 << ' ' << Y(y2)
           << " l S\n";
  }

  void Rect(double x, double y, double w, double h, double fill_gray = -1.0,
            double stroke_gray = 0.65) {
    if (fill_gray >= 0.0) {
      stream << std::fixed << std::setprecision(2) << fill_gray << " g " << x
             << ' ' << (kPageHeight - y - h) << ' ' << w << ' ' << h
             << " re f\n";
    }
    stream << std::fixed << std::setprecision(2) << stroke_gray << " G 0.45 w "
           << x << ' ' << (kPageHeight - y - h) << ' ' << w << ' ' << h
           << " re S\n";
  }

  void Text(double x, double y, const wxString& text, double size = 7.0,
            bool bold = false) {
    if (text.IsEmpty()) return;
    stream << "0 g BT /" << (bold ? "F2" : "F1") << ' ' << std::fixed
           << std::setprecision(1) << size << " Tf 1 0 0 1 "
           << std::setprecision(2) << x << ' ' << (kPageHeight - y - size)
           << " Tm (" << PdfEscape(text) << ") Tj ET\n";
  }
};

class SimplePdf {
public:
  void AddPage(const std::string& content) { pages_.push_back(content); }

  bool Save(const wxString& path) const {
    if (pages_.empty()) return false;

    const int catalog_obj = 1;
    const int pages_obj = 2;
    const int font_obj = 3;
    const int bold_font_obj = 4;
    const int first_content_obj = 5;
    const int object_count = 4 + static_cast<int>(pages_.size()) * 2;

    std::vector<std::string> objects(static_cast<size_t>(object_count + 1));
    objects[catalog_obj] = "<< /Type /Catalog /Pages 2 0 R >>";
    objects[font_obj] =
        "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";
    objects[bold_font_obj] =
        "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>";

    std::ostringstream kids;
    kids << "[";
    for (size_t i = 0; i < pages_.size(); ++i) {
      const int content_obj = first_content_obj + static_cast<int>(i) * 2;
      const int page_obj = content_obj + 1;
      kids << page_obj << " 0 R ";

      std::ostringstream content;
      content << "<< /Length " << pages_[i].size() << " >>\nstream\n"
              << pages_[i] << "\nendstream";
      objects[content_obj] = content.str();

      std::ostringstream page;
      page << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 "
           << static_cast<int>(kPageWidth) << ' ' << static_cast<int>(kPageHeight)
           << "] /Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> "
           << "/Contents " << content_obj << " 0 R >>";
      objects[page_obj] = page.str();
    }
    kids << "]";
    objects[pages_obj] = "<< /Type /Pages /Kids " + kids.str() + " /Count " +
                         std::to_string(pages_.size()) + " >>";

    std::ostringstream out;
    out << "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
    std::vector<long long> offsets(static_cast<size_t>(object_count + 1), 0);
    for (int i = 1; i <= object_count; ++i) {
      offsets[static_cast<size_t>(i)] = static_cast<long long>(out.tellp());
      out << i << " 0 obj\n" << objects[static_cast<size_t>(i)]
          << "\nendobj\n";
    }
    const long long xref = static_cast<long long>(out.tellp());
    out << "xref\n0 " << (object_count + 1) << "\n";
    out << "0000000000 65535 f \n";
    for (int i = 1; i <= object_count; ++i) {
      out << std::setw(10) << std::setfill('0')
          << offsets[static_cast<size_t>(i)] << " 00000 n \n";
    }
    out << "trailer\n<< /Size " << (object_count + 1)
        << " /Root 1 0 R >>\nstartxref\n" << xref << "\n%%EOF\n";

    const std::string bytes = out.str();
    wxFFile file(path, "wb");
    if (!file.IsOpened()) return false;
    file.Write(bytes.data(), bytes.size());
    file.Close();
    return true;
  }

private:
  std::vector<std::string> pages_;
};

const std::vector<double> kWidths = {28, 60, 82, 86, 52, 52,
                                     94, 52, 52, 116, 120};
const std::vector<wxString> kHeaders = {
    "WP", "Waypoint", "Latitude", "Longitude", "Course °T", "Dist NM",
    "Sight mark", "EBL °T", "Range NM", "Sight lat", "Sight lon"};

void DrawTableHeader(PdfCanvas& page, double y) {
  double x = kMargin;
  for (size_t i = 0; i < kWidths.size(); ++i) {
    page.Rect(x, y, kWidths[i], kHeaderHeight, 0.90, 0.55);
    page.Text(x + 3, y + 6, FitText(kHeaders[i], kWidths[i], 7.0), 7.0, true);
    x += kWidths[i];
  }
}

void DrawPageTitle(PdfCanvas& page, const wxString& route_name,
                   double total_nm, bool continued) {
  page.Text(kMargin, 22,
            continued ? "Route Report - " + route_name + " (continued)"
                      : "Route Report - " + route_name,
            14.0, true);
  if (!continued) {
    page.Text(kMargin, 43,
              "Generated: " + wxDateTime::Now().FormatISOCombined(' '), 8.0);
    page.Text(kMargin + 260, 43,
              wxString::Format("Route distance: %.2f NM", total_nm), 8.0, true);
  }
  DrawTableHeader(page, continued ? 52.0 : 64.0);
}

}  // namespace

void route_toolbar_pi::ExportRoutePdfReport() {
  CancelInteractiveTool();

  const wxArrayString route_guids = GetRouteGUIDArray();
  if (route_guids.IsEmpty()) {
    wxMessageBox(_("There are no OpenCPN routes to export."), _("Route Report"),
                 wxOK | wxICON_INFORMATION,
                 m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window);
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

  wxSingleChoiceDialog route_dialog(
      m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
      _("Select the OpenCPN route to include in the PDF report."),
      _("Route Report - PDF"), choices);
  if (route_dialog.ShowModal() != wxID_OK) return;

  const int selection = route_dialog.GetSelection();
  if (selection < 0 ||
      static_cast<size_t>(selection) >= choice_to_route_index.size())
    return;

  const wxString route_guid =
      route_guids[choice_to_route_index[static_cast<size_t>(selection)]];
  auto route = GetRoute_Plugin(route_guid);
  if (!route || !route->pWaypointList) return;
  wxString route_name = route->m_NameString;
  if (route_name.IsEmpty()) route_name = _("Unnamed route");

  std::vector<PdfWaypoint> waypoints;
  for (auto node = route->pWaypointList->GetFirst(); node;
       node = node->GetNext()) {
    PlugIn_Waypoint* wp = node->GetData();
    if (!wp) continue;
    PdfWaypoint row;
    row.lat = wp->m_lat;
    row.lon = wp->m_lon;
    row.guid = wp->m_GUID;
    row.name = wp->m_MarkName;
    waypoints.push_back(row);
  }
  if (waypoints.empty()) return;

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

  for (PdfWaypoint& wp : waypoints) {
    for (const RouteEblVrm& ebl : m_ebl_vrms) {
      const bool same_route =
          ebl.route_guid == route_guid ||
          (ebl.route_guid.IsEmpty() && ebl.route_name == route_name);
      if (!same_route) continue;

      bool same_wp = !wp.guid.IsEmpty() && ebl.waypoint_guid == wp.guid;
      if (!same_wp && ebl.waypoint_guid.IsEmpty()) {
        double d = 0.0;
        DistanceBearingMercator_Plugin(wp.lat, wp.lon, ebl.start_lat,
                                       ebl.start_lon, nullptr, &d);
        same_wp = d <= 0.002;
      }
      if (!same_wp) continue;

      PdfSight sight;
      sight.name = ebl.target_name;
      sight.bearing = ebl.bearing_true;
      sight.range_nm = ebl.range_nm;
      sight.lat = ebl.target_lat;
      sight.lon = ebl.target_lon;
      wp.sights.push_back(sight);
    }
  }

  SimplePdf pdf;
  PdfCanvas page;
  bool first_page = true;
  DrawPageTitle(page, route_name, total_distance_nm, false);
  double y = 64.0 + kHeaderHeight;

  const auto finish_page = [&]() mutable {
    pdf.AddPage(page.stream.str());
    page = PdfCanvas{};
  };

  for (size_t i = 0; i < waypoints.size(); ++i) {
    const PdfWaypoint& wp = waypoints[i];
    const size_t subrows = std::max<size_t>(1, wp.sights.size());
    const double group_height = subrows * kRowHeight;

    if (y + group_height > kBottom) {
      finish_page();
      first_page = false;
      DrawPageTitle(page, route_name, total_distance_nm, true);
      y = 52.0 + kHeaderHeight;
    }

    const double fill = (i % 2 == 0) ? 0.985 : 0.955;
    double x = kMargin;
    for (size_t c = 0; c < 6; ++c) {
      page.Rect(x, y, kWidths[c], group_height, fill, 0.70);
      x += kWidths[c];
    }

    std::vector<wxString> base = {
        wxString::Format("%lu", static_cast<unsigned long>(i + 1)),
        wp.name,
        FormatCoordinate(wp.lat, true),
        FormatCoordinate(wp.lon, false),
        std::isfinite(wp.course_to_next)
            ? wxString::Format("%03.0f", wp.course_to_next)
            : wxString(),
        std::isfinite(wp.distance_to_next)
            ? wxString::Format("%.2f", wp.distance_to_next)
            : wxString()};

    x = kMargin;
    for (size_t c = 0; c < 6; ++c) {
      page.Text(x + 3, y + 6,
                FitText(base[c], kWidths[c], c >= 4 ? 7.0 : 7.2),
                c >= 4 ? 7.0 : 7.2, c == 0);
      x += kWidths[c];
    }

    const double sight_x = kMargin + kWidths[0] + kWidths[1] + kWidths[2] +
                           kWidths[3] + kWidths[4] + kWidths[5];
    for (size_t r = 0; r < subrows; ++r) {
      double sx = sight_x;
      const double row_y = y + r * kRowHeight;
      for (size_t c = 6; c < kWidths.size(); ++c) {
        page.Rect(sx, row_y, kWidths[c], kRowHeight, fill, 0.70);
        sx += kWidths[c];
      }

      if (r < wp.sights.size()) {
        const PdfSight& s = wp.sights[r];
        std::vector<wxString> values = {
            s.name, wxString::Format("%03.0f", s.bearing),
            wxString::Format("%.2f", s.range_nm),
            FormatCoordinate(s.lat, true), FormatCoordinate(s.lon, false)};
        sx = sight_x;
        for (size_t c = 0; c < values.size(); ++c) {
          page.Text(sx + 3, row_y + 6,
                    FitText(values[c], kWidths[c + 6], 7.0), 7.0);
          sx += kWidths[c + 6];
        }
      }
    }
    y += group_height;
  }

  std::vector<const RoutePil*> route_pils;
  for (const RoutePil& pil : m_pils) {
    const bool same_route =
        pil.route_guid == route_guid ||
        (pil.route_guid.IsEmpty() && pil.route_name == route_name);
    if (same_route) route_pils.push_back(&pil);
  }

  if (!route_pils.empty()) {
    if (y + 70.0 + route_pils.size() * kRowHeight > kBottom) {
      finish_page();
      first_page = false;
      page.Text(kMargin, 24, "Parallel Index Lines - " + route_name, 13.0,
                true);
      y = 52.0;
    } else {
      y += 24.0;
      page.Text(kMargin, y, "Parallel Index Lines", 11.0, true);
      y += 18.0;
    }

    const std::vector<double> pw = {36, 70, 70, 58, 66, 52, 62, 115, 115};
    const std::vector<wxString> ph = {"Leg", "From WP", "To WP", "Course °T",
                                      "Leg NM", "Side", "Offset NM", "Ref lat",
                                      "Ref lon"};
    double x = kMargin;
    for (size_t c = 0; c < pw.size(); ++c) {
      page.Rect(x, y, pw[c], kHeaderHeight, 0.90, 0.55);
      page.Text(x + 3, y + 6, FitText(ph[c], pw[c], 7.0), 7.0, true);
      x += pw[c];
    }
    y += kHeaderHeight;

    for (const RoutePil* pil : route_pils) {
      if (y + kRowHeight > kBottom) {
        finish_page();
        first_page = false;
        page.Text(kMargin, 24, "Parallel Index Lines - " + route_name, 13.0,
                  true);
        y = 52.0;
        x = kMargin;
        for (size_t c = 0; c < pw.size(); ++c) {
          page.Rect(x, y, pw[c], kHeaderHeight, 0.90, 0.55);
          page.Text(x + 3, y + 6, FitText(ph[c], pw[c], 7.0), 7.0, true);
          x += pw[c];
        }
        y += kHeaderHeight;
      }

      const std::vector<wxString> values = {
          wxString::Format("%d", pil->leg_index), pil->from_name, pil->to_name,
          wxString::Format("%03.0f", pil->course_true),
          wxString::Format("%.2f", pil->leg_distance_nm),
          pil->offset_nm >= 0.0 ? "Stbd" : "Port",
          wxString::Format("%.2f", std::fabs(pil->offset_nm)),
          FormatCoordinate(pil->anchor_lat, true),
          FormatCoordinate(pil->anchor_lon, false)};
      x = kMargin;
      for (size_t c = 0; c < pw.size(); ++c) {
        page.Rect(x, y, pw[c], kRowHeight, 0.985, 0.70);
        page.Text(x + 3, y + 6, FitText(values[c], pw[c], 7.0), 7.0);
        x += pw[c];
      }
      y += kRowHeight;
    }
  }

  page.Text(kMargin, std::min(kBottom + 10.0, y + 18.0),
            wxString::Format("Summary: %lu waypoints, %lu EBL/VRM sight marks, %lu PIL, %.2f NM total",
                             static_cast<unsigned long>(waypoints.size()),
                             static_cast<unsigned long>([&]() {
                               size_t n = 0;
                               for (const auto& wp : waypoints) n += wp.sights.size();
                               return n;
                             }()),
                             static_cast<unsigned long>(route_pils.size()),
                             total_distance_nm),
            8.0, true);
  finish_page();

  const wxString default_name = SafeFilename(route_name) + "-route-report.pdf";
  wxFileDialog save_dialog(
      m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
      _("Save Route Report PDF"), wxEmptyString, default_name,
      _("PDF files (*.pdf)|*.pdf"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (save_dialog.ShowModal() != wxID_OK) return;

  if (!pdf.Save(save_dialog.GetPath())) {
    wxMessageBox(_("Could not create the PDF report."), _("Route Report"),
                 wxOK | wxICON_ERROR,
                 m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window);
    return;
  }

  SetStatus(wxString::Format(_("Landscape PDF report saved: %s"),
                             save_dialog.GetPath()));
  wxMessageDialog open_dialog(
      m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window,
      _("The landscape PDF Route Report has been saved. Open it now?"),
      _("Route Report"), wxYES_NO | wxYES_DEFAULT | wxICON_INFORMATION);
  if (open_dialog.ShowModal() == wxID_YES)
    wxLaunchDefaultApplication(save_dialog.GetPath());
}
