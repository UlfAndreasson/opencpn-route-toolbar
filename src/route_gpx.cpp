// SPDX-License-Identifier: GPL-2.0-or-later

#include "route_toolbar_pi.h"
#include "route_toolbar_dialog.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <set>
#include <vector>

#include <wx/choicdlg.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/xml/xml.h>

namespace {

constexpr const char* kRtNamespace =
    "https://nubisor.se/opencpn/route-toolbar/1";

wxString LocalName(const wxString& name) {
  const int pos = name.Find(':');
  return pos == wxNOT_FOUND ? name : name.Mid(pos + 1);
}

wxXmlNode* Child(wxXmlNode* parent, const wxString& local_name) {
  if (!parent) return nullptr;

  for (wxXmlNode* node = parent->GetChildren();
       node;
       node = node->GetNext()) {
    if (node->GetType() == wxXML_ELEMENT_NODE &&
        LocalName(node->GetName()) == local_name)
      return node;
  }

  return nullptr;
}

wxString Text(wxXmlNode* node) {
  if (!node) return wxEmptyString;

  wxString result;

  for (wxXmlNode* child = node->GetChildren();
       child;
       child = child->GetNext()) {
    if (child->GetType() == wxXML_TEXT_NODE)
      result += child->GetContent();
  }

  return result;
}

wxXmlNode* AddElement(wxXmlNode* parent,
                      const wxString& name) {
  auto* node =
      new wxXmlNode(wxXML_ELEMENT_NODE, name);

  parent->AddChild(node);
  return node;
}

wxXmlNode* AddText(wxXmlNode* parent,
                   const wxString& name,
                   const wxString& value) {
  wxXmlNode* node = AddElement(parent, name);

  node->AddChild(
      new wxXmlNode(wxXML_TEXT_NODE,
                    wxEmptyString,
                    value));

  return node;
}

wxString DoubleText(double value,
                    int decimals = 10) {
  wxString text =
      wxString::Format("%.*f", decimals, value);

  // GPX/XML always uses decimal point.
  text.Replace(",", ".");
  return text;
}

double AttrDouble(wxXmlNode* node,
                  const wxString& name,
                  double default_value = 0.0) {
  if (!node) return default_value;

  wxString value =
      node->GetAttribute(name, wxEmptyString);

  if (value.IsEmpty())
    return default_value;

  value.Replace(",", ".");

  double result = default_value;

  if (!value.ToDouble(&result))
    return default_value;

  return result;
}

long AttrLong(wxXmlNode* node,
              const wxString& name,
              long default_value = 0) {
  if (!node) return default_value;

  long result = default_value;

  if (!node->GetAttribute(name, wxEmptyString)
           .ToLong(&result))
    return default_value;

  return result;
}

bool AttrBool(wxXmlNode* node,
              const wxString& name,
              bool default_value = false) {
  if (!node) return default_value;

  wxString value =
      node->GetAttribute(name, wxEmptyString)
          .Lower();

  if (value.IsEmpty())
    return default_value;

  return value == "1" ||
         value == "true" ||
         value == "yes";
}

wxString ColourText(const wxColour& colour) {
  return wxString::Format(
      "#%02X%02X%02X",
      colour.Red(),
      colour.Green(),
      colour.Blue());
}

wxString SafeFilename(wxString name) {
  if (name.IsEmpty())
    name = "route";

  static const wxString invalid =
      "\\/:*?\"<>|";

  for (wxUniChar ch : invalid)
    name.Replace(wxString(ch), "-");

  name.Trim(true).Trim(false);

  if (name.IsEmpty())
    name = "route";

  return name;
}

wxString NewGuid(const wxString& prefix) {
  static std::mt19937_64 generator(
      std::random_device{}());

  const auto random_value = generator();

  const long long ticks =
      wxGetUTCTimeMillis().GetValue();

  return wxString::Format(
      "%s-%llX-%llX",
      prefix,
      static_cast<unsigned long long>(ticks),
      static_cast<unsigned long long>(random_value));
}

struct GpxWaypoint {
  double lat{0.0};
  double lon{0.0};

  wxString name;
  wxString symbol;
  wxString guid;
};

struct GpxMark {
  double lat{0.0};
  double lon{0.0};

  wxString name;
  wxString symbol;
  wxString guid;
};

wxString ExtensionGuid(wxXmlNode* parent,
                       const wxString& child_name) {
  wxXmlNode* extensions =
      Child(parent, "extensions");

  if (!extensions)
    return wxEmptyString;

  for (wxXmlNode* node = extensions->GetChildren();
       node;
       node = node->GetNext()) {
    if (LocalName(node->GetName()) ==
        child_name)
      return node->GetAttribute(
          "guid", wxEmptyString);
  }

  return wxEmptyString;
}

}  // namespace


void route_toolbar_pi::RouteGpx() {
  CancelInteractiveTool();

  wxArrayString actions;
  actions.Add(_("Export complete route package..."));
  actions.Add(_("Import route package..."));

  const int selected = ShowActionButtons(
      _("Route GPX"),
      _("What do you want to do with Route Toolbar GPX?"),
      actions);
  if (selected < 0) return;

  if (selected == 0)
    ExportRouteGpxPackage();
  else
    ImportRouteGpxPackage();
}


