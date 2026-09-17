#pragma once

#include <QtCore/QObject>
#include <QtPositioning/QGeoCoordinate>
#include <QtQmlIntegration/QtQmlIntegration>

class StandoffRing;
class RingSlot;
class VehicleAgent;
class QmlObjectListModel;
class Vehicle;

/// OrchestrationManager — the singleton and mission owner (ICD §3.1).
/// Exposed to QML as QGroundControl.orchestration (ICD §9.2).
/// Owns the single active mission, the agent set, and the mission FSM.
class OrchestrationManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
    Q_MOC_INCLUDE("QmlObjectListModel.h")
    Q_MOC_INCLUDE("StandoffRing.h")

public:
    enum MissionState {                                 // ICD §5.1
        IDLE, DEFINING_RING, ASSIGNING_SLOTS, PLAN_REVIEW,
        PREFLIGHT, EXECUTING, ON_STATION, RESLOTTING, TERMINATING
    };
    Q_ENUM(MissionState)

    Q_PROPERTY(MissionState        missionState READ missionState NOTIFY missionStateChanged)
    Q_PROPERTY(StandoffRing*       ring         READ ring         NOTIFY ringChanged)
    Q_PROPERTY(QmlObjectListModel* agents       READ agents       CONSTANT)   ///< VehicleAgent*
    Q_PROPERTY(bool                degraded     READ degraded     NOTIFY degradedChanged)  ///< orthogonal flag, not a state
    Q_PROPERTY(bool                armable      READ armable      NOTIFY armableChanged)    ///< false until FailsafeVerifier passes (RE6)

    explicit OrchestrationManager(QObject *parent = nullptr);

    static OrchestrationManager *instance();

    MissionState        missionState() const { return _missionState; }
    StandoffRing*       ring()         const { return _ring; }
    QmlObjectListModel* agents()       const { return _agents; }
    bool                degraded()     const { return _degraded; }
    bool                armable()      const { return _armable; }

    // Operator intent (wizard-driven) — ICD §3.1. The surface is frozen so the
    // QML (ICD §6) binds to it now; S2/S4 bodies fill proposeAssignment/runPreflight.
    Q_INVOKABLE void beginMission();                                    // IDLE -> DEFINING_RING
    Q_INVOKABLE void setTarget(const QGeoCoordinate &target);
    Q_INVOKABLE void setRing(double radiusMeters, double heightMeters);
    Q_INVOKABLE void proposeAssignment();                              // S2: SlotAssignmentSolver
    Q_INVOKABLE bool assignVehicleToSlot(int vehicleId, int slotIndex); // operator override
    Q_INVOKABLE void reviewPlan();                                     // ASSIGNING_SLOTS -> PLAN_REVIEW
    Q_INVOKABLE void runPreflight();                                   // S2: FailsafeVerifier (RE6)
    Q_INVOKABLE void execute();                                        // PREFLIGHT -> EXECUTING (guarded by armable)
    Q_INVOKABLE void holdAll();                                        // operator-authorised; every agent -> Hold
    Q_INVOKABLE void terminate();                                      // -> TERMINATING
    Q_INVOKABLE void reslot(int vehicleId, double newBearingDeg);      // ON_STATION re-commit

    // --- S1 debug harness only; deleted when FailsafeVerifier wires the real gate (S2, RE6) ---
    Q_INVOKABLE void debugForceArmable(bool armable);

signals:
    void missionStateChanged(MissionState s);
    void ringChanged();
    void degradedChanged(bool degraded);
    void armableChanged(bool armable);
    void advisory(const QString &text);   ///< surfaced to the operator; never actuated (ICD §3.1)

private:
    void _setMissionState(MissionState s);
    void _onVehicleAdded(Vehicle *vehicle);     ///< MultiVehicleManager subscription (ICD §3.1)
    void _onVehicleRemoved(Vehicle *vehicle);
    VehicleAgent* _agentForVehicle(int vehicleId) const;

    MissionState        _missionState = IDLE;
    StandoffRing*       _ring     = nullptr;
    QmlObjectListModel* _agents   = nullptr;    ///< VehicleAgent*
    bool                _degraded = false;
    bool                _armable  = false;
};
