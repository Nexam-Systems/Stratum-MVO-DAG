#pragma once

#include <QtCore/QObject>
#include <QtQmlIntegration/QtQmlIntegration>

class Vehicle;
class RingSlot;

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
        ON_STATION,    // nav_state 9 -> 3 AND arrival predicate (ICD §7.4)
        HOLD, RESLOT, RTL, LINK_LOST
    };
    Q_ENUM(AgentState)

    Q_PROPERTY(Vehicle*   vehicle      READ vehicle      CONSTANT)          ///< pinned for life (RE2)
    Q_PROPERTY(AgentState state        READ state        NOTIFY stateChanged)
    Q_PROPERTY(RingSlot*  slot         READ slot         NOTIFY slotChanged)
    Q_PROPERTY(double     transitLevel READ transitLevel NOTIFY transitLevelChanged)  ///< AMSL m
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

    void assignSlot(RingSlot *slot);
    void setTransitLevel(double amslMeters);
    void beginCommit();     ///< executes the ICD §7 commit sequence for this one agent
    void hold();            ///< -> Hold (operator-authorised path only)
    void abortToRtl();      ///< operator-directed RTL
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

    // --- ICD §7 commit sequence, per agent. Bodies are filled and demonstrated
    //     against multi-instance SITL during S1 (roadmap S1 exit gate). The
    //     ordering below is firmware-coupled and MUST NOT be reordered (RE5). ---
    void _stepEnsureTransitLevel();  // 1: guidedModeTakeoff / guidedModeChangeAltitude
    void _stepSendStandoffSetpoint(); // 2: sendMavCommandIntWithHandler(31010, ...) ACK-matched, BEFORE mode
    void _stepSwitchMode();           // 3: flightMode = "Standoff"; await nav_state == 9
    bool _arrivalPredicate() const;   // ICD §7.4: (9->3) AND slot-dist < NAV_ACC_RAD AND alt within tol

    Vehicle* const _vehicle;         ///< immutable — the compile-time guarantee of RE2
    RingSlot*      _slot         = nullptr;
    AgentState     _state        = UNASSIGNED;
    double         _transitLevel = 0.0;
    double         _runInLengthM = 0.0;
    double         _etaSeconds   = 0.0;
    bool           _linkHealthy  = true;
};
