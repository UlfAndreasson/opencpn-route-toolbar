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

wxPoint RouteLegLabelPosition(const wxPoint& a, const wxPoint& b,
                              const wxSize& extent) {
  const int mid_x = (a.x + b.x) / 2;
  const int mid_y = (a.y + b.y) / 2;
  const int dx = std::abs(b.x - a.x);
  const int dy = std::abs(b.y - a.y);
  constexpr int gap_px = 4;

  // Keep the annotation physically attached to the route leg. Mostly
  // vertical legs get the label immediately to the left; all other legs get
  // it immediately above. No viewport-edge clamping is used, since clamping
  // makes labels appear to move independently of the route as the chart pans.
  if (dy > dx)
    return wxPoint(mid_x - extent.x - gap_px, mid_y - extent.y / 2);

  return wxPoint(mid_x - extent.x / 2, mid_y - extent.y - gap_px);
}

bool RouteLegLabelVisible(const wxPoint& pos, const wxSize& extent,
                          const PlugIn_ViewPort* vp) {
  if (!vp) return false;
  return pos.x + extent.x >= 0 && pos.y + extent.y >= 0 &&
         pos.x < vp->pix_width && pos.y < vp->pix_height;
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
      const wxPoint pos = RouteLegLabelPosition(a, b, extent);
      if (RouteLegLabelVisible(pos, extent, vp))
        dc.DrawText(text, pos);
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

  wxBitmap measure_bitmap(1, 1, 24);
  wxMemoryDC measure_dc(measure_bitmap);
  measure_dc.SetFont(GetEblLabelFont());

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
      const wxPoint pos = RouteLegLabelPosition(a, b, extent);
      if (RouteLegLabelVisible(pos, extent, vp))
        DrawGlText(text, pos.x, pos.y, colour);
    }
  }

  measure_dc.SelectObject(wxNullBitmap);
}
