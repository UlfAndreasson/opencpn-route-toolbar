// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_pi.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <wx/brush.h>
#include <wx/dc.h>
#include <wx/dcmemory.h>
#include <wx/image.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace {

struct RouteLegLabelGeometry {
  wxPoint p0;
  wxPoint p1;
  wxPoint p2;
  wxPoint p3;
  double wx_angle_deg{0.0};
};

RouteLegLabelGeometry BuildRouteLegLabelGeometry(const wxPoint& a,
                                                 const wxPoint& b,
                                                 const wxSize& extent) {
  double dx = static_cast<double>(b.x - a.x);
  double dy = static_cast<double>(b.y - a.y);
  const double length = std::hypot(dx, dy);
  if (length < 0.5) return {};

  double ux = dx / length;
  double uy = dy / length;

  // Keep the text readable left-to-right while preserving the exact line
  // direction. A purely vertical leg is oriented upward so the annotation
  // naturally sits on the left side of the route line.
  if (ux < 0.0 || (std::abs(ux) < 1e-9 && uy > 0.0)) {
    ux = -ux;
    uy = -uy;
  }

  // Screen-space normal pointing to the visual "upper/left" side of the leg.
  const double nx = uy;
  const double ny = -ux;

  constexpr double gap_px = 1.0;
  const double half_w = static_cast<double>(extent.x) / 2.0;
  const double half_h = static_cast<double>(extent.y) / 2.0;
  const double mid_x = (static_cast<double>(a.x) + b.x) / 2.0;
  const double mid_y = (static_cast<double>(a.y) + b.y) / 2.0;

  // Centre the label on the leg midpoint and offset it by just one pixel so
  // the lower edge of the text essentially touches the route line.
  const double cx = mid_x + nx * (half_h + gap_px);
  const double cy = mid_y + ny * (half_h + gap_px);

  RouteLegLabelGeometry g;
  g.p0 = wxPoint(static_cast<int>(std::lround(cx - ux * half_w + nx * half_h)),
                 static_cast<int>(std::lround(cy - uy * half_w + ny * half_h)));
  g.p1 = wxPoint(static_cast<int>(std::lround(cx + ux * half_w + nx * half_h)),
                 static_cast<int>(std::lround(cy + uy * half_w + ny * half_h)));
  g.p2 = wxPoint(static_cast<int>(std::lround(cx + ux * half_w - nx * half_h)),
                 static_cast<int>(std::lround(cy + uy * half_w - ny * half_h)));
  g.p3 = wxPoint(static_cast<int>(std::lround(cx - ux * half_w - nx * half_h)),
                 static_cast<int>(std::lround(cy - uy * half_w - ny * half_h)));

  // wxDC uses positive angles counter-clockwise. Screen Y grows downward, so
  // negate the screen-space line angle.
  g.wx_angle_deg = -std::atan2(uy, ux) * 180.0 / M_PI;
  return g;
}

bool RouteLegLabelVisible(const RouteLegLabelGeometry& g,
                          const PlugIn_ViewPort* vp) {
  if (!vp) return false;
  const int min_x = std::min({g.p0.x, g.p1.x, g.p2.x, g.p3.x});
  const int max_x = std::max({g.p0.x, g.p1.x, g.p2.x, g.p3.x});
  const int min_y = std::min({g.p0.y, g.p1.y, g.p2.y, g.p3.y});
  const int max_y = std::max({g.p0.y, g.p1.y, g.p2.y, g.p3.y});
  return max_x >= 0 && max_y >= 0 && min_x < vp->pix_width &&
         min_y < vp->pix_height;
}

