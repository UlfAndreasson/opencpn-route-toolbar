// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <wx/bitmap.h>
#include <wx/window.h>

#include "ocpn_plugin.h"

class RouteToolbarDialog;

class route_toolbar_pi final : public opencpn_plugin_118 {
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

  void OnDialogHidden();

private:
  void ShowToolWindow();
  void HideToolWindow();

  wxWindow* m_parent_window{nullptr};
  RouteToolbarDialog* m_dialog{nullptr};
  int m_toolbar_id{-1};
  bool m_dialog_visible{false};
  wxBitmap m_toolbar_bitmap;
};
