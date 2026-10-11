// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_pi.h"
#include "route_toolbar_dialog.h"

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
constexpr double kPageW = 842.0;   // A4 landscape, PDF points
constexpr double kPageH = 595.0;
constexpr double kMargin = 24.0;
constexpr double kBottom = 558.0;
constexpr double kRowH = 18.0;
constexpr double kHeadH = 22.0;

const std::vector<double> kWidths = {28, 60, 82, 86, 52, 52,
                                     94, 52, 52, 116, 120};
const std::vector<wxString> kHeaders = {
    "WP", "Waypoint", "Latitude", "Longitude", "Course °T", "Dist NM",
    "Sight mark", "EBL °T", "Range NM", "Sight lat", "Sight lon"};

struct SightRow {
  wxString name;
  double bearing{0.0};
  double range_nm{0.0};
  double lat{0.0};
  double lon{0.0};
};

struct WaypointRow {
  double lat{0.0};
  double lon{0.0};
  wxString guid;
  wxString name;
  double course{NAN};
  double distance{NAN};
  std::vector<SightRow> sights;
};

wxString Coord(double value, bool latitude) {
  const double a = std::fabs(value);
  const int deg = static_cast<int>(std::floor(a));
  const double min = (a - deg) * 60.0;
  const wxChar hemi = latitude ? (value >= 0 ? 'N' : 'S')
                               : (value >= 0 ? 'E' : 'W');
  return latitude ? wxString::Format("%02d° %06.3f' %c", deg, min, hemi)
                  : wxString::Format("%03d° %06.3f' %c", deg, min, hemi);
}

wxString SafeName(wxString name) {
  if (name.IsEmpty()) name = "route";
  static const wxString invalid = "\\/:*?\"<>|";
  for (wxUniChar ch : invalid) name.Replace(wxString(ch), "-");
  name.Trim(true).Trim(false);
  return name.IsEmpty() ? wxString("route") : name;
}

std::string PdfText(const wxString& text) {
  std::string out;
  out.reserve(text.length() + 8);
  for (wxUniChar ch : text) {
    const unsigned long v = ch.GetValue();
    char c = v <= 255 ? static_cast<char>(v) : '?';
    if (c == '\\' || c == '(' || c == ')') out.push_back('\\');
    out.push_back(c);
  }
  return out;
}

wxString Fit(wxString text, double width, double size = 7.0) {
  const int n = std::max(1, static_cast<int>((width - 6.0) / (size * 0.52)));
  if (static_cast<int>(text.length()) <= n) return text;
  if (n < 4) return text.Left(static_cast<size_t>(n));
  return text.Left(static_cast<size_t>(n - 3)) + "...";
}

struct Canvas {
  std::ostringstream s;
  static double Y(double y) { return kPageH - y; }

  void Rect(double x, double y, double w, double h, double fill = 1.0) {
    s << std::fixed << std::setprecision(2) << fill << " g " << x << ' '
      << (kPageH - y - h) << ' ' << w << ' ' << h << " re f\n"
      << "0.70 G 0.45 w " << x << ' ' << (kPageH - y - h) << ' ' << w
      << ' ' << h << " re S\n";
  }

  void Text(double x, double y, const wxString& text, double size = 7.0,
            bool bold = false) {
    if (text.IsEmpty()) return;
    s << "0 g BT /" << (bold ? "F2" : "F1") << ' ' << std::fixed
      << std::setprecision(1) << size << " Tf 1 0 0 1 "
      << std::setprecision(2) << x << ' ' << (kPageH - y - size)
      << " Tm (" << PdfText(text) << ") Tj ET\n";
  }
};

class PdfDoc {
public:
  void AddPage(const std::string& page) { pages_.push_back(page); }

