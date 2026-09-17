#include "OrchestrationManager.h"
#include "StandoffRing.h"
#include "RingSlot.h"
#include "VehicleAgent.h"

#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "QmlObjectListModel.h"

#include <QtCore/QApplicationStatic>

Q_APPLICATION_STATIC(OrchestrationManager, _orchestrationManagerInstance);

OrchestrationManager::OrchestrationManager(QObject *parent)
    : QObject(parent)
    , _ring(new StandoffRing(this))
    , _agents(new QmlObjectListModel(this))
{
    // Track the fleet for the object's whole life (ICD §3.1). Never reads
    // the active-vehicle pointer (RE2); agents are pinned per Vehicle*.
    MultiVehicleManager *mvm = MultiVehicleManager::instance();
    (void) connect(mvm, &MultiVehicleManager::vehicleAdded,   this, &OrchestrationManager::_onVehicleAdded);
    (void) connect(mvm, &MultiVehicleManager::vehicleRemoved, this, &OrchestrationManager::_onVehicleRemoved);
}

OrchestrationManager *OrchestrationManager::instance()
{
    return _orchestrationManagerInstance();
}

void OrchestrationManager::_setMissionState(MissionState s)
{
    if (_missionState == s) {
        return;
    }
    _missionState = s;
    emit missionStateChanged(_missionState);
}

void OrchestrationManager::beginMission()
{
    _setMissionState(DEFINING_RING);
}

void OrchestrationManager::setTarget(const QGeoCoordinate &target)
{
    _ring->setGeometry(target, _ring->radiusM(), _ring->heightM());
    emit ringChanged();
}

void OrchestrationManager::setRing(double radiusMeters, double heightMeters)
{
    _ring->setGeometry(_ring->target(), radiusMeters, heightMeters);
    emit ringChanged();
    _setMissionState(ASSIGNING_SLOTS);
}

bool OrchestrationManager::assignVehicleToSlot(int vehicleId, int slotIndex)
{
    Q_UNUSED(slotIndex);
    Vehicle *v = MultiVehicleManager::instance()->getVehicleById(vehicleId);
    if (!v) {
        emit advisory(QStringLiteral("assignVehicleToSlot: no vehicle %1").arg(vehicleId));
        return false;
    }
    if (_agentForVehicle(vehicleId)) {
        return false;   // one agent per vehicle (bijection, ICD §3.4)
    }
    auto *agent = new VehicleAgent(v, this);
    agent->setRing(_ring);   // pin the derived ring geometry (target, R, H) for commit
    auto *slot  = new RingSlot(this);
    slot->setAgent(agent);
    slot->setSlotState(RingSlot::ASSIGNED);
    agent->assignSlot(slot);
    _agents->append(agent);
    return true;
}

void OrchestrationManager::proposeAssignment()
{
    // S2: SlotAssignmentSolver — min total run-in path, cyclic-order (no-crossing).
    emit advisory(QStringLiteral("proposeAssignment: SlotAssignmentSolver lands in S2"));
}

void OrchestrationManager::reviewPlan()
{
    _setMissionState(PLAN_REVIEW);
}

void OrchestrationManager::runPreflight()
{
    // S2: FailsafeVerifier gates ARM (RE6). armable stays false until it passes.
    _setMissionState(PREFLIGHT);
    emit advisory(QStringLiteral("runPreflight: FailsafeVerifier lands in S2"));
}

void OrchestrationManager::execute()
{
    if (!_armable) {
        emit advisory(QStringLiteral("execute blocked: not armable (RE6)"));
        return;   // no-op while !armable (ICD §3.1)
    }
    _setMissionState(EXECUTING);
    // S1 transit-level stratification (relative height above home). A distinct level
    // per agent so the firmware-owned run-in (F3) is vertically separated by
    // construction; TransitPlanner formalises this (AMSL, ΔH, ETA, staggered
    // CommitSchedule) in S2.
    const double baseTransit = _ring->heightM() + 10.0;   // clear the common hold height H
    const double deltaH      = 10.0;                       // ICD §6.3 workhorse layer
    for (int i = 0; i < _agents->count(); ++i) {
        if (auto *agent = qobject_cast<VehicleAgent *>(_agents->get(i))) {
            const double transit = baseTransit + (i * deltaH);
            agent->setTransitLevel(transit);
            if (agent->slot()) {
                agent->slot()->setTransitLevel(transit);
            }
            agent->beginCommit();   // staggered CommitSchedule offsets land in S2
        }
    }
}

void OrchestrationManager::holdAll()
{
    // Operator-authorised HOLD ALL — the only autonomous-capable mode change (RE4).
    for (int i = 0; i < _agents->count(); ++i) {
        if (auto *agent = qobject_cast<VehicleAgent *>(_agents->get(i))) {
            agent->hold();
        }
    }
}

void OrchestrationManager::terminate()
{
    _setMissionState(TERMINATING);
}

void OrchestrationManager::reslot(int vehicleId, double newBearingDeg)
{
    Q_UNUSED(newBearingDeg);
    if (auto *agent = _agentForVehicle(vehicleId)) {
        Q_UNUSED(agent);   // re-commit from Hold repeats §7.1 with new bearing (S2)
        _setMissionState(RESLOTTING);
    }
}

void OrchestrationManager::_onVehicleAdded(Vehicle *vehicle)
{
    Q_UNUSED(vehicle);   // a newly-appeared airframe becomes assignable (ICD §3.1)
}

void OrchestrationManager::_onVehicleRemoved(Vehicle *vehicle)
{
    if (!vehicle) {
        return;
    }
    if (auto *agent = _agentForVehicle(vehicle->id())) {
        agent->markLinkLost();   // -> LINK_LOST; manager tears down (ICD §3.2)
        _degraded = true;
        emit degradedChanged(_degraded);
    }
}

VehicleAgent *OrchestrationManager::_agentForVehicle(int vehicleId) const
{
    for (int i = 0; i < _agents->count(); ++i) {
        auto *agent = qobject_cast<VehicleAgent *>(_agents->get(i));
        if (agent && agent->vehicle() && agent->vehicle()->id() == vehicleId) {
            return agent;
        }
    }
    return nullptr;
}

void OrchestrationManager::debugForceArmable(bool armable)
{
    if (_armable == armable) {
        return;
    }
    _armable = armable;
    emit armableChanged(_armable);
}
