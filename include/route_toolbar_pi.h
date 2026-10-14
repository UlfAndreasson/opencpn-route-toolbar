// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <vector>

#include <wx/bitmap.h>
#include <wx/colour.h>
#include <wx/font.h>
#include <wx/window.h>

#include "ocpn_plugin.h"

class RouteToolbarDialog;
class RouteToolbarLabelMoveFilter;
class wxFileConfig;
class wxTimer;
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
  void BeginTurn();
  void BeginMoveLabels();
  void EndMoveLabels(bool cancel_current = false);
  bool GetMoveLabelsActive() const { return m_move_labels_active; }
  void ShowObjectManager();
  void ExportRouteReport();
  void RouteGpx();
  int ShowActionButtons(const wxString& title, const wxString& prompt,
                        const wxArrayString& labels);
  void ExportRouteGpxPackage();
  void ImportRouteGpxPackage();
  void ExportRouteCsvReport();
  void ExportRoutePdfReport();
  void CancelInteractiveTool();
  void OnDialogHidden();
  void EnsureContextMenu();
  void OnDelayedStateLoad();

  int GetLabelFontSize() const { return m_label_font_size; }
  void SetLabelFontSize(int point_size);

  // User-facing terminology is "labels". m_notes_visible is retained as the
  // internal storage name for backwards-compatible config/state handling.
  bool GetLabelsVisible() const { return m_notes_visible; }
  void SetLabelsVisible(bool visible) {
    if (m_notes_visible == visible) return;
    m_notes_visible = visible;
    SavePersistentState();
    if (m_parent_window) RequestRefresh(m_parent_window);
  }
  bool GetNotesVisible() const { return GetLabelsVisible(); }
  void SetNotesVisible(bool visible) { SetLabelsVisible(visible); }

  bool GetLegInfoVisible() const { return m_leg_info_visible; }
  void SetLegInfoVisible(bool visible);
  void LoadUiPreferences();

  const wxColour& GetEblColour() const { return m_ebl_colour; }
  const wxColour& GetPilColour() const { return m_pil_colour; }
  const wxColour& GetTurnColour() const { return m_turn_colour; }
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
  void SetTurnColour(const wxColour& colour) {
    if (!colour.IsOk()) return;
    m_turn_colour = colour;
    SavePersistentState();
    if (m_parent_window) RequestRefresh(m_parent_window);
  }

