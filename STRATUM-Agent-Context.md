# STRATUM Desktop — Agent Handoff Context

Paste this file (or attach it) at the start of a new chat so the next agent
picks up where we left off.

---

## 1. Project scope

STRATUM is a **QGroundControl fork** (Qt 6 QML + C++) built on Windows.

- Primary working tree: `Dropper/STRATUM-Desktop/qgroundcontrol/`
- Mirror target (pending): `Dagger/STRATUM-Desktop/qgroundcontrol/`
- Build / install docs at the STRATUM-Desktop root:
  `BUILD-Windows.md`, `INSTALL-Windows.md`, `check-build-env.ps1`.
- Style / UI notes: `UI-changes.md`, `FlyView_Override_Surface_Map.md`.
- End-user guide (with screenshots): `STRATUM-User-Guide.md`,
  `STRATUM-User-Guide.pdf`, `STRATUM-User-Guide.docx`
  (built by `build-user-guide-pdf.py` / `build-user-guide-docx.py`).

The shell is **PowerShell**. Never chain commands with `&&`; use `;`.

---

## 2. STRATUM design system

| Purpose | Value |
|---|---|
| Ribbon background | `#1B2228` |
| Engagement ribbon | `#DC2626` |
| Cyan accent | `#48D6FF` |
| Ribbon text | `#F1F4F7` — bound as `qgcPal.text` on the dark palette |
| Dark accent text | `#001622` |

**Rule:** never hard-code `#000000` / `"black"` on ribbon-hosted text.
Always bind to `qgcPal.text` (or a property whose value is `qgcPal.text`).

---

## 3. Cumulative UI changes

### 3.1 Top ribbon (`src/Toolbar/`)

**Arm chip — Ready / Not Ready / Armed / Flying** — `MainStatusIndicator.qml`
- Tightened root `RowLayout` spacing so the chip hugs the label + chevron span.
- Single `MouseArea` anchored to the full chip
  (`left: label.left`, `right: chevron.right`, `top/bottom: parent`,
  `-0.35em` L/R, `0.15em` T/B, `radius 0.3em`, `z: 10`,
  `acceptedButtons: Qt.LeftButton`, `onClicked: dropMainStatusIndicator()`).
- Removed inner `QGCMouseArea`s and the chevron negative-margin hack.

**GPS accuracy readout — HAcc / VAcc / Nsat + icon** — `GPSIndicator.qml`
- Replaced 7 hardcoded `color: "#000000"` with `color: qgcPal.text` so
  text is readable against the dark ribbon.

**Flight mode display** — `FlightModeIndicator.qml`
- REMOVED the interactive dropdown from the ribbon.
- Rewritten (~90 lines) as a read-only display: `Item` root,
  `RowLayout` with `QGCColoredImage(/qmlimages/FlightModesComponentIcon.png)`,
  mode-name label (maps `Position → Manual`), optional VTOL tag,
  optional Dagger "tracking" pill.
- No chip, no chevron, no MouseArea, no drawer.
- Retains the `ribbonTextColor` property because `FlyViewToolBar.qml`
  still binds to it.

### 3.2 Left tool strip (fly-view command strip)

- REMOVED: **Vision**, **Tracking**, **PN** buttons.
- ADDED: **Flight Mode** button (new mode picker).
- Current order (top → bottom):
  Takeoff, Standoff, Land, Hold, Flight Mode, Abort, Engage.

Files:

- `src/FlyView/FlyViewFlightModeAction.qml` — **NEW**.
  - `id: _root` (see gotcha in §5).
  - `_allowedModes` whitelist:
    `["Takeoff", "Land", "Safe Recovery", "Return", "Position",
      "Standoff", "Engagement", "Hold", "Abort"]`.
  - `_displayLabel(mode)` maps `Position → Manual`.
  - `dropPanelComponent`: `ColumnLayout { id: panelColumn; ... }` with a
    header `Current: <mode>` and a Repeater of `QGCButton` rows.
  - Selecting a row calls `dropPanel.hide()` then confirms via
    `QGroundControl.showMessageDialog(..., Ok|Cancel, cb)` before
    writing `vehicle.flightMode = modeName`.
- `src/FlyView/CMakeLists.txt` — registered `FlyViewFlightModeAction.qml`
  in `QML_FILES`.
- `src/FlyView/FlyViewToolStripActionList.qml` — inserted
  `FlyViewFlightModeAction { }` between the Hold action and
  `GuidedActionAbort`.

### 3.3 Map interaction

- ADDED map-click context action: **Set Standoff here** (drops the
  standoff geometry at the clicked coordinate; complements the ribbon's
  existing draggable **Set Standoff** button).

> A new agent should `grep` `src/FlyView/` for `Set Standoff` /
> `standoffHere` to confirm the exact file(s) touched (likely
> `FlyViewMap.qml` and/or a right-click menu Component).

### 3.4 Fly-view GCS overlay — `src/FlyView/FlyViewMap.qml`

- Bound props:
  - `_gcsPosition = QGroundControl.qgcPositionManger.gcsPosition`
  - `_gcsHeading`
  - `_gcsTrail = []`
