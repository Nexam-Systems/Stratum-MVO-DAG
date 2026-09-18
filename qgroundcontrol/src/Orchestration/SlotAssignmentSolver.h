#pragma once

#include <QtCore/QVector>
#include <QtCore/QPair>

class Vehicle;
class StandoffRing;

/// SlotAssignmentSolver — ICD §3.5 / architecture D5.
/// "Optimum" := minimum total run-in path length subject to a no-crossing
/// constraint. At n <= 3 the assignment problem is <= 6 permutations, enumerated
/// directly; no solver library is introduced. The solver proposes; the operator
/// disposes. An override that induces a crossing is flagged, not blocked.
class SlotAssignmentSolver
{
public:
    struct Assignment {
        QVector<QPair<int, double>> pairs;         ///< (vehicleId, bearingDeg)
        double                      totalPathM   = 0.0;
        bool                        crossingFree = true;
    };

    /// Enumerate assignments of `vehicles` to `slotBearings`, score each by total
    /// run-in path length, and return the minimum-path crossing-free assignment;
    /// fall back to the minimum-path assignment overall (crossingFree = false) if
    /// none is crossing-free.
    Assignment propose(const QVector<Vehicle*>& vehicles,
                       const StandoffRing& ring,
                       const QVector<double>& slotBearings) const;

    /// True if straight-line run-ins cross — i.e. the cyclic angular order of the
    /// vehicles' launch positions about the target does NOT match the cyclic order
    /// of their assigned slot bearings. `assignedBearings[i]` is the bearing given
    /// to `vehicles[i]` (same index order).
    bool inducesCrossing(const QVector<Vehicle*>& vehicles,
                         const QVector<double>& assignedBearings,
                         const StandoffRing& ring) const;
};