private:
  friend class RouteToolbarLabelMoveFilter;

  struct RouteWaypointHit {
    bool found{false};
    bool is_mark{false};
    bool is_turn_anchor{false};
    bool turn_anchor_is_end{false};
    bool is_free_point{false};
    double lat{0.0};
    double lon{0.0};
    double distance_nm{0.0};
    wxString route_guid;
    wxString route_name;
    wxString waypoint_guid;
    wxString waypoint_name;
  };

  struct RouteEblVrm {
    bool visible{true};
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
    bool start_is_mark{false};
    wxString start_mark_guid;
    wxString start_mark_name;
    bool start_is_turn{false};
    bool start_turn_end{false};
    bool start_is_free{false};
    wxString start_turn_route_guid;
    wxString start_turn_waypoint_guid;
    wxString start_turn_waypoint_name;
    wxString target_name;
    bool label_manual{false};
    int label_dx_px{0};
    int label_dy_px{0};
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
    bool visible{true};
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
    double offset_nm{0.0};
    int leg_index{0};
    wxString route_guid;
    wxString route_name;
    wxString from_guid;
    wxString from_name;
    wxString to_guid;
    wxString to_name;
    bool label_manual{false};
    int label_dx_px{12};
    int label_dy_px{-30};
  };

  struct RouteTurn {
    bool visible{true};
    double rudder_angle_deg{0.0};
    double waypoint_lat{0.0};
    double waypoint_lon{0.0};
    double start_lat{0.0};
    double start_lon{0.0};
    double end_lat{0.0};
    double end_lon{0.0};
    double center_lat{0.0};
    double center_lon{0.0};
    double inbound_course_true{0.0};
    double outbound_course_true{0.0};
    double inbound_leg_nm{0.0};
    double outbound_leg_nm{0.0};
    double turn_angle_deg{0.0};
    double radius_nm{0.10};
    double tangent_nm{0.0};
    wxString route_guid;
    wxString route_name;
    wxString waypoint_guid;
    wxString waypoint_name;
    wxString prev_guid;
    wxString prev_name;
    wxString next_guid;
    wxString next_name;
    bool label_manual{false};
    int label_dx_px{10};
    int label_dy_px{8};
  };

  enum class EblMode {
    Idle,
    AwaitRoutePoint,
    AwaitTarget,
    MoveExistingStart,
    MoveExistingTarget,
    MoveExistingLabel,
  };

  enum class PilMode { Idle, AwaitLeg, AwaitOffsetPoint, MoveExistingAnchor };
  enum class TurnMode { Idle, AwaitWaypoint };
  enum class LabelMoveType { None, Ebl, Pil, Turn };

  void ShowToolWindow();
  void HideToolWindow();
  void SetStatus(const wxString& text);
  void UpdateToolButtons();
  void UpdateContextMenuHit();

  RouteWaypointHit FindNearestRouteWaypoint(double lat, double lon,
                                            double max_distance_nm) const;
  RouteWaypointHit FindNearestStandaloneMark(double lat, double lon,
                                             double max_distance_nm) const;
  RouteWaypointHit FindNearestEblStart(double lat, double lon,
                                       double max_distance_nm) const;
  RouteWaypointHit FindNearestTurnAnchor(double lat, double lon,
                                         double max_distance_nm) const;
  bool RefreshEblStartFromMark(RouteEblVrm& item) const;
  bool RefreshEblStartFromTurn(RouteEblVrm& item) const;
  int FindNearestEblTarget(double lat, double lon,
                           double max_distance_nm) const;
  int FindNearestEblGeometry(double lat, double lon,
                             double max_distance_nm) const;
  void ShowEblTargetActions(int index);
  void BeginMoveEblStart(int index);
  bool CommitMoveEblStart(double start_lat, double start_lon);
  void BeginMoveEblTarget(int index);
  bool CommitMoveEblTarget(double target_lat, double target_lon);
  void BeginMoveEblLabel(int index);
  bool CommitMoveEblLabel();
  void ResetEblLabel(int index);
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

  RouteWaypointHit FindNearestInteriorRouteWaypoint(
      double lat, double lon, double max_distance_nm) const;
  bool BuildTurnFromRoute(const wxString& route_guid,
                          const wxString& waypoint_guid, double radius_nm,
                          RouteTurn& item, wxString* error) const;
  int FindTurnByWaypoint(const wxString& route_guid,
                         const wxString& waypoint_guid) const;
  void CreateOrEditTurn(const RouteWaypointHit& hit);
  void DeleteTurn(int index);
  void DrawTurn(wxDC& dc, PlugIn_ViewPort* vp, const RouteTurn& item) const;
  void DrawTurnGL(PlugIn_ViewPort* vp, const RouteTurn& item) const;
  void LoadTurnPersistentState(wxFileConfig* config);
  void SaveTurnPersistentState(wxFileConfig* config) const;

  void LoadPersistentState();
  void SavePersistentState() const;
  wxFont GetEblLabelFont() const;
  wxPoint FindSafeLabelBox(PlugIn_ViewPort* vp, const RouteEblVrm& item,
                           const wxPoint& target_px, int box_width,
                           int box_height) const;

  void DrawEblVrm(wxDC& dc, PlugIn_ViewPort* vp, const RouteEblVrm& item,
                  bool preview) const;
  void DrawEblVrmGL(PlugIn_ViewPort* vp, const RouteEblVrm& item,
                    bool preview) const;
  void DrawRouteLegLabels(wxDC& dc, PlugIn_ViewPort* vp) const;
  void DrawRouteLegLabelsGL(PlugIn_ViewPort* vp) const;
  wxColour ResolveRouteColour(const wxString& route_guid) const;
  void DrawGlText(const wxString& label, int x, int y,
                  const wxColour& colour) const;
  void DrawGlLabel(const wxString& label, int x, int y) const;
  static double NormalizeBearing(double bearing);

  void OnMoveLabelsMouse(wxMouseEvent& event);
  bool TryBeginLabelMove(const wxPoint& mouse);
  void UpdateActiveLabelPosition(const wxPoint& mouse);
  void RestoreActiveLabelPosition();
  void CommitActiveLabelPosition();
  wxRect EblLabelRect(PlugIn_ViewPort* vp, const RouteEblVrm& item) const;
  wxRect PilLabelRect(PlugIn_ViewPort* vp, const RoutePil& item) const;
  wxRect TurnLabelRect(PlugIn_ViewPort* vp, const RouteTurn& item) const;

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
  int m_pending_label_dx_px{0};
  int m_pending_label_dy_px{0};

  PilMode m_pil_mode{PilMode::Idle};
  RouteLegHit m_pending_leg;
  std::vector<RoutePil> m_pils;
  int m_edit_pil_index{-1};

  TurnMode m_turn_mode{TurnMode::Idle};
  std::vector<RouteTurn> m_turns;
  double m_default_turn_radius_nm{0.10};

  bool m_move_labels_active{false};
  LabelMoveType m_move_label_type{LabelMoveType::None};
  int m_move_label_index{-1};
  int m_label_grab_dx_px{0};
  int m_label_grab_dy_px{0};
  bool m_move_original_manual{false};
  int m_move_original_dx_px{0};
  int m_move_original_dy_px{0};
  RouteToolbarLabelMoveFilter* m_label_move_filter{nullptr};
  // OpenCPN owns the viewport passed to render callbacks. Keep an owned copy
  // for mouse hit-testing between render callbacks instead of retaining a
  // pointer to OpenCPN's temporary viewport object.
  mutable PlugIn_ViewPort m_last_viewport_storage;
  mutable PlugIn_ViewPort* m_last_viewport{nullptr};

  wxTimer* m_delayed_state_load{nullptr};
  int m_label_font_size{14};
  bool m_notes_visible{true};
  bool m_leg_info_visible{true};
  bool m_ui_preferences_loaded{false};
  wxColour m_ebl_colour{210, 0, 220};
  wxColour m_pil_colour{0, 105, 125};
  wxColour m_turn_colour{255, 125, 0};
  double m_cursor_lat{0.0};
  double m_cursor_lon{0.0};
  bool m_has_cursor{false};

  // Preserve where the chart context menu was opened. The cursor may move
  // while the user selects an item from the popup/action dialog.
  double m_context_click_lat{0.0};
  double m_context_click_lon{0.0};
  bool m_has_context_click{false};
  bool m_state_loaded{false};
};
