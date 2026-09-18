#pragma once

#include <QtCore/QVector>

#include "OrchestrationTypes.h"

class VehicleAgent;
class StandoffRing;

/// TransitPlanner — ICD §3.6 / architecture §6.3, D7.
/// Allocates a distinct transit level per agent (base + i*deltaH, deltaH >= 10 m —
/// the workhorse vertical-deconfliction layer), computes run-in lengths and ETAs,
/// and produces the staggered CommitSchedule.
///
/// Decisions in force (DECISION-LOG, 2026-09-18):
///  - Common hold height H: only transit levels differ.
///  - Single-pad launch topology: ~5 m pad spacing sits inside R_req (~7 m), so
///    adjacent airframes must NOT climb simultaneously. A departure-time stagger
///    is therefore REQUIRED, on top of altitude stratification.
class TransitPlanner
{
public:
    TransitPlan build(const QVector<VehicleAgent*>& agents,
                      const StandoffRing& ring,
                      double deltaH,
                      double nominalCruiseMps) const;
};
