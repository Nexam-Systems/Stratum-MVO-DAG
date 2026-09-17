#include "RingSlot.h"

RingSlot::RingSlot(QObject *parent)
    : QObject(parent)
{
}

void RingSlot::setBearingDeg(double bearingDeg)
{
    if (qFuzzyCompare(_bearingDeg, bearingDeg)) {
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
