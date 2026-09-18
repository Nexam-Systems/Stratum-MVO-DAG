#include "SlotAssignmentSolver.h"
#include "StandoffRing.h"
#include "Vehicle.h"

#include <QtPositioning/QGeoCoordinate>

#include <algorithm>
#include <numeric>

namespace {

// Launch reference for a vehicle: its home if known, else its live position. The
// conflict the solver guards against lives near the launch point (architecture §6.1).
QGeoCoordinate launchPos(Vehicle *v)
{
    if (!v) {
        return QGeoCoordinate();
    }
    const QGeoCoordinate home = v->homePosition();
    return home.isValid() ? home : v->coordinate();
}

// Cyclic-order test (architecture D5). Reading the assigned bearings in order of
// increasing launch azimuth about the target, straight-line run-ins are
// crossing-free iff that bearing sequence is cyclically monotonic — at most one
// descent when the sequence is traversed as a ring. Robust for n <= 3.
bool crossingFreeByCyclicOrder(QVector<QPair<double, double>> launchAzAndBearing)
{
    const int n = launchAzAndBearing.size();
    if (n < 3) {
        return true;   // 0, 1 or 2 straight run-ins to distinct ring slots cannot cross
    }
    std::sort(launchAzAndBearing.begin(), launchAzAndBearing.end(),
              [](const QPair<double, double> &a, const QPair<double, double> &b) {
                  return a.first < b.first;
              });
    int descents = 0;
    for (int i = 0; i < n; ++i) {
        const double cur  = launchAzAndBearing[i].second;
        const double next = launchAzAndBearing[(i + 1) % n].second;
        if (next < cur) {
            ++descents;
        }
    }
    return descents <= 1;
}

} // namespace

bool SlotAssignmentSolver::inducesCrossing(const QVector<Vehicle*> &vehicles,
                                           const QVector<double> &assignedBearings,
                                           const StandoffRing &ring) const
{
    const int n = std::min(vehicles.size(), assignedBearings.size());
    const QGeoCoordinate target = ring.target();
    QVector<QPair<double, double>> pairs;
    pairs.reserve(n);
    for (int i = 0; i < n; ++i) {
        const double az = target.azimuthTo(launchPos(vehicles[i]));
        pairs.append({ az, assignedBearings[i] });
    }
    return !crossingFreeByCyclicOrder(pairs);
}

SlotAssignmentSolver::Assignment
SlotAssignmentSolver::propose(const QVector<Vehicle*> &vehicles,
                              const StandoffRing &ring,
                              const QVector<double> &slotBearings) const
{
    const int n = std::min(vehicles.size(), slotBearings.size());
    Assignment result;
    if (n == 0) {
        return result;
    }

    const QGeoCoordinate target = ring.target();

    // Precompute slot coordinates for each bearing.
    QVector<QGeoCoordinate> slotCoord(n);
    for (int j = 0; j < n; ++j) {
        slotCoord[j] = ring.slotCoordinate(slotBearings[j]);
    }

    QVector<int> perm(n);
    std::iota(perm.begin(), perm.end(), 0);   // sorted start so next_permutation enumerates all

    bool         haveCF  = false;
    double       bestCF  = 0.0;
    double       bestAny = 0.0;
    QVector<int> permCF;
    QVector<int> permAny;

    do {
        double total = 0.0;
        QVector<QPair<double, double>> azBearing;
        azBearing.reserve(n);
        for (int i = 0; i < n; ++i) {
            const QGeoCoordinate launch = launchPos(vehicles[i]);
            total += launch.distanceTo(slotCoord[perm[i]]);   // vehicle i -> bearing perm[i]
            azBearing.append({ target.azimuthTo(launch), slotBearings[perm[i]] });
        }
        const bool cf = crossingFreeByCyclicOrder(azBearing);

        if (permAny.isEmpty() || total < bestAny) {
            bestAny = total;
            permAny = perm;
        }
        if (cf && (!haveCF || total < bestCF)) {
            haveCF = true;
            bestCF = total;
            permCF = perm;
        }
    } while (std::next_permutation(perm.begin(), perm.end()));

    const QVector<int> &chosen = haveCF ? permCF : permAny;
    result.crossingFree = haveCF;
    result.totalPathM   = haveCF ? bestCF : bestAny;
    for (int i = 0; i < n; ++i) {
        result.pairs.append({ vehicles[i]->id(), slotBearings[chosen[i]] });
    }
    return result;
}