void DrawGlRotatedText(const wxString& label, const wxFont& font,
                       const wxColour& colour,
                       const RouteLegLabelGeometry& g) {
  if (label.IsEmpty()) return;

  wxBitmap measure_bitmap(1, 1, 24);
  wxMemoryDC measure_dc(measure_bitmap);
  measure_dc.SetFont(font);
  const wxSize extent = measure_dc.GetTextExtent(label);
  measure_dc.SelectObject(wxNullBitmap);
  if (extent.x <= 0 || extent.y <= 0) return;

  const int width = extent.x + 2;
  const int height = extent.y + 2;
  wxBitmap bitmap(width, height, 24);
  wxMemoryDC dc(bitmap);
  dc.SetBackground(wxBrush(*wxBLACK));
  dc.Clear();
  dc.SetFont(font);
  dc.SetTextForeground(*wxWHITE);
  dc.DrawText(label, 1, 1);
  dc.SelectObject(wxNullBitmap);

  wxImage image = bitmap.ConvertToImage();
  if (!image.IsOk() || !image.GetData()) return;
  const unsigned char* rgb = image.GetData();
  std::vector<unsigned char> rgba(static_cast<size_t>(width * height * 4), 0);
  for (int i = 0; i < width * height; ++i) {
    const unsigned char alpha = std::max(rgb[i * 3],
                                 std::max(rgb[i * 3 + 1], rgb[i * 3 + 2]));
    rgba[i * 4] = colour.Red();
    rgba[i * 4 + 1] = colour.Green();
    rgba[i * 4 + 2] = colour.Blue();
    rgba[i * 4 + 3] = alpha;
  }

  GLint previous_texture = 0;
  GLint previous_unpack_alignment = 4;
  const GLboolean texture_was_enabled = glIsEnabled(GL_TEXTURE_2D);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
  GLuint texture = 0;
  glGenTextures(1, &texture);
  if (!texture) return;

  glBindTexture(GL_TEXTURE_2D, texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
  glEnable(GL_TEXTURE_2D);
  glColor4ub(255, 255, 255, 255);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f); glVertex2i(g.p0.x, g.p0.y);
  glTexCoord2f(1.0f, 0.0f); glVertex2i(g.p1.x, g.p1.y);
  glTexCoord2f(1.0f, 1.0f); glVertex2i(g.p2.x, g.p2.y);
  glTexCoord2f(0.0f, 1.0f); glVertex2i(g.p3.x, g.p3.y);
  glEnd();

  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
  glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack_alignment);
  if (texture_was_enabled) glEnable(GL_TEXTURE_2D); else glDisable(GL_TEXTURE_2D);
  glDeleteTextures(1, &texture);
}

}  // namespace

wxColour route_toolbar_pi::ResolveRouteColour(const wxString&) const {
  // OpenCPN API 1.21 does not expose the route's optional GPXX display colour
  // through PlugIn_Route / PlugIn_Route_Ex. Use OpenCPN's current route display
  // colour so labels follow the normal/default route rendering and adapt to the
  // active colour scheme.
  wxColour colour(0, 105, 255);
  wxColour global;
  if (GetGlobalColor("UINFB", &global) && global.IsOk()) colour = global;
  return colour;
}

void route_toolbar_pi::DrawRouteLegLabels(wxDC& dc, PlugIn_ViewPort* vp) const {
  if (!vp || !vp->bValid || !m_notes_visible) return;

  const wxFont old_font = dc.GetFont();
  const wxColour old_text = dc.GetTextForeground();
  dc.SetFont(GetEblLabelFont());

  const wxArrayString routes = GetRouteGUIDArray();
  for (size_t r = 0; r < routes.GetCount(); ++r) {
    auto route = GetRoute_Plugin(routes[r]);
    if (!route || !route->pWaypointList) continue;

    std::vector<PlugIn_Waypoint*> points;
    for (auto node = route->pWaypointList->GetFirst(); node; node = node->GetNext()) {
      if (node->GetData()) points.push_back(node->GetData());
    }
    if (points.size() < 2) continue;

    const wxColour colour = ResolveRouteColour(routes[r]);
    dc.SetTextForeground(colour);

    for (size_t i = 0; i + 1 < points.size(); ++i) {
      double course = 0.0;
      double distance = 0.0;
      DistanceBearingMercator_Plugin(points[i + 1]->m_lat, points[i + 1]->m_lon,
                                     points[i]->m_lat, points[i]->m_lon,
                                     &course, &distance);
      course = NormalizeBearing(course);
      const wxString text = wxString::Format("%03.0f°T  %.2f M", course, distance);

      wxPoint a, b;
      GetCanvasPixLL(vp, &a, points[i]->m_lat, points[i]->m_lon);
      GetCanvasPixLL(vp, &b, points[i + 1]->m_lat, points[i + 1]->m_lon);
      const wxSize extent = dc.GetTextExtent(text);
      const RouteLegLabelGeometry g = BuildRouteLegLabelGeometry(a, b, extent);
      if (RouteLegLabelVisible(g, vp))
        dc.DrawRotatedText(text, g.p0, g.wx_angle_deg);
    }
  }

  dc.SetTextForeground(old_text);
  dc.SetFont(old_font);
}

