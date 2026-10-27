# STRATUM-MVO — GitHub issues to file

**From** handwritten notes, 19 Sep 2026 (STRATUM — DAG — MVO)
**Repo** `Nexam-Systems/Stratum-MVO-DAG`
**Related docs** Architecture, Interface Contract, Roadmap, Decision Log

Six issues, taken from the parts of the notes that aren't already written down as decisions. The notes about *formation not swarm*, *staggered orchestration from STRATUM*, and *fail-safe on link-loss* are already covered in the architecture (D1, D7, D8), so they're left out.

Suggested labels (create once if missing): `mvo`, `envelope`, `safety`, `planner`, `energy`, `annunciation`, `ontology`, `cop`, `needs-decision`, `epic`.

Heads-up on order: these aren't fully independent. The energy estimator (Issue 3) is needed by two of the others. That's flagged per issue.

---

## Issue 1 — Keep MVO missions inside a strict, briefed envelope

**Labels:** `mvo` `envelope` `epic` `needs-decision`

### Background
The single-vehicle STRATUM/Dagger setup works in the field. The next job is hitting one target with more than one Dagger, and we've decided a multi-vehicle standoff can go on to engage. Running several vehicles from the ground brings a lot of "what if" cases — link loss, recall, dwell time, battery — that a fully autonomous vehicle would have to handle by itself.

We're not going that way. We can't make the vehicle smart enough to cover every case, and trying would pile on interfaces and complexity for little gain.

### What we're doing instead
We bound the mission. STRATUM works out safe limits on the ground, briefs them to the operator, and enforces them — the operator flies inside a fixed envelope instead of discovering the edges mid-flight.

This issue is the **parent (epic)** for the specific limits below. It's here so the "envelope" is a real, named concept and not an assumption buried in the planner.

### Scope
**In:** agree the envelope as a proper MVO concept; list the envelope numbers (usable range, recall reserve, RTL dwell, per-vehicle endurance) and who owns each; decide how the envelope is shown to the operator before commit.
**Out:** the individual limits — those are the child issues below.

### Done when
- [ ] A short "MVO Operating Envelope" section exists (in the architecture or a new doc) listing the envelope numbers and their owners.
- [ ] Each envelope number links to a child issue.
- [ ] The Decision Log records "bound the mission" as a decision.
- [ ] The operator sees the envelope before committing (a pre-mission summary).

### To decide
Is the envelope **fixed per platform** (one set of numbers for Dagger), or **set per mission** (operator picks range/reserve within hard limits at plan time)?

---

## Issue 2 — In multi-vehicle mode, cut usable range to half (15 km → 7.5 km) to keep recall margin

**Labels:** `mvo` `envelope` `planner` `safety`
**Part of:** Issue 1

### Background
With one vehicle we give 15 km line-of-sight range, with enough battery for that distance **one way**. That's fine with a single vehicle — the mission spends the whole budget going out.

With several vehicles it isn't. If one fails partway, the operator has to be able to **call it back** — and that's a round trip, not one way. A budget spent reaching 15 km leaves nothing to bring a struggling vehicle home.

### The rule
In multi-vehicle mode, **halve the usable range: 15 km → 7.5 km**. The same battery that took one vehicle 15 km out now covers 7.5 km out *and* 7.5 km back — so any failed vehicle can be recalled within what's left. The return margin is reserved up front, before flight.

### Approach
- Add a range cap that depends on mode: full range for single-vehicle, half range for multi-vehicle. Make the halving a named setting, not a hidden number.
- Enforce it at plan time (in the planner / ring setup): if a target or ring would push any vehicle past the cap, block it at plan review and tell the operator why.
- Keep two things separate: the **standoff ring radius** (around the target) and the **range from home/launch** (what this cap limits). They're different measurements.

### Done when
- [ ] Single-vehicle missions still get the full 15 km — no change.
- [ ] A multi-vehicle plan that goes past 7.5 km is blocked at plan review and names the vehicle/leg.
- [ ] The cap and both ranges are written down as envelope numbers (Issue 1).
- [ ] The reserved margin is shown in the plan-review screen before commit.

### To decide
Is a flat half the right reserve, or should it later be **worked out from the live battery budget** (Issue 3), with half as the safe fallback until that exists? Suggestion: ship the flat half now as the floor; let Issue 4 tighten it later.

---

## Issue 3 — Work out each vehicle's remaining energy / flight time

**Labels:** `mvo` `energy` `safety` `telemetry`
**Blocks:** Issue 4

### Background
The half-range rule (Issue 2) is a simple, safe guess. The real question in the air is: **how much flight does each vehicle have left right now, given where it is and what it's already used?** Even at half range there's a corner case — how long a vehicle can sit on station before it can no longer make it home. Answering that needs an actual energy model, not just a battery percentage.

STRATUM already shows per-vehicle telemetry (position, altitude, battery), and we've already said we'll show remaining endurance so the operator sees a battery problem coming. This issue is the **calculation** behind that.

### Approach
Build a per-vehicle estimator that combines:
- **Known measured numbers** — the hover and cruise energy figures from the Dagger energy-characterization tests.
- **Live readings** — current battery, groundspeed, altitude, and distance to home.

and gives, per vehicle: **energy left**, **flight time left**, and **energy needed to get home from where it is now**. The gap between energy-left and energy-to-get-home is the safe time-on-station that Issue 4 will display.

- One estimator per vehicle, using only that vehicle's own data.
- Be honest when inputs are missing: if a number is stale or absent, lower the confidence — never guess high.
- It only calculates and reports. It doesn't command anything.

