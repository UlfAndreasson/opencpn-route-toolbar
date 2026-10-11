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

  actions.Add(
      _("Export complete route package..."));

  actions.Add(
      _("Import route package..."));

  wxSingleChoiceDialog dialog(
      m_dialog
          ? static_cast<wxWindow*>(m_dialog)
          : m_parent_window,
      _("Export or import a complete Route Toolbar GPX package."),
      _("Route GPX"),
      actions);

  if (dialog.ShowModal() != wxID_OK)
    return;

  if (dialog.GetSelection() == 0)
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

  wxSingleChoiceDialog route_dialog(
      m_dialog
          ? static_cast<wxWindow*>(m_dialog)
          : m_parent_window,
      _("Select the route to package."),
      _("Export Route GPX"),
      choices);

  if (route_dialog.ShowModal() != wxID_OK)
    return;

  const int selected =
      route_dialog.GetSelection();

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

  size_t ebl_count = 0;
  size_t pil_count = 0;

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
          _("GPX package saved: %lu EBL/VRM, %lu PIL."),
          static_cast<unsigned long>(
              ebl_count),
          static_cast<unsigned long>(
              pil_count)));
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

  std::unique_ptr<PlugIn_Route>
      existing_backup;

  if (existing_route) {
    existing_backup =
        GetRoute_Plugin(
            old_route_guid);

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
  // Replace route only after parsing succeeded.
  // ----------------------------------------------------------

  if (existing_route &&
      !import_as_new) {
    wxString guid =
        old_route_guid;

    DeletePlugInRoute(guid);
  }

  PlugIn_Route imported_route;

  imported_route.m_NameString =
      route_name;

  imported_route.m_GUID =
      new_route_guid;

  for (const GpxWaypoint& waypoint :
       waypoints) {
    wxString symbol =
        waypoint.symbol;

    if (symbol.IsEmpty())
      symbol = "circle";

    auto* imported_waypoint =
        new PlugIn_Waypoint(
            waypoint.lat,
            waypoint.lon,
            symbol,
            waypoint.name,
            waypoint.guid);

    imported_route.pWaypointList->Append(
        imported_waypoint);
  }

  if (!AddPlugInRoute(
          &imported_route, true)) {
    if (existing_backup)
      AddPlugInRoute(
          existing_backup.get(),
          true);

    wxMessageBox(
        _("OpenCPN could not create the imported route."),
        _("Route GPX"),
        wxOK | wxICON_ERROR,
        m_dialog
            ? static_cast<wxWindow*>(m_dialog)
            : m_parent_window);

    return;
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
  }

  size_t ebl_count = 0;
  size_t pil_count = 0;

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
    }

    for (wxXmlNode* node =
             toolbar->GetChildren();
         node;
         node = node->GetNext()) {
      const wxString type =
          LocalName(node->GetName());

      if (type == "ebl-vrm") {
        RouteEblVrm item;

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

          // Snap the imported EBL/VRM start to the exact
          // OpenCPN waypoint position rather than retaining
          // the older rounded startLat/startLon from GPX.
          if (!item.waypoint_guid.IsEmpty()) {
            PlugIn_Waypoint imported_start;

            if (GetSingleWaypoint(
                    item.waypoint_guid,
                    &imported_start)) {
              item.start_lat =
                  imported_start.m_lat;
              item.start_lon =
                  imported_start.m_lon;

              if (!imported_start.m_MarkName.IsEmpty())
                item.waypoint_name =
                    imported_start.m_MarkName;
            }
          }
        }

        if (item.start_is_mark) {
          auto mapped =
              mark_guid_map.find(
                  item.start_mark_guid);

          if (mapped !=
              mark_guid_map.end())
            item.start_mark_guid =
                mapped->second;

          RefreshEblStartFromMark(item);
        }

        RecalculateEbl(item);

        bool duplicate = false;

        for (const RouteEblVrm& existing :
             m_ebl_vrms) {
          const bool same_start =
              item.start_is_mark ==
                  existing.start_is_mark &&
              (item.start_is_mark
                   ? item.start_mark_guid ==
                         existing.start_mark_guid
                   : item.route_guid ==
                             existing.route_guid &&
                         item.waypoint_guid ==
                             existing.waypoint_guid);

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

      if (type == "pil") {
        RoutePil item;

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
          _("GPX package imported: %lu WP, %lu EBL/VRM, %lu PIL."),
          static_cast<unsigned long>(
              waypoints.size()),
          static_cast<unsigned long>(
              ebl_count),
          static_cast<unsigned long>(
              pil_count)));

  wxMessageBox(
      wxString::Format(
          _("Route '%s' imported successfully.\n\n"
            "%lu waypoints\n"
            "%lu EBL/VRM\n"
            "%lu PIL"),
          route_name,
          static_cast<unsigned long>(
              waypoints.size()),
          static_cast<unsigned long>(
              ebl_count),
          static_cast<unsigned long>(
              pil_count)),
      _("Route GPX"),
      wxOK | wxICON_INFORMATION,
      m_dialog
          ? static_cast<wxWindow*>(m_dialog)
          : m_parent_window);
}
