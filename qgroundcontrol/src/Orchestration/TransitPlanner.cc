#include "TransitPlanner.h"
#include "VehicleAgent.h"
#include "RingSlot.h"
#include "StandoffRing.h"
#include "Vehicle.h"

#include <QtPositioning/QGeoCoordinate>

#include <algorithm>

namespace {
constexpr double kNominalClimbMps  = 3.0;    // nominal climb rate (ratified in flight test)
constexpr double kDepartureMarginS = 2.0;    // headroom on top of the deltaH-climb time
constexpr double kMinDeltaH        = 10.0;   // ICD §6.3 floor

QGeoCoordinate launchPos(Vehicle *v)
{
    if (!v) {
        return QGeoCoordinate();
    }
    const QGeoCoordinate home = v->homePosition();
    return home.isValid() ? home : v->coordinate();
}
} // namespace

TransitPlan TransitPlanner::build(const QVector<VehicleAgent*> &agents,
                                  const StandoffRing &ring,
                                  double deltaH,
                                  double nominalCruiseMps) const
{
    TransitPlan plan;
    const double dH = std::max(deltaH, kMinDeltaH);
    plan.deltaH = dH;

    // Base transit level clears the common hold height H by one deltaH so that even
    // the lowest run-in sits above the ring's terminal altitude.
    const double base = ring.heightM() + dH;

    // Departure stagger: airframe i+1 lifts only after i has climbed a full deltaH,
    // i.e. once i holds a complete vertical bubble over the shared pad (single-pad
    // decision — simultaneous climb from adjacent points is disallowed).
    const double staggerStep = (dH / kNominalClimbMps) + kDepartureMarginS;

    for (int i = 0; i < agents.size(); ++i) {
        VehicleAgent *a = agents[i];

        const double level = base + (i * dH);
        plan.transitLevels.append(level);

        double path = 0.0;
        if (a && a->slot()) {
            const QGeoCoordinate slotC = ring.slotCoordinate(a->slot()->bearingDeg());
            path = launchPos(a->vehicle()).distanceTo(slotC);
        }
        plan.pathLengths.append(path);

        const double cruiseEta = (nominalCruiseMps > 0.0) ? (path / nominalCruiseMps) : 0.0;
        const double climbEta  = level / kNominalClimbMps;
        plan.etas.append(cruiseEta + climbEta);

        const int vid = (a && a->vehicle()) ? a->vehicle()->id() : -1;
        plan.schedule.entries.append({ vid, i * staggerStep });
    }

    return plan;
}
