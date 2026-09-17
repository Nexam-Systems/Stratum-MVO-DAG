#include "VehicleAgent.h"
#include "RingSlot.h"
#include "StandoffRing.h"

#include "Vehicle.h"
#include "FactGroup.h"
#include "Fact.h"
#include "QGCMAVLink.h"

#include <cmath>

namespace {
// S1 acceptance tolerances. S2 replaces the arrival radius with the vehicle's
// NAV_ACC_RAD parameter and the altitude test with the AMSL form (ICD §7.4).
constexpr double kAltToleranceM  = 3.0;    // transit-level reach / hold-altitude acceptance
constexpr double kArrivalRadiusM = 15.0;   // horizontal slot-arrival acceptance
}

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

void VehicleAgent::setRing(StandoffRing *ring)
{
    _ring = ring;   // not owned; the manager owns the single derived ring
}

void VehicleAgent::assignSlot(RingSlot *slot)
{
    _slot = slot;
    emit slotChanged();
    _setState(ASSIGNED);
}

void VehicleAgent::setTransitLevel(double relativeMeters)
{
    _transitLevel = relativeMeters;
    emit transitLevelChanged();
}

void VehicleAgent::beginCommit()
{
    // ICD §7.1 — geometry precedes mode (RE5). Steps 1 -> 2 -> 3, sequenced on
    // telemetry: climb completes before the setpoint; the setpoint is ACK-confirmed
    // before the mode switch; RUN_IN is entered only when the mode reads back.
    if (!_vehicle || !_ring || !_slot) {
        qWarning("VehicleAgent::beginCommit: missing vehicle/ring/slot; commit skipped");
        return;
    }
    _wire();
    _stepEnsureTransitLevel();
}

void VehicleAgent::hold()
{
    // Operator-authorised only (RE4). Native PX4 Hold — the one autonomous-capable
    // mode change the subsystem may issue, and only on operator command.
    if (_vehicle) {
        _vehicle->setFlightMode(QStringLiteral("Hold"));
    }
    _setState(HOLD);
}

void VehicleAgent::abortToRtl()
{
    // Operator-directed per-agent RTL (ICD §2.2). Never issued autonomously.
    if (_vehicle) {
        _vehicle->guidedModeRTL(false);
    }
    _setState(RTL);
}

void VehicleAgent::markLinkLost()
{
    _linkHealthy = false;
    emit linkHealthyChanged();
    _setState(LINK_LOST);
}

// --- Telemetry wiring -------------------------------------------------------

void VehicleAgent::_wire()
{
    if (_wired || !_vehicle) {
        return;
    }
    _wired = true;
    (void) connect(_vehicle, &Vehicle::flightModeChanged, this, &VehicleAgent::_onFlightModeChanged);
    if (FactGroup *vfg = _vehicle->vehicleFactGroup()) {
        if (Fact *alt = vfg->getFact(QStringLiteral("altitudeRelative"))) {
            (void) connect(alt, &Fact::rawValueChanged, this, &VehicleAgent::_onAltitudeChanged);
        }
    }
}

double VehicleAgent::_currentRelAlt() const
{
    if (FactGroup *vfg = _vehicle ? _vehicle->vehicleFactGroup() : nullptr) {
        if (Fact *alt = vfg->getFact(QStringLiteral("altitudeRelative"))) {
            return alt->rawValue().toDouble();
        }
    }
    return 0.0;
}

// --- Commit steps (ICD §2.2 / §7.1) -----------------------------------------

void VehicleAgent::_stepEnsureTransitLevel()
{
    // 1. Reach the agent's own transit level so the run-in (F3) is altitude-stratified.
    const double target = _transitLevel;
    if (target <= 0.0) {
        // No transit level requested — commit at current altitude (F3 still holds).
        _stepSendStandoffSetpoint();
        return;
    }
    if (_vehicle->flying()) {
        _setState(CLIMB_TO_TRANSIT_LEVEL);
        const double delta = target - _currentRelAlt();
        if (qAbs(delta) > kAltToleranceM) {
            _vehicle->guidedModeChangeAltitude(delta, false);
        }
    } else {
        _setState(TAKEOFF);
        _vehicle->guidedModeTakeoff(target);
    }
    // Already at level? Don't wait on a fact change that may never come.
    if (qAbs(target - _currentRelAlt()) <= kAltToleranceM) {
        _stepSendStandoffSetpoint();
    }
}

void VehicleAgent::_onAltitudeChanged()
{
    if (_state == TAKEOFF || _state == CLIMB_TO_TRANSIT_LEVEL) {
        if (qAbs(_transitLevel - _currentRelAlt()) <= kAltToleranceM) {
            _stepSendStandoffSetpoint();
        }
    }
}

