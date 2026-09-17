#include "VehicleAgent.h"
#include "RingSlot.h"
#include "Vehicle.h"

VehicleAgent::VehicleAgent(Vehicle *vehicle, QObject *parent)
    : QObject(parent)
    , _vehicle(vehicle)   // pinned for life (RE2); _vehicle is const
{
}

void VehicleAgent::_setState(AgentState s)
{
    if (_state == s) {
        return;
    }
    _state = s;
    emit stateChanged(_state);
}

void VehicleAgent::assignSlot(RingSlot *slot)
{
    _slot = slot;
    emit slotChanged();
    _setState(ASSIGNED);
}

void VehicleAgent::setTransitLevel(double amslMeters)
{
    _transitLevel = amslMeters;
    emit transitLevelChanged();
}

void VehicleAgent::beginCommit()
{
    // ICD §7.1 — geometry precedes mode (RE5). Steps 1→2→3 in order.
    // S1: implement against SITL; prove 31010 ACK precedes DO_SET_MODE in log.
    _stepEnsureTransitLevel();
    _stepSendStandoffSetpoint();
    _stepSwitchMode();
}

void VehicleAgent::hold()
{
    // Operator-authorised only. -> "Hold". (RE4: never issued autonomously.)
    _setState(HOLD);
}

void VehicleAgent::abortToRtl()
{
    // Operator-directed per-agent RTL (ICD §2.2 guidedModeRTL).
    _setState(RTL);
}

void VehicleAgent::markLinkLost()
{
    _linkHealthy = false;
    emit linkHealthyChanged();
    _setState(LINK_LOST);
}

// --- Commit steps (S1 SITL fill; signatures per ICD §2.2 / §7.1) ------------

void VehicleAgent::_stepEnsureTransitLevel()
{
    // if on ground:  _vehicle->guidedModeTakeoff(_transitLevel);
    // if airborne:   _vehicle->guidedModeChangeAltitude(delta, false);
    _setState(_vehicle ? TAKEOFF : UNASSIGNED);   // placeholder transition
}

void VehicleAgent::_stepSendStandoffSetpoint()
{
    // sendMavCommandIntWithHandler(ack, _vehicle->defaultComponentId(),
    //     MAV_CMD_USER_1 /*31010*/, MAV_FRAME_GLOBAL_RELATIVE_ALT,
    //     R, theta_i, 0, NaN, target.lat, target.lon, H);  await MAV_RESULT_ACCEPTED
    _setState(COMMIT_QUEUED);
}

void VehicleAgent::_stepSwitchMode()
{
    // _vehicle->setFlightMode("Standoff");  await customMode decode -> nav_state == 9
    _setState(STANDOFF_COMMANDED);
}

bool VehicleAgent::_arrivalPredicate() const
{
    // ICD §7.4: (nav_state 9->3) AND horizontal dist to slotCoordinate < NAV_ACC_RAD
    //           AND |altitudeAMSL - (home_alt + H)| < alt acceptance.
    return false;   // S1 fill
}