- `_gcsEffectiveHeading` fallback cascade:
  `gcsHeading` → last-trail-segment azimuth (dist > 0.5 m) →
  azimuth to active vehicle → `0`.
- `_gcsHeadingIsFallback = isNaN(_gcsHeading)` drives `opacity: 0.55` on
  the chevron so pilots see it's estimated.
- `Component.onCompleted` seeds `_gcsPosition`, `_gcsTrail`, and
  `gcsTrailPolyline.path` from the current GCS position (fixes cold-start
  where the marker didn't appear until the receiver next moved).
  Logs `STRATUM FlyViewMap: initial gcsPosition valid=<bool>`.
- `gcsHeadingWedge`: always visible, `rotation: _root._gcsEffectiveHeading`,
  animated with `RotationAnimation`. Chevron canvas dims:
  `tip = -h*0.08`, `baseY = h*0.32`, `half = w*0.18`.
- Bearing line + `<bearing>° <distance> m` label from GCS to active vehicle.
- Blue dotted trail follows GCS motion.
- All overlays gated by `!pipMode`.

### 3.5 NMEA / GCS position source — `src/Comms/LinkManager.cc`

- `_addSerialAutoConnectLink` now explicitly:
  `newPort->open(QIODevice::ReadOnly)` **before** handing to
  `QGCPositionManager::setNmeaSourceDevice`.
- Logs `Opened NMEA serial port <port> at <baud> baud` on success and
  `qCWarning` with `errorString()` on failure.
- Fixes the case where another app (e.g. UPrecise) had been holding the
  port, leaving the GCS marker invisible.

### 3.6 Settings — Joystick

- Joystick configuration was **added** to the Configure / Settings panel.
- **Additive only** — nothing was removed from the existing configure
  panels. The next agent should `grep` `Joystick` in
  `src/UI/AppSettings.qml` / `src/QmlControls/AppSettings*.qml` /
  `src/Joystick/*` to identify the exact insertion point before editing.

---

## 4. Palette + text-color rule (repeat)

Any **new** ribbon or toolbar text must bind to `qgcPal.text` (or a QML
property whose value is `qgcPal.text`), never `"#000000"` or `"black"`.

---

## 5. QML gotchas to remember

1. **`QGCButton` inherits `AbstractButton.action`.**  Never use `id: action`
   on an enclosing `ToolStripAction` (or anything containing a `QGCButton`).
   Inside the button binding scope, `action` resolves to the button's own
   null `Action` and shadows the outer id, silently producing empty rows.
   Use `id: _root` (or similar) and qualify all references
   (`_root._displayLabel(...)`, `_root._commandMode(...)`).

2. **`ToolStripAction.dropPanelComponent` scope** sees both the outer file
   scope AND the `dropPanel` id from `ToolStripDropPanel` inside
   `ToolStrip.qml`, so `dropPanel.hide()` inside a panel component works.

3. **Stationary GNSS receivers usually don't emit RMC track angle**, so
   `QGCPositionManager.gcsHeading` stays NaN. Always cascade to a
   motion-based or bearing-based fallback and dim the chevron so pilots
   know it's estimated.

---

## 6. Open items

- [ ] Mirror every Dropper edit above to
  `Dagger/STRATUM-Desktop/qgroundcontrol/`. User asked to hold this until
  they field-verify the Dropper build.
- [ ] Field-verify GCS marker appears at cold start with the real receiver
  (Uprecise closed). Look for these lines in the app log:
  - `Configuring nmea port …`
  - `Opened NMEA serial port … at <baud> baud`
  - `STRATUM FlyViewMap: initial gcsPosition valid=true`
  If validity stays `false` after `open` succeeds, the receiver is
  emitting non-standard NMEA (not `$GNGGA` / `$GNRMC`).
- [ ] Housekeeping: delete stray files
  `pdf_extract.txt`, `_md_to_pdf.py`, `_roadmap_tmp.html`.
- [ ] User is committing manually — **do not auto-commit or push**.

---

## 7. File index (quick reference)

| Area | File | Change |
|---|---|---|
| Ribbon | `src/Toolbar/GPSIndicator.qml` | text → `qgcPal.text` |
| Ribbon | `src/Toolbar/MainStatusIndicator.qml` | chip re-anchored, one MouseArea |
| Ribbon | `src/Toolbar/FlightModeIndicator.qml` | rewritten as read-only display |
| Strip | `src/FlyView/FlyViewFlightModeAction.qml` | **NEW** mode picker |
| Strip | `src/FlyView/FlyViewToolStripActionList.qml` | inserted mode action, removed Vision/Tracking/PN |
| Strip | `src/FlyView/CMakeLists.txt` | registered new QML |
| Map | `src/FlyView/FlyViewMap.qml` | GCS marker, heading fallback, trail, bearing, Set-Standoff-here |
| Comms | `src/Comms/LinkManager.cc` | NMEA port `open()` + logging |
| Settings | Joystick section | additive only |

---

*End of context. Anything above is factually settled from the Dropper
session. Anything a new agent needs to verify — Set Standoff map action,
exact Joystick settings file, Vision/Tracking/PN removal specifics — is
called out with a "grep this" hint in the relevant section.*