void VehicleAgent::_stepSendStandoffSetpoint()
{
    // Idempotent: never re-fire the setpoint once committed.
    if (_state == COMMIT_QUEUED || _state == STANDOFF_COMMANDED
            || _state == RUN_IN || _state == ON_STATION) {
        return;
    }
    _setState(COMMIT_QUEUED);

    Vehicle::MavCmdAckHandlerInfo_t info{};
    info.resultHandler       = &VehicleAgent::_onStandoffAck;
    info.resultHandlerData   = this;
    info.progressHandler     = nullptr;
    info.progressHandlerData = nullptr;

    const QGeoCoordinate target = _ring->target();
    const double R     = _ring->radiusM();
    const double theta = _slot->bearingDeg();
    const double H     = _ring->heightM();

    // 2. RE3 wire contract: COMMAND_INT 31010 (MAV_CMD_USER_1 / DO_STANDOFF),
    //    MAV_FRAME_GLOBAL_RELATIVE_ALT. ACK-matched so the FSM keys the mode switch on
    //    the actual MAV_RESULT_ACCEPTED (ICD §7.3), not a fire-and-forget assumption.
    _vehicle->sendMavCommandIntWithHandler(
        &info,
        _vehicle->defaultComponentId(),
        static_cast<MAV_CMD>(MAV_CMD_USER_1),
        MAV_FRAME_GLOBAL_RELATIVE_ALT,
        static_cast<float>(R),       // p1: distance from target
        static_cast<float>(theta),   // p2: bearing target->hold point (deg, 0=N, CW)
        0.0f,                        // p3
        NAN,                         // p4: PX4 chooses yaw -> faces target
        target.latitude(),           // p5 (double): target lat
        target.longitude(),          // p6 (double): target lon
        static_cast<float>(H));      // p7: relative height H above home
}

void VehicleAgent::_onStandoffAck(void *data, int /*compId*/, const mavlink_command_ack_t &ack,
                                  VehicleTypes::MavCmdResultFailureCode_t /*failureCode*/)
{
    auto *self = static_cast<VehicleAgent *>(data);
    if (!self) {
        return;
    }
    if (ack.result == MAV_RESULT_ACCEPTED) {
        if (self->_state == COMMIT_QUEUED) {
            self->_setState(STANDOFF_COMMANDED);
            self->_stepSwitchMode();   // 3: mode AFTER the 31010 ACK (RE5)
        }
    } else {
        qWarning("VehicleAgent: 31010 rejected (result=%d); commit halted for this agent",
                 int(ack.result));
    }
}

void VehicleAgent::_stepSwitchMode()
{
    // 3. DO_SET_MODE main=4, sub=20. RUN_IN is entered when the mode reads back as
    //    "Standoff" (nav_state 9) via flightModeChanged — not on the write itself.
    if (_vehicle) {
        _vehicle->setFlightMode(QStringLiteral("Standoff"));
    }
}

void VehicleAgent::_onFlightModeChanged(const QString &mode)
{
    // Standoff activation confirmed: nav_state == 9.
    if (_state == STANDOFF_COMMANDED && mode == QStringLiteral("Standoff")) {
        _setState(RUN_IN);
        return;
    }
    // nav_state 9 -> 3: the firmware settled into Hold on arrival (F2). Confirm the
    // arrival predicate before declaring ON_STATION (a vehicle can reach Hold by other
    // routes — operator action, a failsafe).
    if (_state == RUN_IN && mode == QStringLiteral("Hold")) {
        if (_arrivalPredicate()) {
            _setState(ON_STATION);
            emit onStation();
        } else {
            qWarning("VehicleAgent: entered Hold but arrival predicate unmet; not ON_STATION");
        }
    }
}

bool VehicleAgent::_arrivalPredicate() const
{
    // ICD §7.4 (S1 form). Condition 1 (mode 9->3) is established by the caller.
    // Conditions 2 and 3: horizontal distance to the slot coordinate within
    // acceptance, and altitude within tolerance of the hold height H. S2 refines
    // the radius with NAV_ACC_RAD and the altitude test with the AMSL form.
    if (!_vehicle || !_ring || !_slot) {
        return false;
    }
    const QGeoCoordinate here = _vehicle->coordinate();
    const QGeoCoordinate slotCoord = _ring->slotCoordinate(_slot->bearingDeg());
    if (!here.isValid() || !slotCoord.isValid()) {
        return false;
    }
    if (here.distanceTo(slotCoord) > kArrivalRadiusM) {
        return false;
    }
    if (qAbs(_currentRelAlt() - _ring->heightM()) > kAltToleranceM) {
        return false;
    }
    return true;
}