void route_toolbar_pi::DrawGlText(const wxString& label, int x, int y,
                                  const wxColour& colour) const {
  if (label.IsEmpty()) return;

  const wxFont font = GetEblLabelFont();
  wxBitmap measure_bitmap(1, 1, 24);
  wxMemoryDC measure_dc(measure_bitmap);
  measure_dc.SetFont(font);
  const wxSize extent = measure_dc.GetTextExtent(label);
  measure_dc.SelectObject(wxNullBitmap);
  if (extent.x <= 0 || extent.y <= 0) return;

  const int width = extent.x + 2;
  const int height = extent.y + 2;
  wxBitmap bitmap(width, height, 24);
  wxMemoryDC dc(bitmap);
  dc.SetBackground(wxBrush(*wxBLACK));
  dc.Clear();
  dc.SetFont(font);
  dc.SetTextForeground(*wxWHITE);
  dc.DrawText(label, 1, 1);
  dc.SelectObject(wxNullBitmap);

  wxImage image = bitmap.ConvertToImage();
  if (!image.IsOk() || !image.GetData()) return;
  const unsigned char* rgb = image.GetData();
  std::vector<unsigned char> rgba(static_cast<size_t>(width * height * 4), 0);
  for (int i = 0; i < width * height; ++i) {
    const unsigned char alpha = std::max(rgb[i * 3],
                                 std::max(rgb[i * 3 + 1], rgb[i * 3 + 2]));
    rgba[i * 4] = colour.Red();
    rgba[i * 4 + 1] = colour.Green();
    rgba[i * 4 + 2] = colour.Blue();
    rgba[i * 4 + 3] = alpha;
  }

  GLint previous_texture = 0;
  GLint previous_unpack_alignment = 4;
  const GLboolean texture_was_enabled = glIsEnabled(GL_TEXTURE_2D);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
  GLuint texture = 0;
  glGenTextures(1, &texture);
  if (!texture) return;

  glBindTexture(GL_TEXTURE_2D, texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
  glEnable(GL_TEXTURE_2D);
  glColor4ub(255, 255, 255, 255);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f); glVertex2i(x, y);
  glTexCoord2f(1.0f, 0.0f); glVertex2i(x + width, y);
  glTexCoord2f(1.0f, 1.0f); glVertex2i(x + width, y + height);
  glTexCoord2f(0.0f, 1.0f); glVertex2i(x, y + height);
  glEnd();

  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
  glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack_alignment);
  if (texture_was_enabled) glEnable(GL_TEXTURE_2D); else glDisable(GL_TEXTURE_2D);
  glDeleteTextures(1, &texture);
}

void route_toolbar_pi::DrawRouteLegLabelsGL(PlugIn_ViewPort* vp) const {
  if (!vp || !vp->bValid || !m_notes_visible) return;

  const wxFont font = GetEblLabelFont();
  wxBitmap measure_bitmap(1, 1, 24);
  wxMemoryDC measure_dc(measure_bitmap);
  measure_dc.SetFont(font);

  const wxArrayString routes = GetRouteGUIDArray();
  for (size_t r = 0; r < routes.GetCount(); ++r) {
    auto route = GetRoute_Plugin(routes[r]);
    if (!route || !route->pWaypointList) continue;

    std::vector<PlugIn_Waypoint*> points;
    for (auto node = route->pWaypointList->GetFirst(); node; node = node->GetNext()) {
      if (node->GetData()) points.push_back(node->GetData());
    }
    if (points.size() < 2) continue;

    const wxColour colour = ResolveRouteColour(routes[r]);
    for (size_t i = 0; i + 1 < points.size(); ++i) {
      double course = 0.0;
      double distance = 0.0;
      DistanceBearingMercator_Plugin(points[i + 1]->m_lat, points[i + 1]->m_lon,
                                     points[i]->m_lat, points[i]->m_lon,
                                     &course, &distance);
      course = NormalizeBearing(course);
      const wxString text = wxString::Format("%03.0f°T  %.2f M", course, distance);
      const wxSize extent = measure_dc.GetTextExtent(text);

      wxPoint a, b;
      GetCanvasPixLL(vp, &a, points[i]->m_lat, points[i]->m_lon);
      GetCanvasPixLL(vp, &b, points[i + 1]->m_lat, points[i + 1]->m_lon);
      const RouteLegLabelGeometry g = BuildRouteLegLabelGeometry(a, b, extent);
      if (RouteLegLabelVisible(g, vp))
        DrawGlRotatedText(text, font, colour, g);
    }
  }

  measure_dc.SelectObject(wxNullBitmap);
}