void route_toolbar_pi::ExportRouteGpxPackage() {
  const wxArrayString route_guids =
      GetRouteGUIDArray();

  if (route_guids.IsEmpty()) {
    wxMessageBox(
        _("There are no OpenCPN routes to export."),
        _("Route GPX"),
        wxOK | wxICON_INFORMATION,
        m_dialog
            ? static_cast<wxWindow*>(m_dialog)
            : m_parent_window);

    return;
  }

  wxArrayString choices;
  std::vector<size_t> route_indices;

  for (size_t i = 0;
       i < route_guids.GetCount();
       ++i) {
    auto route =
        GetRoute_Plugin(route_guids[i]);

    if (!route ||
        !route->pWaypointList)
      continue;

    wxString name =
        route->m_NameString;

    if (name.IsEmpty())
      name = _("Unnamed route");

    choices.Add(name);
    route_indices.push_back(i);
  }

  if (choices.IsEmpty())
    return;

  const int selected = ShowActionButtons(
      _("Export Route GPX"),
      _("Select the route to package."),
      choices);

  if (selected < 0)
    return;

  if (selected < 0 ||
      static_cast<size_t>(selected) >=
          route_indices.size())
    return;

  const wxString route_guid =
      route_guids[
          route_indices[
              static_cast<size_t>(selected)]];

  auto route =
      GetRoute_Plugin(route_guid);

  if (!route ||
      !route->pWaypointList)
    return;

  wxString route_name =
      route->m_NameString;

  route_name.Trim(true).Trim(false);

  if (route_name.IsEmpty())
    route_name = _("Unnamed route");

  wxFileDialog save_dialog(
      m_dialog
          ? static_cast<wxWindow*>(m_dialog)
          : m_parent_window,
      _("Save complete Route Toolbar GPX"),
      wxEmptyString,
      SafeFilename(route_name) +
          "-route-toolbar.gpx",
      _("GPX files (*.gpx)|*.gpx"),
      wxFD_SAVE |
          wxFD_OVERWRITE_PROMPT);

  if (save_dialog.ShowModal() != wxID_OK)
    return;

  wxXmlDocument document;

  auto* root =
      new wxXmlNode(
          wxXML_ELEMENT_NODE,
          "gpx");

  root->AddAttribute(
      "version", "1.1");

  root->AddAttribute(
      "creator",
      "OpenCPN Route Toolbar");

  root->AddAttribute(
      "xmlns",
      "http://www.topografix.com/GPX/1/1");

  root->AddAttribute(
      "xmlns:rt",
      kRtNamespace);

  document.SetRoot(root);

  // ----------------------------------------------------------
  // Export standalone Marks referenced by mark-based EBLs.
  // ----------------------------------------------------------

  std::set<wxString> exported_marks;

  for (const RouteEblVrm& item :
       m_ebl_vrms) {
    if (!item.start_is_mark)
      continue;

    wxString guid =
        item.start_mark_guid;

    if (guid.IsEmpty() ||
        exported_marks.count(guid))
      continue;

    PlugIn_Waypoint mark;

    bool found =
        GetSingleWaypoint(
            guid, &mark);

    const double lat =
        found
            ? mark.m_lat
            : item.start_lat;

    const double lon =
        found
            ? mark.m_lon
            : item.start_lon;

    wxString name =
        found
            ? mark.m_MarkName
            : item.start_mark_name;

    wxString symbol =
        found
            ? mark.m_IconName
            : "diamond";

    auto* wpt =
        AddElement(root, "wpt");

    wpt->AddAttribute(
        "lat",
        DoubleText(lat));

    wpt->AddAttribute(
        "lon",
        DoubleText(lon));

    AddText(wpt, "name", name);

    if (!symbol.IsEmpty())
      AddText(wpt, "sym", symbol);

    auto* extensions =
        AddElement(wpt, "extensions");

    auto* mark_extension =
        AddElement(
            extensions,
            "rt:mark");

    mark_extension->AddAttribute(
        "guid", guid);

    exported_marks.insert(guid);
  }

  // ----------------------------------------------------------
  // Standard GPX route.
  // ----------------------------------------------------------

  auto* rte =
      AddElement(root, "rte");

  AddText(
      rte, "name", route_name);

  auto* route_extensions =
      AddElement(rte, "extensions");

  auto* route_extension =
      AddElement(
          route_extensions,
          "rt:route");

  route_extension->AddAttribute(
      "guid", route_guid);

  for (auto node =
           route->pWaypointList->GetFirst();
       node;
       node = node->GetNext()) {
    PlugIn_Waypoint* waypoint =
        node->GetData();

    if (!waypoint)
      continue;

    auto* rtept =
        AddElement(rte, "rtept");

    rtept->AddAttribute(
        "lat",
        DoubleText(waypoint->m_lat));

    rtept->AddAttribute(
        "lon",
        DoubleText(waypoint->m_lon));

    AddText(
        rtept,
        "name",
        waypoint->m_MarkName);

    if (!waypoint->m_IconName.IsEmpty())
      AddText(
          rtept,
          "sym",
          waypoint->m_IconName);

    auto* extensions =
        AddElement(
            rtept,
            "extensions");

    auto* wp_extension =
        AddElement(
            extensions,
            "rt:waypoint");

    wp_extension->AddAttribute(
        "guid",
        waypoint->m_GUID);
  }

  // ----------------------------------------------------------
  // Route Toolbar extensions.
  // ----------------------------------------------------------

  auto* root_extensions =
      AddElement(
          root, "extensions");

  auto* toolbar =
      AddElement(
          root_extensions,
          "rt:route-toolbar");

  toolbar->AddAttribute(
      "version", "1");

  toolbar->AddAttribute(
      "routeGuid", route_guid);

  toolbar->AddAttribute(
      "routeName", route_name);

  auto* settings =
      AddElement(
          toolbar,
          "rt:settings");

  settings->AddAttribute(
      "labelFontSize",
      wxString::Format(
          "%d",
          m_label_font_size));

  settings->AddAttribute(
      "eblColour",
      ColourText(m_ebl_colour));

  settings->AddAttribute(
      "pilColour",
      ColourText(m_pil_colour));

  settings->AddAttribute(
      "turnColour",
      ColourText(m_turn_colour));

  size_t ebl_count = 0;
  size_t pil_count = 0;
  size_t turn_count = 0;

  for (const RouteEblVrm& item :
       m_ebl_vrms) {
    const bool same_route =
        !item.start_is_mark &&
        (item.route_guid == route_guid ||
         (item.route_guid.IsEmpty() &&
          item.route_name == route_name));

    if (!same_route &&
        !item.start_is_mark)
      continue;

    auto* ebl =
        AddElement(
            toolbar,
            "rt:ebl-vrm");

    ebl->AddAttribute(
        "visible",
        item.visible ? "1" : "0");

    ebl->AddAttribute(
        "startLat",
        DoubleText(item.start_lat));

    ebl->AddAttribute(
        "startLon",
        DoubleText(item.start_lon));

    ebl->AddAttribute(
        "targetLat",
        DoubleText(item.target_lat));

    ebl->AddAttribute(
        "targetLon",
        DoubleText(item.target_lon));

    ebl->AddAttribute(
        "bearingTrue",
        DoubleText(
            item.bearing_true, 6));

    ebl->AddAttribute(
        "rangeNm",
        DoubleText(
            item.range_nm, 6));

    ebl->AddAttribute(
        "arcWidthDeg",
        DoubleText(
            item.arc_width_deg, 3));

    ebl->AddAttribute(
        "routeGuid",
        item.route_guid);

    ebl->AddAttribute(
        "routeName",
        item.route_name);

    ebl->AddAttribute(
        "waypointGuid",
        item.waypoint_guid);

    ebl->AddAttribute(
        "waypointName",
        item.waypoint_name);

    ebl->AddAttribute(
        "startIsMark",
        item.start_is_mark
            ? "1"
            : "0");

    ebl->AddAttribute(
        "startMarkGuid",
        item.start_mark_guid);

    ebl->AddAttribute(
        "startMarkName",
        item.start_mark_name);

    ebl->AddAttribute(
        "startIsTurn",
        item.start_is_turn ? "1" : "0");

    ebl->AddAttribute(
        "startTurnEnd",
        item.start_turn_end ? "1" : "0");

    ebl->AddAttribute(
        "startIsFree",
        item.start_is_free ? "1" : "0");

    ebl->AddAttribute(
        "startTurnRouteGuid",
        item.start_turn_route_guid);

    ebl->AddAttribute(
        "startTurnWaypointGuid",
        item.start_turn_waypoint_guid);

    ebl->AddAttribute(
        "startTurnWaypointName",
        item.start_turn_waypoint_name);

    ebl->AddAttribute(
        "targetName",
        item.target_name);

    ebl->AddAttribute(
        "labelManual",
        item.label_manual
            ? "1"
            : "0");

    ebl->AddAttribute(
        "labelDxPx",
        wxString::Format(
            "%d",
            item.label_dx_px));

    ebl->AddAttribute(
        "labelDyPx",
        wxString::Format(
            "%d",
            item.label_dy_px));

    ++ebl_count;
  }

  for (const RouteTurn& item :
       m_turns) {
    const bool same_route =
        item.route_guid == route_guid ||
        (item.route_guid.IsEmpty() &&
         item.route_name == route_name);

    if (!same_route)
      continue;

    auto* turn =
        AddElement(toolbar, "rt:turn");

    turn->AddAttribute("visible", item.visible ? "1" : "0");
    turn->AddAttribute("rudderAngleDeg", DoubleText(item.rudder_angle_deg, 3));
    turn->AddAttribute("routeGuid", item.route_guid);
    turn->AddAttribute("routeName", item.route_name);
    turn->AddAttribute("waypointGuid", item.waypoint_guid);
    turn->AddAttribute("waypointName", item.waypoint_name);
    turn->AddAttribute("radiusNm", DoubleText(item.radius_nm, 6));
    turn->AddAttribute("turnAngleDeg", DoubleText(item.turn_angle_deg, 6));
    turn->AddAttribute("tangentNm", DoubleText(item.tangent_nm, 6));
    turn->AddAttribute("startLat", DoubleText(item.start_lat));
    turn->AddAttribute("startLon", DoubleText(item.start_lon));
    turn->AddAttribute("endLat", DoubleText(item.end_lat));
    turn->AddAttribute("endLon", DoubleText(item.end_lon));

    ++turn_count;
  }

  for (const RoutePil& item :
       m_pils) {
    const bool same_route =
        item.route_guid == route_guid ||
        (item.route_guid.IsEmpty() &&
         item.route_name == route_name);

    if (!same_route)
      continue;

    auto* pil =
        AddElement(
            toolbar,
            "rt:pil");

    pil->AddAttribute(
        "visible",
        item.visible ? "1" : "0");

    pil->AddAttribute(
        "routeGuid",
        item.route_guid);

    pil->AddAttribute(
        "routeName",
        item.route_name);

    pil->AddAttribute(
        "legIndex",
        wxString::Format(
            "%d",
            item.leg_index));

    pil->AddAttribute(
        "fromGuid",
        item.from_guid);

    pil->AddAttribute(
        "fromName",
        item.from_name);

    pil->AddAttribute(
        "toGuid",
        item.to_guid);

    pil->AddAttribute(
        "toName",
        item.to_name);

    pil->AddAttribute(
        "offsetNm",
        DoubleText(
            item.offset_nm, 6));

    pil->AddAttribute(
        "anchorLat",
        DoubleText(
            item.anchor_lat));

    pil->AddAttribute(
        "anchorLon",
        DoubleText(
            item.anchor_lon));

    pil->AddAttribute(
        "legStartLat",
        DoubleText(
            item.leg_start_lat));

    pil->AddAttribute(
        "legStartLon",
        DoubleText(
            item.leg_start_lon));

    pil->AddAttribute(
        "legEndLat",
        DoubleText(
            item.leg_end_lat));

    pil->AddAttribute(
        "legEndLon",
        DoubleText(
            item.leg_end_lon));

    pil->AddAttribute(
        "lineStartLat",
        DoubleText(
            item.line_start_lat));

    pil->AddAttribute(
        "lineStartLon",
        DoubleText(
            item.line_start_lon));

    pil->AddAttribute(
        "lineEndLat",
        DoubleText(
            item.line_end_lat));

    pil->AddAttribute(
        "lineEndLon",
        DoubleText(
            item.line_end_lon));

    pil->AddAttribute(
        "courseTrue",
        DoubleText(
            item.course_true, 6));

    pil->AddAttribute(
        "legDistanceNm",
        DoubleText(
            item.leg_distance_nm, 6));

    ++pil_count;
  }

  if (!document.Save(
          save_dialog.GetPath())) {
    wxMessageBox(
        _("Could not save the GPX package."),
        _("Route GPX"),
        wxOK | wxICON_ERROR,
        m_dialog
            ? static_cast<wxWindow*>(m_dialog)
            : m_parent_window);

    return;
  }

  SetStatus(
      wxString::Format(
          _("GPX package saved: %lu EBL/VRM, %lu PIL, %lu TURN."),
          static_cast<unsigned long>(
              ebl_count),
          static_cast<unsigned long>(
              pil_count),
          static_cast<unsigned long>(
              turn_count)));
}


