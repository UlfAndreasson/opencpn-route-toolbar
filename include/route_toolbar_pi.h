// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <vector>

#include <wx/bitmap.h>
#include <wx/window.h>

#include "ocpn_plugin.h"

class RouteToolbarDialog;

class route_toolbar_pi final : public opencpn_plugin_121 {
public:
  explicit route_toolbar_pi(void* ppimgr);
  ~route_toolbar_pi() override;

  int Init() override;
  bool DeInit() override;

  int GetAPIVersionMajor() override;
  int GetAPIVersionMinor() override;
  int GetPlugInVersionMajor() override;
  int GetPlugInVersionMinor() override;
  int GetPlugInVersionPatch() override;

  wxString GetCommonName() override;
  wxString GetShortDescription() override;
  wxString GetLongDescription() override;
  wxBitmap* GetPlugInBitmap() override;

  int GetToolbarToolCount() override;
  void OnToolbarToolCallback(int id) override;

  bool MouseEventHook(wxMouseEvent& event) override;
  void SetCursorLatLon(double lat, double lon) override;
  bool RenderOverlay(wxDC& dc, PlugIn_ViewPort* vp) override;
  bool RenderGLOverlay(wxGLContext* pcontext, PlugIn_ViewPort* vp) override;

  void BeginEblVrm();
  void CancelInteractiveTool();
  void OnDialogHidden();

private:
  struct RouteWaypointHit {
    bool found{false};
    double lat{0.0};
    double lon{0.0};
    double distance_nm{0.0};
    wxString route_guid;
    wxString route_name;
    wxString waypoint_guid;
    wxString waypoint_name;
  };

  struct RouteEblVrm {
    double start_lat{0.0};
    double start_lon{0.0};
    double target_lat{0.0};
    double target_lon{0.0};
    double bearing_true{0.0};
    double range_nm{0.0};
    double arc_width_deg{90.0};
    wxString route_guid;
    wxString route_name;
    wxString waypoint_guid;
    wxString waypoint_name;
    wxString target_name;
  };

  enum class EblMode {
    Idle,
    AwaitRoutePoint,
    AwaitTarget,
  };

  void ShowToolWindow();
  void HideToolWindow();
  void SetStatus(const wxString& text);
  RouteWaypointHit FindNearestRouteWaypoint(double lat, double lon,
                                            double max_distance_nm) const;
  void CompleteEblVrm(double target_lat, double target_lon);
  void DrawEblVrm(wxDC& dc, PlugIn_ViewPort* vp, const RouteEblVrm& item,
                  bool preview) const;
  void DrawEblVrmGL(PlugIn_ViewPort* vp, const RouteEblVrm& item,
                    bool preview) const;
  void DrawGlLabel(const wxString& label, int x, int y) const;
  static double NormalizeBearing(double bearing);

  wxWindow* m_parent_window{nullptr};
  RouteToolbarDialog* m_dialog{nullptr};
  int m_toolbar_id{-1};
  bool m_dialog_visible{false};
  wxBitmap m_toolbar_bitmap;

  EblMode m_ebl_mode{EblMode::Idle};
  RouteWaypointHit m_pending_start;
  std::vector<RouteEblVrm> m_ebl_vrms;
  double m_cursor_lat{0.0};
  double m_cursor_lon{0.0};
  bool m_has_cursor{false};
};
