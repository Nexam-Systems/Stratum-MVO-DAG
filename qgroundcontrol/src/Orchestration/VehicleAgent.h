#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtQmlIntegration/QtQmlIntegration>

#include "VehicleTypes.h"   // mavlink_command_ack_t + MavCmdResultFailureCode_t for the ACK handler

class Vehicle;
class RingSlot;
class StandoffRing;

/// VehicleAgent — one vehicle, one slot, one FSM (ICD §3.2).
/// Pins its Vehicle* at construction and never lets go (RE2). Observes and
/// commands only that one vehicle; makes no separation claim.
class VehicleAgent : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
    Q_MOC_INCLUDE("Vehicle.h")
    Q_MOC_INCLUDE("RingSlot.h")

public:
    enum AgentState {                                   // ICD §5.2
        UNASSIGNED, ASSIGNED, PREFLIGHT, LAUNCH_QUEUED, TAKEOFF,
        CLIMB_TO_TRANSIT_LEVEL, COMMIT_QUEUED, STANDOFF_COMMANDED,
        RUN_IN,        // nav_state == 9
        ON_STATION,    // nav_state 9 -> 4 AND arrival predicate (ICD §7.4)
        HOLD,
        ENGAGE_COMMANDED,  // operator engage issued (Addendum A); mode write sent
        ENGAGING,          // coordinate Engagement (sub=21) confirmed via flightMode
        RESLOT, RTL, LINK_LOST
    };
    Q_ENUM(AgentState)

    Q_PROPERTY(Vehicle*   vehicle      READ vehicle      CONSTANT)          ///< pinned for life (RE2)
    Q_PROPERTY(AgentState state        READ state        NOTIFY stateChanged)
    Q_PROPERTY(RingSlot*  slot         READ slot         NOTIFY slotChanged)
    Q_PROPERTY(double     transitLevel READ transitLevel NOTIFY transitLevelChanged)  ///< S1: relative height above home
    Q_PROPERTY(double     runInLengthM READ runInLengthM NOTIFY planChanged)
    Q_PROPERTY(double     etaSeconds   READ etaSeconds   NOTIFY planChanged)
    Q_PROPERTY(bool       linkHealthy  READ linkHealthy  NOTIFY linkHealthyChanged)

    explicit VehicleAgent(Vehicle *vehicle, QObject *parent = nullptr);

    Vehicle*   vehicle()      const { return _vehicle; }
    AgentState state()        const { return _state; }
    RingSlot*  slot()         const { return _slot; }
    double     transitLevel() const { return _transitLevel; }
    double     runInLengthM() const { return _runInLengthM; }
    double     etaSeconds()   const { return _etaSeconds; }
    bool       linkHealthy()  const { return _linkHealthy; }

    void setRing(StandoffRing *ring);   ///< the derived ring geometry (target, R, H) this agent commits to
    void assignSlot(RingSlot *slot);
    void setTransitLevel(double relativeMeters);
    void setPlanMetrics(double runInLengthM, double etaSeconds);   ///< planner metrics for display (ICD §3.2)
    void beginCommit();     ///< executes the ICD §7 commit sequence for this one agent
    void hold();            ///< -> Hold (operator-authorised path only)
    void abortToRtl();      ///< operator-directed RTL
    void engage();          ///< operator-commanded coordinate Engagement (sub=21); ON_STATION only (Addendum A)
    void markLinkLost();    ///< driven by OrchestrationManager on vehicleRemoved

signals:
    void stateChanged(AgentState s);
    void slotChanged();
    void transitLevelChanged();
    void planChanged();
    void linkHealthyChanged();
    void onStation();       ///< arrival predicate satisfied (ICD §7.4)

private:
    void _setState(AgentState s);
    void _wire();                       ///< connect telemetry once (flightMode, altitude)
    double _currentRelAlt() const;

    // --- ICD §7 commit sequence, per agent. The ordering below is firmware-coupled
    //     and MUST NOT be reordered (RE5): geometry (31010) is ACK-confirmed BEFORE the
    //     mode switch, and the mode switch is confirmed via telemetry before RUN_IN. ---
    void _stepEnsureTransitLevel();   // 1: guidedModeTakeoff / guidedModeChangeAltitude
    void _stepSendStandoffSetpoint(); // 2: sendMavCommandIntWithHandler(31010, ...) ACK-matched
    void _stepSwitchMode();           // 3: flightMode = "Standoff"; RUN_IN on nav_state == 9
    bool _arrivalPredicate() const;   // ICD §7.4: (9->4) AND slot-dist < acc AND alt within tol

    // Telemetry-driven progression.
    void _onFlightModeChanged(const QString &mode);
    void _onAltitudeChanged();

    // ACK handler for the 31010 COMMAND_INT (ICD §7.3). Static so it matches the
    // MavCmdResultHandler function-pointer type; `data` carries the VehicleAgent*.
    static void _onStandoffAck(void *data, int compId, const mavlink_command_ack_t &ack,
                               VehicleTypes::MavCmdResultFailureCode_t failureCode);

    Vehicle* const _vehicle;         ///< immutable — the compile-time guarantee of RE2
    StandoffRing*  _ring         = nullptr;   ///< derived geometry; not owned
    RingSlot*      _slot         = nullptr;
    AgentState     _state        = UNASSIGNED;
    double         _transitLevel = 0.0;       ///< S1: relative height above home for the run-in
    double         _runInLengthM = 0.0;
    double         _etaSeconds   = 0.0;
    bool           _linkHealthy  = true;
    bool           _wired        = false;
};