void route_toolbar_pi::ImportRouteGpxPackage() {
  wxFileDialog open_dialog(
      m_dialog
          ? static_cast<wxWindow*>(m_dialog)
          : m_parent_window,
      _("Import complete Route Toolbar GPX"),
      wxEmptyString,
      wxEmptyString,
      _("GPX files (*.gpx)|*.gpx"),
      wxFD_OPEN |
          wxFD_FILE_MUST_EXIST);

  if (open_dialog.ShowModal() != wxID_OK)
    return;

  wxXmlDocument document;

  if (!document.Load(
          open_dialog.GetPath())) {
    wxMessageBox(
        _("The GPX file could not be read."),
        _("Route GPX"),
        wxOK | wxICON_ERROR,
        m_dialog
            ? static_cast<wxWindow*>(m_dialog)
            : m_parent_window);

    return;
  }

  wxXmlNode* root =
      document.GetRoot();

  if (!root ||
      LocalName(root->GetName()) != "gpx") {
    wxMessageBox(
        _("This is not a valid GPX document."),
        _("Route GPX"),
        wxOK | wxICON_ERROR,
        m_dialog
            ? static_cast<wxWindow*>(m_dialog)
            : m_parent_window);

    return;
  }

  wxXmlNode* rte =
      Child(root, "rte");

  if (!rte) {
    wxMessageBox(
        _("The GPX package contains no route."),
        _("Route GPX"),
        wxOK | wxICON_ERROR,
        m_dialog
            ? static_cast<wxWindow*>(m_dialog)
            : m_parent_window);

    return;
  }

  wxString route_name =
      Text(Child(rte, "name"));

  route_name.Trim(true).Trim(false);

  if (route_name.IsEmpty())
    route_name = _("Imported Route");

  wxString old_route_guid =
      ExtensionGuid(rte, "route");

  std::vector<GpxWaypoint> waypoints;

  for (wxXmlNode* node =
           rte->GetChildren();
       node;
       node = node->GetNext()) {
    if (LocalName(node->GetName()) !=
        "rtept")
      continue;

    GpxWaypoint waypoint;

    waypoint.lat =
        AttrDouble(node, "lat");

    waypoint.lon =
        AttrDouble(node, "lon");

    waypoint.name =
        Text(Child(node, "name"));

    waypoint.symbol =
        Text(Child(node, "sym"));

    waypoint.guid =
        ExtensionGuid(
            node, "waypoint");

    waypoints.push_back(waypoint);
  }

  if (waypoints.empty()) {
    wxMessageBox(
        _("The route contains no waypoints."),
        _("Route GPX"),
        wxOK | wxICON_ERROR,
        m_dialog
            ? static_cast<wxWindow*>(m_dialog)
            : m_parent_window);

    return;
  }

  // ----------------------------------------------------------
  // Locate Route Toolbar extension.
  // ----------------------------------------------------------

  wxXmlNode* toolbar = nullptr;

  if (wxXmlNode* extensions =
          Child(root, "extensions")) {
    for (wxXmlNode* node =
             extensions->GetChildren();
         node;
         node = node->GetNext()) {
      if (LocalName(node->GetName()) ==
          "route-toolbar") {
        toolbar = node;
        break;
      }
    }
  }

  if (old_route_guid.IsEmpty() &&
      toolbar) {
    old_route_guid =
        toolbar->GetAttribute(
            "routeGuid",
            wxEmptyString);
  }

  // ----------------------------------------------------------
  // Determine import behaviour.
  // ----------------------------------------------------------

  bool existing_route = false;

  if (!old_route_guid.IsEmpty())
    existing_route =
        static_cast<bool>(
            GetRoute_Plugin(
                old_route_guid));

  bool import_as_new = false;

  const auto belongs_to_existing_route = [&](const wxString& guid,
                                               const wxString& name) {
    return guid == old_route_guid ||
           (guid.IsEmpty() && name == route_name);
  };

  const bool has_existing_toolbar_objects =
      existing_route &&
      (std::any_of(m_ebl_vrms.begin(), m_ebl_vrms.end(),
                   [&](const RouteEblVrm& item) {
                     return !item.start_is_mark &&
                            belongs_to_existing_route(item.route_guid,
                                                      item.route_name);
                   }) ||
       std::any_of(m_pils.begin(), m_pils.end(),
                   [&](const RoutePil& item) {
                     return belongs_to_existing_route(item.route_guid,
                                                      item.route_name);
                   }) ||
       std::any_of(m_turns.begin(), m_turns.end(),
                   [&](const RouteTurn& item) {
                     return belongs_to_existing_route(item.route_guid,
                                                      item.route_name);
                   }));

  if (existing_route && has_existing_toolbar_objects) {
    wxMessageDialog conflict_dialog(
        m_dialog
            ? static_cast<wxWindow*>(m_dialog)
            : m_parent_window,
        wxString::Format(
            _("Route '%s' already exists.\n\n"
              "Replace its Route Toolbar package, "
              "or import this GPX as a new copy?"),
            route_name),
        _("Route GPX"),
        wxYES_NO |
            wxCANCEL |
            wxICON_QUESTION);

    conflict_dialog.SetYesNoLabels(
        _("Replace existing"),
        _("Import as new"));

    const int answer =
        conflict_dialog.ShowModal();

    if (answer == wxID_CANCEL)
      return;

    import_as_new =
        answer == wxID_NO;
  }

  const bool reuse_existing_route =
      existing_route && !import_as_new;

  wxString new_route_guid =
      old_route_guid;

  if (new_route_guid.IsEmpty() ||
      import_as_new)
    new_route_guid =
        NewGuid("RT-ROUTE");

  std::map<wxString, wxString>
      waypoint_guid_map;

  for (GpxWaypoint& waypoint :
       waypoints) {
    const wxString old_guid =
        waypoint.guid;

    if (waypoint.guid.IsEmpty() ||
        import_as_new)
      waypoint.guid =
          NewGuid("RT-WP");

    if (!old_guid.IsEmpty())
      waypoint_guid_map[
          old_guid] =
              waypoint.guid;
  }

  if (reuse_existing_route) {
    auto current_route = GetRoute_Plugin(old_route_guid);
    if (current_route && current_route->pWaypointList) {
      size_t index = 0;
      for (auto node = current_route->pWaypointList->GetFirst();
           node && index < waypoints.size();
           node = node->GetNext(), ++index) {
        PlugIn_Waypoint* current_wp = node->GetData();
        if (!current_wp) continue;
        const wxString package_guid = waypoints[index].guid;
        if (!package_guid.IsEmpty())
          waypoint_guid_map[package_guid] = current_wp->m_GUID;
        waypoints[index].guid = current_wp->m_GUID;
      }
    }
  }

  // ----------------------------------------------------------
  // Parse/add referenced standalone Marks.
  // ----------------------------------------------------------

  std::map<wxString, wxString>
      mark_guid_map;

  for (wxXmlNode* node =
           root->GetChildren();
       node;
       node = node->GetNext()) {
    if (LocalName(node->GetName()) !=
        "wpt")
      continue;

    wxString old_guid =
        ExtensionGuid(
            node, "mark");

    if (old_guid.IsEmpty())
      continue;

    wxString actual_guid =
        old_guid;

    PlugIn_Waypoint existing_mark;

    wxString lookup_guid =
        actual_guid;

    const bool mark_exists =
        GetSingleWaypoint(
            lookup_guid,
            &existing_mark);

    if (!mark_exists) {
      const double lat =
          AttrDouble(node, "lat");

      const double lon =
          AttrDouble(node, "lon");

      wxString name =
          Text(Child(node, "name"));

      wxString symbol =
          Text(Child(node, "sym"));

      if (symbol.IsEmpty())
        symbol = "diamond";

      PlugIn_Waypoint mark(
          lat,
          lon,
          symbol,
          name,
          actual_guid);

      if (!AddSingleWaypoint(
              &mark, true)) {
        // Avoid breaking entire route import just because one
        // optional referenced Mark could not be added.
      }
    }

    mark_guid_map[
        old_guid] =
            actual_guid;
  }

  // ----------------------------------------------------------
  // OpenCPN owns the route. Reuse an existing route unchanged; create the
  // GPX route only when it does not exist or the user chose Import as new.
  // ----------------------------------------------------------

  if (!reuse_existing_route) {
    PlugIn_Route imported_route;

    imported_route.m_NameString = route_name;
    imported_route.m_GUID = new_route_guid;

    for (const GpxWaypoint& waypoint : waypoints) {
      wxString symbol = waypoint.symbol;
      if (symbol.IsEmpty()) symbol = "circle";

      auto* imported_waypoint = new PlugIn_Waypoint(
          waypoint.lat, waypoint.lon, symbol, waypoint.name, waypoint.guid);
      imported_route.pWaypointList->Append(imported_waypoint);
    }

    if (!AddPlugInRoute(&imported_route, true)) {
      wxMessageBox(
          _("OpenCPN could not create the imported route."),
          _("Route GPX"),
          wxOK | wxICON_ERROR,
          m_dialog ? static_cast<wxWindow*>(m_dialog) : m_parent_window);
      return;
    }
  }

  // ----------------------------------------------------------
  // Replace Route Toolbar objects for existing package.
  // ----------------------------------------------------------

  if (existing_route &&
      !import_as_new) {
    m_ebl_vrms.erase(
        std::remove_if(
            m_ebl_vrms.begin(),
            m_ebl_vrms.end(),
            [&](const RouteEblVrm& item) {
              return !item.start_is_mark &&
                     (item.route_guid ==
                          old_route_guid ||
                      (item.route_guid.IsEmpty() &&
                       item.route_name ==
                           route_name));
            }),
        m_ebl_vrms.end());

    m_pils.erase(
        std::remove_if(
            m_pils.begin(),
            m_pils.end(),
            [&](const RoutePil& item) {
              return item.route_guid ==
                         old_route_guid ||
                     (item.route_guid.IsEmpty() &&
                      item.route_name ==
                          route_name);
            }),
        m_pils.end());

    m_turns.erase(
        std::remove_if(
            m_turns.begin(),
            m_turns.end(),
            [&](const RouteTurn& item) {
              return item.route_guid ==
                         old_route_guid ||
                     (item.route_guid.IsEmpty() &&
                      item.route_name ==
                          route_name);
            }),
        m_turns.end());
  }

  size_t ebl_count = 0;
  size_t pil_count = 0;
  size_t turn_count = 0;

  // ----------------------------------------------------------
  // Restore settings and Route Toolbar objects.
  // ----------------------------------------------------------

  if (toolbar) {
    if (wxXmlNode* settings =
            Child(toolbar, "settings")) {
      const long font_size =
          AttrLong(
              settings,
              "labelFontSize",
              m_label_font_size);

      m_label_font_size =
          std::clamp(
              static_cast<int>(font_size),
              8,
              30);

      wxColour imported_ebl(
          settings->GetAttribute(
              "eblColour",
              wxEmptyString));

      if (imported_ebl.IsOk())
        m_ebl_colour =
            imported_ebl;

      wxColour imported_pil(
          settings->GetAttribute(
              "pilColour",
              wxEmptyString));

      if (imported_pil.IsOk())
        m_pil_colour =
            imported_pil;

      wxColour imported_turn(
          settings->GetAttribute(
              "turnColour",
              wxEmptyString));

      if (imported_turn.IsOk())
        m_turn_colour =
            imported_turn;
    }

    for (wxXmlNode* node =
             toolbar->GetChildren();
         node;
         node = node->GetNext()) {
      const wxString type =
          LocalName(node->GetName());

      if (type == "ebl-vrm") {
        RouteEblVrm item;

        item.visible =
            AttrBool(node, "visible", true);

        item.start_lat =
            AttrDouble(
                node, "startLat");

        item.start_lon =
            AttrDouble(
                node, "startLon");

        item.target_lat =
            AttrDouble(
                node, "targetLat");

        item.target_lon =
            AttrDouble(
                node, "targetLon");

        item.arc_width_deg =
            AttrDouble(
                node,
                "arcWidthDeg",
                25.0);

        item.route_guid =
            node->GetAttribute(
                "routeGuid",
                wxEmptyString);

        item.route_name =
            node->GetAttribute(
                "routeName",
                wxEmptyString);

        item.waypoint_guid =
            node->GetAttribute(
                "waypointGuid",
                wxEmptyString);

        item.waypoint_name =
            node->GetAttribute(
                "waypointName",
                wxEmptyString);

        item.start_is_mark =
            AttrBool(
                node,
                "startIsMark");

        item.start_mark_guid =
            node->GetAttribute(
                "startMarkGuid",
                wxEmptyString);

        item.start_mark_name =
            node->GetAttribute(
                "startMarkName",
                wxEmptyString);

        item.start_is_turn =
            AttrBool(node, "startIsTurn");

        item.start_turn_end =
            AttrBool(node, "startTurnEnd");

        item.start_is_free =
            AttrBool(node, "startIsFree");

        item.start_turn_route_guid =
            node->GetAttribute(
                "startTurnRouteGuid",
                wxEmptyString);

        item.start_turn_waypoint_guid =
            node->GetAttribute(
                "startTurnWaypointGuid",
                wxEmptyString);

        item.start_turn_waypoint_name =
            node->GetAttribute(
                "startTurnWaypointName",
                wxEmptyString);

        item.target_name =
            node->GetAttribute(
                "targetName",
                wxEmptyString);

        item.label_manual =
            AttrBool(
                node,
                "labelManual");

        item.label_dx_px =
            static_cast<int>(
                AttrLong(
                    node,
                    "labelDxPx"));

        item.label_dy_px =
            static_cast<int>(
                AttrLong(
                    node,
                    "labelDyPx"));

        if (!item.start_is_mark &&
            (item.route_guid ==
                 old_route_guid ||
             item.route_guid.IsEmpty())) {
          item.route_guid =
              new_route_guid;

          item.route_name =
              route_name;

          auto mapped =
              waypoint_guid_map.find(
                  item.waypoint_guid);

          if (mapped !=
              waypoint_guid_map.end())
            item.waypoint_guid =
                mapped->second;

          // Preserve the exact stored EBL/VRM start coordinate.
          // Route/WP identity is metadata only and does not move the start.
        }

        if (item.start_is_turn) {
          if (item.start_turn_route_guid == old_route_guid ||
              item.start_turn_route_guid.IsEmpty())
            item.start_turn_route_guid = new_route_guid;

          auto mapped_turn_wp =
              waypoint_guid_map.find(
                  item.start_turn_waypoint_guid);

          if (mapped_turn_wp != waypoint_guid_map.end())
            item.start_turn_waypoint_guid =
                mapped_turn_wp->second;

          // General route/waypoint identity follows the anchor too.
          item.route_guid = item.start_turn_route_guid;
          item.waypoint_guid = item.start_turn_waypoint_guid;
        }

        if (item.start_is_mark) {
          auto mapped =
              mark_guid_map.find(
                  item.start_mark_guid);

          if (mapped !=
              mark_guid_map.end())
            item.start_mark_guid =
                mapped->second;

        }

        RecalculateEbl(item);

        bool duplicate = false;

        for (const RouteEblVrm& existing :
             m_ebl_vrms) {
          bool same_start = false;

          if (item.start_is_mark || existing.start_is_mark) {
            same_start =
                item.start_is_mark == existing.start_is_mark &&
                item.start_mark_guid == existing.start_mark_guid;
          } else if (item.start_is_turn || existing.start_is_turn) {
            same_start =
                item.start_is_turn == existing.start_is_turn &&
                item.start_turn_end == existing.start_turn_end &&
                item.start_turn_route_guid ==
                    existing.start_turn_route_guid &&
                item.start_turn_waypoint_guid ==
                    existing.start_turn_waypoint_guid;
          } else if (item.start_is_free || existing.start_is_free) {
            same_start =
                item.start_is_free == existing.start_is_free &&
                std::fabs(item.start_lat - existing.start_lat) < 1e-7 &&
                std::fabs(item.start_lon - existing.start_lon) < 1e-7;
          } else {
            same_start =
                item.route_guid == existing.route_guid &&
                item.waypoint_guid == existing.waypoint_guid;
          }

          const bool same_target =
              std::fabs(
                  item.target_lat -
                  existing.target_lat) <
                  1e-7 &&
              std::fabs(
                  item.target_lon -
                  existing.target_lon) <
                  1e-7;

          if (same_start &&
              same_target &&
              item.target_name ==
                  existing.target_name) {
            duplicate = true;
            break;
          }
        }

        if (!duplicate) {
          m_ebl_vrms.push_back(item);
          ++ebl_count;
        }
      }

      if (type == "turn") {
        wxString turn_route_guid =
            node->GetAttribute(
                "routeGuid",
                wxEmptyString);

        wxString turn_waypoint_guid =
            node->GetAttribute(
                "waypointGuid",
                wxEmptyString);

        const double radius_nm =
            AttrDouble(node, "radiusNm", 0.10);

        const bool turn_visible =
            AttrBool(node, "visible", true);

        const double rudder_angle_deg =
            AttrDouble(node, "rudderAngleDeg", 0.0);

        if (turn_route_guid == old_route_guid ||
            turn_route_guid.IsEmpty())
          turn_route_guid = new_route_guid;

        auto mapped_turn_wp =
            waypoint_guid_map.find(
                turn_waypoint_guid);

        if (mapped_turn_wp != waypoint_guid_map.end())
          turn_waypoint_guid =
              mapped_turn_wp->second;

        RouteTurn turn;
        wxString turn_error;

        if (BuildTurnFromRoute(
                turn_route_guid,
                turn_waypoint_guid,
                radius_nm,
                turn,
                &turn_error)) {
          bool duplicate_turn = false;

          for (const RouteTurn& existing :
               m_turns) {
            if (existing.route_guid == turn.route_guid &&
                existing.waypoint_guid == turn.waypoint_guid) {
              duplicate_turn = true;
              break;
            }
          }

          if (!duplicate_turn) {
            turn.visible = turn_visible;
            turn.rudder_angle_deg =
                std::clamp(rudder_angle_deg, 0.0, 45.0);
            m_turns.push_back(turn);
            ++turn_count;
          }
        }
      }

      if (type == "pil") {
        RoutePil item;

        item.visible =
            AttrBool(node, "visible", true);

        item.route_guid =
            node->GetAttribute(
                "routeGuid",
                wxEmptyString);

        item.route_name =
            node->GetAttribute(
                "routeName",
                wxEmptyString);

        item.leg_index =
            static_cast<int>(
                AttrLong(
                    node,
                    "legIndex"));

        item.from_guid =
            node->GetAttribute(
                "fromGuid",
                wxEmptyString);

        item.from_name =
            node->GetAttribute(
                "fromName",
                wxEmptyString);

        item.to_guid =
            node->GetAttribute(
                "toGuid",
                wxEmptyString);

        item.to_name =
            node->GetAttribute(
                "toName",
                wxEmptyString);

        item.offset_nm =
            AttrDouble(
                node,
                "offsetNm");

        item.anchor_lat =
            AttrDouble(
                node,
                "anchorLat");

        item.anchor_lon =
            AttrDouble(
                node,
                "anchorLon");

        item.leg_start_lat =
            AttrDouble(
                node,
                "legStartLat");

        item.leg_start_lon =
            AttrDouble(
                node,
                "legStartLon");

        item.leg_end_lat =
            AttrDouble(
                node,
                "legEndLat");

        item.leg_end_lon =
            AttrDouble(
                node,
                "legEndLon");

        item.line_start_lat =
            AttrDouble(
                node,
                "lineStartLat");

        item.line_start_lon =
            AttrDouble(
                node,
                "lineStartLon");

        item.line_end_lat =
            AttrDouble(
                node,
                "lineEndLat");

        item.line_end_lon =
            AttrDouble(
                node,
                "lineEndLon");

        item.course_true =
            AttrDouble(
                node,
                "courseTrue");

        item.leg_distance_nm =
            AttrDouble(
                node,
                "legDistanceNm");

        if (item.route_guid ==
                old_route_guid ||
            item.route_guid.IsEmpty()) {
          item.route_guid =
              new_route_guid;

          item.route_name =
              route_name;

          auto from =
              waypoint_guid_map.find(
                  item.from_guid);

          if (from !=
              waypoint_guid_map.end())
            item.from_guid =
                from->second;

          auto to =
              waypoint_guid_map.find(
                  item.to_guid);

          if (to !=
              waypoint_guid_map.end())
            item.to_guid =
                to->second;
        }

        bool duplicate = false;

        for (const RoutePil& existing :
             m_pils) {
          if (existing.route_guid ==
                  item.route_guid &&
              existing.leg_index ==
                  item.leg_index &&
              std::fabs(
                  existing.offset_nm -
                  item.offset_nm) <
                  1e-6) {
            duplicate = true;
            break;
          }
        }

        if (!duplicate) {
          m_pils.push_back(item);
          ++pil_count;
        }
      }
    }
  }

  SavePersistentState();

  if (m_parent_window)
    RequestRefresh(m_parent_window);

  SetStatus(
      wxString::Format(
          _("GPX package imported: %lu WP, %lu EBL/VRM, %lu PIL, %lu TURN."),
          static_cast<unsigned long>(
              waypoints.size()),
          static_cast<unsigned long>(
              ebl_count),
          static_cast<unsigned long>(
              pil_count),
          static_cast<unsigned long>(
              turn_count)));

  wxMessageBox(
      wxString::Format(
          _("Route '%s' imported successfully.\n\n"
            "%lu waypoints\n"
            "%lu EBL/VRM\n"
            "%lu PIL\n"
            "%lu TURN"),
          route_name,
          static_cast<unsigned long>(
              waypoints.size()),
          static_cast<unsigned long>(
              ebl_count),
          static_cast<unsigned long>(
              pil_count),
          static_cast<unsigned long>(
              turn_count)),
      _("Route GPX"),
      wxOK | wxICON_INFORMATION,
      m_dialog
          ? static_cast<wxWindow*>(m_dialog)
          : m_parent_window);
}