  bool Save(const wxString& path) const {
    if (pages_.empty()) return false;
    const int count = 4 + static_cast<int>(pages_.size()) * 2;
    std::vector<std::string> obj(static_cast<size_t>(count + 1));
    obj[1] = "<< /Type /Catalog /Pages 2 0 R >>";
    obj[3] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";
    obj[4] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>";

    std::ostringstream kids;
    kids << '[';
    for (size_t i = 0; i < pages_.size(); ++i) {
      const int content = 5 + static_cast<int>(i) * 2;
      const int page = content + 1;
      kids << page << " 0 R ";
      obj[content] = "<< /Length " + std::to_string(pages_[i].size()) +
                     " >>\nstream\n" + pages_[i] + "\nendstream";
      obj[page] = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 842 595] "
                  "/Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> "
                  "/Contents " + std::to_string(content) + " 0 R >>";
    }
    kids << ']';
    obj[2] = "<< /Type /Pages /Kids " + kids.str() + " /Count " +
             std::to_string(pages_.size()) + " >>";

    std::ostringstream out;
    out << "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
    std::vector<long long> offsets(static_cast<size_t>(count + 1));
    for (int i = 1; i <= count; ++i) {
      offsets[static_cast<size_t>(i)] = static_cast<long long>(out.tellp());
      out << i << " 0 obj\n" << obj[static_cast<size_t>(i)] << "\nendobj\n";
    }
    const long long xref = static_cast<long long>(out.tellp());
    out << "xref\n0 " << count + 1 << "\n0000000000 65535 f \n";
    for (int i = 1; i <= count; ++i)
      out << std::setw(10) << std::setfill('0') << offsets[static_cast<size_t>(i)]
          << " 00000 n \n";
    out << "trailer\n<< /Size " << count + 1
        << " /Root 1 0 R >>\nstartxref\n" << xref << "\n%%EOF\n";

    const std::string bytes = out.str();
    wxFFile f(path, "wb");
    if (!f.IsOpened()) return false;
    f.Write(bytes.data(), bytes.size());
    f.Close();
    return true;
  }

private:
  std::vector<std::string> pages_;
};

void TableHeader(Canvas& c, double y) {
  double x = kMargin;
  for (size_t i = 0; i < kWidths.size(); ++i) {
    c.Rect(x, y, kWidths[i], kHeadH, 0.90);
    c.Text(x + 3, y + 6, Fit(kHeaders[i], kWidths[i]), 7.0, true);
    x += kWidths[i];
  }
}

void PageHeader(Canvas& c, const wxString& route, double total,
                bool continued) {
  c.Text(kMargin, 20,
         continued ? "Route Report - " + route + " (continued)"
                   : "Route Report - " + route,
         14.0, true);
  if (!continued) {
    c.Text(kMargin, 42,
           "Generated: " + wxDateTime::Now().FormatISOCombined(' '), 8.0);
    c.Text(330, 42, wxString::Format("Route distance: %.2f NM", total), 8.0,
           true);
  }
  TableHeader(c, continued ? 50.0 : 62.0);
}
}  // namespace

