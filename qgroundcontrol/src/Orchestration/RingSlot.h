#pragma once

#include <QtCore/QObject>
#include <QtQmlIntegration/QtQmlIntegration>

#include <functional>

class VehicleAgent;

/// RingSlot — a station on the ring (ICD §3.4).
/// RingSlot <-> VehicleAgent is a bijection: an unassigned slot does not exist.
class RingSlot : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
    Q_MOC_INCLUDE("VehicleAgent.h")

public:
    enum SlotState { EMPTY, ASSIGNED, COMMITTED, OCCUPIED, VACATED };   // ICD §5.3
    Q_ENUM(SlotState)

    Q_PROPERTY(double        bearingDeg   READ bearingDeg   WRITE setBearingDeg NOTIFY changed)  ///< theta
    Q_PROPERTY(double        transitLevel READ transitLevel                     NOTIFY changed)  ///< AMSL m
    Q_PROPERTY(VehicleAgent* agent        READ agent                            NOTIFY changed)
    Q_PROPERTY(SlotState     slotState    READ slotState                        NOTIFY changed)

    explicit RingSlot(QObject *parent = nullptr);

    double        bearingDeg()   const { return _bearingDeg; }
    double        transitLevel() const { return _transitLevel; }
    VehicleAgent* agent()        const { return _agent; }
    SlotState     slotState()    const { return _slotState; }

    // theta_min enforcement (ICD §3.4) is mediated by OrchestrationManager, which
    // installs a guard closure evaluated against StandoffRing::bearingAllowed() over
    // the other assigned slots. With a guard installed, setBearingDeg refuses a
    // violating value and emits bearingRejected instead of storing it.
    void setBearingDeg(double bearingDeg);
    void setTransitLevel(double amslMeters);
    void setAgent(VehicleAgent *agent);
    void setSlotState(SlotState s);
    void setBearingGuard(std::function<bool(double)> guard);   ///< returns true if a bearing is admissible

signals:
    void changed();
    void bearingRejected(double requestedDeg);   ///< emitted instead of accepting a theta_min violation

private:
    double                       _bearingDeg   = 0.0;
    double                       _transitLevel = 0.0;
    VehicleAgent*                _agent        = nullptr;
    SlotState                    _slotState    = EMPTY;
    std::function<bool(double)>  _bearingGuard;   ///< installed by the manager; empty = accept (initial placement)
};
