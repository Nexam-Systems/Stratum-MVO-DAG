#include "RingSlot.h"

#include <utility>

RingSlot::RingSlot(QObject *parent)
    : QObject(parent)
{
}

void RingSlot::setBearingDeg(double bearingDeg)
{
    if (qFuzzyCompare(_bearingDeg, bearingDeg)) {
        return;
    }
    // theta_min structural deconfliction (ICD §3.4/§6.2), evaluated before flight.
    // A guard is installed once the slot joins the ring; before that, placement is free.
    if (_bearingGuard && !_bearingGuard(bearingDeg)) {
        emit bearingRejected(bearingDeg);
        return;
    }
    _bearingDeg = bearingDeg;
    emit changed();
}

void RingSlot::setTransitLevel(double amslMeters)
{
    _transitLevel = amslMeters;
    emit changed();
}

void RingSlot::setAgent(VehicleAgent *agent)
{
    _agent = agent;
    emit changed();
}

void RingSlot::setSlotState(SlotState s)
{
    if (_slotState == s) {
        return;
    }
    _slotState = s;
    emit changed();
}

void RingSlot::setBearingGuard(std::function<bool(double)> guard)
{
    _bearingGuard = std::move(guard);
}
