#pragma once

#include <QtCore/QVector>
#include <QtCore/QPair>
#include <QtCore/QString>

// Shared value types crossing Orchestration module seams (ICD §4).
// All distances metres, angles degrees (compass, 0 = North, CW), coordinates WGS-84.

/// Staggered-commit schedule (ICD §4, architecture D7).
/// One ordered (vehicleId, departure time offset in seconds) entry per agent.
struct CommitSchedule {
    QVector<QPair<int, double>> entries;
};

/// TransitPlanner output (ICD §4). Parallel arrays, indexed in agent order.
struct TransitPlan {
    QVector<double> transitLevels;   ///< relative height above home, metres (S1/agent semantics)
    QVector<double> pathLengths;     ///< run-in straight-line length, metres
    QVector<double> etas;            ///< estimated run-in duration, seconds
    double          deltaH = 0.0;    ///< vertical stratification step (>= 10 m)
    CommitSchedule  schedule;
};

/// One parameter reported by FailsafeVerifier (ICD §4, architecture D8/D9).
struct FailsafeFinding {
    int     vehicleId = 0;
    QString param;
    QString expected;
    QString actual;
    bool    blocking = true;   ///< true -> blocks ARM (RE6); false -> advisory only
};

/// FailsafeVerifier result (ICD §3.7). pass == no blocking findings.
struct FailsafeResult {
    bool                     pass = false;
    QVector<FailsafeFinding> findings;
};
