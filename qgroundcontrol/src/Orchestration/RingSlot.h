#pragma once

#include <QtCore/QObject>
#include <QtQmlIntegration/QtQmlIntegration>

class VehicleAgent;

/// RingSlot — a station on the ring (ICD §3.4).
/// RingSlot ↔ VehicleAgent is a bijection: an unassigned slot does not exist.
class RingSlot : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")
    Q_MOC_INCLUDE("VehicleAgent.h")

public:
    enum SlotState { EMPTY, ASSIGNED, COMMITTED, OCCUPIED, VACATED };   // ICD §5.3
    Q_ENUM(SlotState)

    Q_PROPERTY(double        bearingDeg   READ bearingDeg   WRITE setBearingDeg NOTIFY changed)  ///< θ
    Q_PROPERTY(double        transitLevel READ transitLevel                     NOTIFY changed)  ///< AMSL m
    Q_PROPERTY(VehicleAgent* agent        READ agent                            NOTIFY changed)
    Q_PROPERTY(SlotState     slotState    READ slotState                        NOTIFY changed)

    explicit RingSlot(QObject *parent = nullptr);

    double        bearingDeg()   const { return _bearingDeg; }
    double        transitLevel() const { return _transitLevel; }
    VehicleAgent* agent()        const { return _agent; }
    SlotState     slotState()    const { return _slotState; }

    // NOTE: θ_min enforcement (ICD §3.4) is mediated by OrchestrationManager via
    // StandoffRing::bearingAllowed() and wired in S2; here setBearingDeg only stores.
    void setBearingDeg(double bearingDeg);
    void setTransitLevel(double amslMeters);
    void setAgent(VehicleAgent *agent);
    void setSlotState(SlotState s);

signals:
    void changed();
    void bearingRejected(double requestedDeg);   ///< emitted instead of accepting a θ_min violation

private:
    double        _bearingDeg   = 0.0;
    double        _transitLevel = 0.0;
    VehicleAgent* _agent        = nullptr;
    SlotState     _slotState    = EMPTY;
};
