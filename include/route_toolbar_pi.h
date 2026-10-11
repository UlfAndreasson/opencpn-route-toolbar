// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <vector>

#include <wx/bitmap.h>
#include <wx/colour.h>
#include <wx/font.h>
#include <wx/window.h>

#include "ocpn_plugin.h"

class RouteToolbarDialog;
class wxFileConfig;
class wxMenu;
class wxMenuItem;

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
  void OnContextMenuItemCallback(int id) override;

  bool MouseEventHook(wxMouseEvent& event) override;
  void SetCursorLatLon(double lat, double lon) override;
  bool RenderOverlay(wxDC& dc, PlugIn_ViewPort* vp) override;
  bool RenderGLOverlay(wxGLContext* pcontext, PlugIn_ViewPort* vp) override;

  void BeginEblVrm();
  void BeginPil();
  void ExportRouteReport();
  void ExportRouteCsvReport();
  void ExportRoutePdfReport();
  void CancelInteractiveTool();
  void OnDialogHidden();
  void EnsureContextMenu();

  int GetLabelFontSize() const { return m_label_font_size; }
  void SetLabelFontSize(int point_size);

  const wxColour& GetEblColour() const { return m_ebl_colour; }
  const wxColour& GetPilColour() const { return m_pil_colour; }
  void SetEblColour(const wxColour& colour) {
    if (!colour.IsOk()) return;
    m_ebl_colour = colour;
    SavePersistentState();
    if (m_parent_window) RequestRefresh(m_parent_window);
  }
  void SetPilColour(const wxColour& colour) {
    if (!colour.IsOk()) return;
    m_pil_colour = colour;
    SavePersistentState();
    if (m_parent_window) RequestRefresh(m_parent_window);
  }

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

  struct RouteLegHit {
    bool found{false};
    double pick_distance_nm{0.0};
    double start_lat{0.0};
    double start_lon{0.0};
    double end_lat{0.0};
    double end_lon{0.0};
    double course_true{0.0};
    double distance_nm{0.0};
    int leg_index{0};
    wxString route_guid;
    wxString route_name;
    wxString from_guid;
    wxString from_name;
    wxString to_guid;
    wxString to_name;
  };

  struct RoutePil {
    double leg_start_lat{0.0};
    double leg_start_lon{0.0};
    double leg_end_lat{0.0};
    double leg_end_lon{0.0};
    double line_start_lat{0.0};
    double line_start_lon{0.0};
    double line_end_lat{0.0};
    double line_end_lon{0.0};
    double anchor_lat{0.0};
    double anchor_lon{0.0};
    double course_true{0.0};
    double leg_distance_nm{0.0};
    double offset_nm{0.0};  // positive = starboard, negative = port
    int leg_index{0};
    wxString route_guid;
    wxString route_name;
    wxString from_guid;
    wxString from_name;
    wxString to_guid;
    wxString to_name;
  };

  enum class EblMode {
    Idle,
    AwaitRoutePoint,
    AwaitTarget,
    MoveExistingTarget,
  };

  enum class PilMode {
    Idle,
    AwaitLeg,
    AwaitOffsetPoint,
    MoveExistingAnchor,
  };

  void ShowToolWindow();
  void HideToolWindow();
  void SetStatus(const wxString& text);
  void UpdateToolButtons();
  void UpdateContextMenuHit();

  RouteWaypointHit FindNearestRouteWaypoint(double lat, double lon,
                                            double max_distance_nm) const;
  int FindNearestEblTarget(double lat, double lon,
                           double max_distance_nm) const;
  void ShowEblTargetActions(int index);
  void BeginMoveEblTarget(int index);
  bool CommitMoveEblTarget(double target_lat, double target_lon);
  void RenameEblTarget(int index);
  void DeleteEblTarget(int index);
  void RecalculateEbl(RouteEblVrm& item) const;
  void CompleteEblVrm(double target_lat, double target_lon);

  RouteLegHit FindNearestRouteLeg(double lat, double lon,
                                  double max_distance_nm) const;
  int FindNearestPil(double lat, double lon, double max_distance_nm) const;
  bool BuildPilFromAnchor(const RouteLegHit& leg, double anchor_lat,
                          double anchor_lon, RoutePil& item) const;
  void CompletePil(double anchor_lat, double anchor_lon);
  void ShowPilActions(int index);
  void BeginMovePilAnchor(int index);
  bool CommitMovePilAnchor(double anchor_lat, double anchor_lon);
  void DeletePil(int index);
  void DrawPil(wxDC& dc, PlugIn_ViewPort* vp, const RoutePil& item,
               bool preview) const;
  void DrawPilGL(PlugIn_ViewPort* vp, const RoutePil& item,
                 bool preview) const;
  void LoadPilPersistentState(wxFileConfig* config);
  void SavePilPersistentState(wxFileConfig* config) const;

  void LoadPersistentState();
  void SavePersistentState() const;
  wxFont GetEblLabelFont() const;

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

  int m_context_ebl_id{-1};
  int m_context_pil_id{-1};
  wxMenu* m_context_menu_owner{nullptr};
  wxMenuItem* m_context_ebl_item{nullptr};
  wxMenuItem* m_context_pil_item{nullptr};

  EblMode m_ebl_mode{EblMode::Idle};
  RouteWaypointHit m_pending_start;
  std::vector<RouteEblVrm> m_ebl_vrms;
  int m_edit_item_index{-1};

  PilMode m_pil_mode{PilMode::Idle};
  RouteLegHit m_pending_leg;
  std::vector<RoutePil> m_pils;
  int m_edit_pil_index{-1};

  int m_label_font_size{14};
  wxColour m_ebl_colour{210, 0, 220};
  wxColour m_pil_colour{0, 105, 125};
  double m_cursor_lat{0.0};
  double m_cursor_lon{0.0};
  bool m_has_cursor{false};
};