void route_toolbar_pi::ExportRoutePdfReport() {
  CancelInteractiveTool();
  wxWindow* parent = m_dialog ? static_cast<wxWindow*>(m_dialog)
                              : m_parent_window;

  const wxArrayString guids = GetRouteGUIDArray();
  if (guids.IsEmpty()) {
    wxMessageBox(_("There are no OpenCPN routes to export."), _("Route Report"),
                 wxOK | wxICON_INFORMATION, parent);
    return;
  }

  wxArrayString choices;
  std::vector<size_t> mapping;
  for (size_t i = 0; i < guids.GetCount(); ++i) {
    auto route = GetRoute_Plugin(guids[i]);
    if (!route || !route->pWaypointList) continue;
    wxString name = route->m_NameString;
    if (name.IsEmpty()) name = _("Unnamed route");
    size_t n = 0;
    for (auto node = route->pWaypointList->GetFirst(); node; node = node->GetNext())
      ++n;
    choices.Add(wxString::Format("%s  (%lu WP)", name,
                                 static_cast<unsigned long>(n)));
    mapping.push_back(i);
  }
  if (choices.IsEmpty()) return;

  wxSingleChoiceDialog pick(parent,
                            _("Select the OpenCPN route to include in the PDF report."),
                            _("Route Report - PDF"), choices);
  if (pick.ShowModal() != wxID_OK) return;
  const int selected = pick.GetSelection();
  if (selected < 0 || static_cast<size_t>(selected) >= mapping.size()) return;

  const wxString route_guid = guids[mapping[static_cast<size_t>(selected)]];
  auto route = GetRoute_Plugin(route_guid);
  if (!route || !route->pWaypointList) return;
  wxString route_name = route->m_NameString;
  if (route_name.IsEmpty()) route_name = _("Unnamed route");

  std::vector<WaypointRow> rows;
  for (auto node = route->pWaypointList->GetFirst(); node; node = node->GetNext()) {
    PlugIn_Waypoint* wp = node->GetData();
    if (!wp) continue;
    WaypointRow row;
    row.lat = wp->m_lat;
    row.lon = wp->m_lon;
    row.guid = wp->m_GUID;
    row.name = wp->m_MarkName;
    rows.push_back(row);
  }
  if (rows.empty()) return;

  double total = 0.0;
  for (size_t i = 0; i + 1 < rows.size(); ++i) {
    double course = 0.0, distance = 0.0;
    DistanceBearingMercator_Plugin(rows[i + 1].lat, rows[i + 1].lon,
                                   rows[i].lat, rows[i].lon,
                                   &course, &distance);
    rows[i].course = NormalizeBearing(course);
    rows[i].distance = distance;
    total += distance;
  }

  size_t sight_count = 0;
  for (WaypointRow& wp : rows) {
    for (const RouteEblVrm& ebl : m_ebl_vrms) {
      const bool same_route = ebl.route_guid == route_guid ||
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
      wp.sights.push_back({ebl.target_name, ebl.bearing_true, ebl.range_nm,
                           ebl.target_lat, ebl.target_lon});
      ++sight_count;
    }
  }

  std::vector<const RoutePil*> pils;
  for (const RoutePil& pil : m_pils) {
    if (pil.route_guid == route_guid ||
        (pil.route_guid.IsEmpty() && pil.route_name == route_name))
      pils.push_back(&pil);
  }

  PdfDoc pdf;
  Canvas page;
  PageHeader(page, route_name, total, false);
  double y = 62.0 + kHeadH;
  auto finish_page = [&]() {
    pdf.AddPage(page.s.str());
    page = Canvas{};
  };

  for (size_t i = 0; i < rows.size(); ++i) {
    const WaypointRow& wp = rows[i];
    const size_t n = std::max<size_t>(1, wp.sights.size());
    const double h = n * kRowH;
    if (y + h > kBottom) {
      finish_page();
      PageHeader(page, route_name, total, true);
      y = 50.0 + kHeadH;
    }

    const double fill = (i % 2 == 0) ? 0.985 : 0.955;
    double x = kMargin;
    const std::vector<wxString> base = {
        wxString::Format("%lu", static_cast<unsigned long>(i + 1)), wp.name,
        Coord(wp.lat, true), Coord(wp.lon, false),
        std::isfinite(wp.course) ? wxString::Format("%03.0f", wp.course) : wxString(),
        std::isfinite(wp.distance) ? wxString::Format("%.2f", wp.distance) : wxString()};
    for (size_t c = 0; c < 6; ++c) {
      page.Rect(x, y, kWidths[c], h, fill);
      page.Text(x + 3, y + 6, Fit(base[c], kWidths[c]), 7.0, c == 0);
      x += kWidths[c];
    }

    const double sx0 = x;
    for (size_t r = 0; r < n; ++r) {
      double sx = sx0;
      const double ry = y + r * kRowH;
      for (size_t c = 6; c < kWidths.size(); ++c) {
        page.Rect(sx, ry, kWidths[c], kRowH, fill);
        sx += kWidths[c];
      }
      if (r < wp.sights.size()) {
        const SightRow& s = wp.sights[r];
        const std::vector<wxString> vals = {
            s.name, wxString::Format("%03.0f", s.bearing),
            wxString::Format("%.2f", s.range_nm), Coord(s.lat, true),
            Coord(s.lon, false)};
        sx = sx0;
        for (size_t c = 0; c < vals.size(); ++c) {
          page.Text(sx + 3, ry + 6, Fit(vals[c], kWidths[c + 6]), 7.0);
          sx += kWidths[c + 6];
        }
      }
    }
    y += h;
  }

  if (!pils.empty()) {
    const std::vector<double> pw = {36, 70, 70, 58, 66, 52, 62, 115, 115};
    const std::vector<wxString> ph = {"Leg", "From WP", "To WP", "Course °T",
                                      "Leg NM", "Side", "Offset NM", "Ref lat",
                                      "Ref lon"};
    const auto draw_pil_header = [&]() {
      double x = kMargin;
      for (size_t c = 0; c < pw.size(); ++c) {
        page.Rect(x, y, pw[c], kHeadH, 0.90);
        page.Text(x + 3, y + 6, Fit(ph[c], pw[c]), 7.0, true);
        x += pw[c];
      }
      y += kHeadH;
    };

    if (y + 50.0 > kBottom) {
      finish_page();
      page.Text(kMargin, 22, "Parallel Index Lines - " + route_name, 13.0, true);
      y = 50.0;
    } else {
      y += 22.0;
      page.Text(kMargin, y, "Parallel Index Lines", 11.0, true);
      y += 18.0;
    }
    draw_pil_header();

    for (const RoutePil* pil : pils) {
      if (y + kRowH > kBottom) {
        finish_page();
        page.Text(kMargin, 22, "Parallel Index Lines - " + route_name, 13.0, true);
        y = 50.0;
        draw_pil_header();
      }
      const std::vector<wxString> vals = {
          wxString::Format("%d", pil->leg_index), pil->from_name, pil->to_name,
          wxString::Format("%03.0f", pil->course_true),
          wxString::Format("%.2f", pil->leg_distance_nm),
          pil->offset_nm >= 0.0 ? wxString("Stbd") : wxString("Port"),
          wxString::Format("%.2f", std::fabs(pil->offset_nm)),
          Coord(pil->anchor_lat, true), Coord(pil->anchor_lon, false)};
      double x = kMargin;
      for (size_t c = 0; c < pw.size(); ++c) {
        page.Rect(x, y, pw[c], kRowH, 0.985);
        page.Text(x + 3, y + 6, Fit(vals[c], pw[c]), 7.0);
        x += pw[c];
      }
      y += kRowH;
    }
  }

  if (y + 26.0 > kBottom) {
    finish_page();
    y = 32.0;
  } else {
    y += 18.0;
  }
  page.Text(kMargin, y,
            wxString::Format("Summary: %lu waypoints, %lu EBL/VRM sight marks, %lu PIL, %.2f NM total",
                             static_cast<unsigned long>(rows.size()),
                             static_cast<unsigned long>(sight_count),
                             static_cast<unsigned long>(pils.size()), total),
            8.0, true);
  finish_page();

  const wxString default_name = SafeName(route_name) + "-route-report.pdf";
  wxFileDialog save(parent, _("Save Route Report PDF"), wxEmptyString,
                    default_name, _("PDF files (*.pdf)|*.pdf"),
                    wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (save.ShowModal() != wxID_OK) return;

  if (!pdf.Save(save.GetPath())) {
    wxMessageBox(_("Could not create the PDF report."), _("Route Report"),
                 wxOK | wxICON_ERROR, parent);
    return;
  }

  SetStatus(_("Landscape PDF Route Report saved."));
  wxMessageDialog open(parent,
                       _("The landscape PDF Route Report has been saved. Open it now?"),
                       _("Route Report"),
                       wxYES_NO | wxYES_DEFAULT | wxICON_INFORMATION);
  if (open.ShowModal() == wxID_YES) wxLaunchDefaultApplication(save.GetPath());
}