### Scope
**In:** the estimator, its measured-baseline input, its energy-to-return calculation, and its per-vehicle output.
**Out:** the warning screen (Issue 4); any automatic action on a low battery (explicitly not doing this — show and warn only).

### Done when
- [ ] Per-vehicle energy-left, time-left, and energy-to-return are calculated from measured baseline + live data.
- [ ] Estimates match a known test flight within an agreed tolerance.
- [ ] Missing/stale inputs lower confidence clearly; no optimistic guessing.
- [ ] The estimator reads only its own vehicle and sends no commands.

### Needs
- Agreed hover/cruise energy numbers from the Dagger energy-characterization tests, as the baseline.

---

## Issue 4 — Show a per-vehicle "safe time left to recall" warning on STRATUM

**Labels:** `mvo` `annunciation` `safety` `ui`
**Needs:** Issue 3

### Background
Once we know each vehicle's live energy (Issue 3), the operator needs to see the one number that matters for recall: **how much longer this vehicle can stay on station and still be brought home safely.** After that point, it can't guarantee the trip back. This is the warning/indication the notes ask for on STRATUM.

### Approach
- Show a per-vehicle **"safe time to recall"** value in the operator display — most naturally in each vehicle's row in the status strip — with a warning that gets more urgent as time runs down.
- The value comes from Issue 3: energy-left minus energy-needed-to-return.
- **Warning only.** STRATUM alerts and lets the operator act (recall, hold, RTL). It doesn't auto-recall.
- Caution/critical thresholds are envelope numbers (Issue 1), briefed before the mission — not surprise cutoffs.

### Scope
**In:** the per-vehicle time-to-recall display, its warning levels, and where it sits in the status strip / alerts.
**Out:** the calculation (Issue 3); any automatic recall.

### Done when
- [ ] Each vehicle row shows a live "safe time to recall" from the Issue 3 estimator.
- [ ] It steps through caution/critical at the briefed thresholds.
- [ ] Crossing critical raises an alert naming the vehicle; nothing moves on its own.
- [ ] It's tested in the fault runs (roadmap S4): a vehicle pushed toward its limit triggers the warning at the right time.

---

## Issue 5 — Let the operator pick single- vs multi-vehicle mode, and put it in the ontology

**Labels:** `mvo` `ontology` `feature`

### Background
STRATUM for Dagger should let the operator **choose the mode — single-vehicle or multi-vehicle — on purpose**, instead of guessing it from how many vehicles happen to be connected. The two modes have different limits (the half-range cap applies to one), different fail-safe setups, and different screens. That difference should be part of the model, not a hidden branch in the code — so it belongs **in the STRATUM C2 ontology**.

### Approach
- Add an **operating-mode** entity (single / multi) to the C2 ontology, along with what depends on it: which envelope applies, which controls are active, which fail-safe settings are required.
- Have the rest of the system read the mode from the model — the range cap (Issue 2), the fail-safe check, the active screen — instead of each working it out separately.
- Leave the single-vehicle path fully intact and as the default.

### Done when
- [ ] Operating mode (single / multi) is a proper entity in the ontology with its dependencies defined.
- [ ] The operator can choose the mode before planning a mission.
- [ ] Envelope, controls, and required fail-safe settings follow the chosen mode.
- [ ] Picking single-vehicle behaves exactly like today.

### To decide
Is the mode picked once at mission start, or can it change mid-mission? Suggestion: pick it at the start and lock it for that mission — changing mid-mission is a re-plan, not a toggle.

---

## Issue 6 — MVO needs a COP-style operator screen

**Labels:** `mvo` `cop` `ui` `feature`

### Background
The single-vehicle screen is built around one vehicle and one target. With multiple vehicles, the operator's job shifts from *flying one vehicle* to *watching a group of them against a target* — that's a **common operating picture (COP)** problem. So MVO needs a COP-style screen: one combined view of all the vehicles, their positions, their status, their link and battery health, and the target/ring, together in one place.

### Approach
- Define a COP view for MVO that brings together the pieces already planned — the ring/slot map overlay, the per-vehicle status rows, the alerts — plus the energy/recall health from Issues 3–4.
- First write down what the COP has to show (which vehicle, which slot, which state, which health, the target), then design the layout. The picture is the point, so it's worth a proper requirements pass rather than bolting bits on.
- Keep it additive: every per-vehicle element gets its vehicle passed in directly; nothing changes the single-vehicle screen.

### Done when
- [ ] A short list exists of what the COP must show (vehicles, slots, states, health, target).
- [ ] One combined MVO view shows all vehicles, slots, states, link + battery health, and the target/ring together.
- [ ] It reuses the existing overlays and the Issue 3–4 health instead of duplicating them.
- [ ] No change to the single-vehicle screen.

### To decide
Is the COP a **new dedicated screen**, or an **add-on to the current FlyView** with orchestration layers on top? Worth settling before design starts.

---

### How they fit together

```
Issue 1 (envelope — parent)
   ├── Issue 2 (half range for recall)          ── simple floor, ship first
   └── Issue 4 (safe-time-to-recall warning) ◀── Issue 3 (energy estimator)
Issue 5 (mode in ontology)                       ── drives which envelope/fail-safe applies
Issue 6 (COP screen)                             ── brings 2/3/4 into one picture
```

Suggested order: **1 → 2 → 3 → 4**, then **5** and **6** alongside. Issue 3 is the long pole — it needs the test-flight energy numbers first, so kick off that handoff early.
