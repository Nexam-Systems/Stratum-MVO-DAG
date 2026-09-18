#include "OrchestrationManager.h"
#include "StandoffRing.h"
#include "RingSlot.h"
#include "VehicleAgent.h"
#include "SlotAssignmentSolver.h"
#include "TransitPlanner.h"
#include "FailsafeVerifier.h"

#include "MultiVehicleManager.h"
#include "Vehicle.h"
#include "QmlObjectListModel.h"

#include <QtCore/QApplicationStatic>
#include <QtCore/QTimer>

namespace {
// Planner inputs. Ratified in flight test; the mechanism does not depend on them.
constexpr double kDeltaH           = 10.0;   // vertical stratification step (ICD §6.3)
constexpr double kNominalCruiseMps = 15.0;   // nominal run-in cruise for ETA/schedule
} // namespace

Q_APPLICATION_STATIC(OrchestrationManager, _orchestrationManagerInstance);

OrchestrationManager::OrchestrationManager(QObject *parent)
    : QObject(parent)
    , _ring(new StandoffRing(this))
    , _agents(new QmlObjectListModel(this))
{
    // Track the fleet for the object's whole life (ICD §3.1). Never reads the
    // active-vehicle pointer (RE2); agents are pinned per Vehicle*.
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

QList<double> OrchestrationManager::_otherBearings(const RingSlot *except) const
{
    QList<double> out;
    for (int i = 0; i < _agents->count(); ++i) {
        auto *agent = qobject_cast<VehicleAgent *>(_agents->get(i));
        if (agent && agent->slot() && agent->slot() != except) {
            out.append(agent->slot()->bearingDeg());
        }
    }
    return out;
}

void OrchestrationManager::_installBearingGuard(RingSlot *slot)
{
    // theta_min structural deconfliction (ICD §3.4/§6.2): a bearing is admissible
    // only if it respects theta_min against every OTHER assigned slot on the ring.
    slot->setBearingGuard([this, slot](double bearingDeg) {
        return _ring->bearingAllowed(bearingDeg, _otherBearings(slot));
    });
}

void OrchestrationManager::_clearAgents()
{
    // Slots are parented to their agent, so deleting the agents cascades to the slots.
    _agents->clearAndDeleteContents();
    _schedule.entries.clear();
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
    agent->setRing(_ring);                       // pin the derived ring geometry for commit
    auto *slot  = new RingSlot(agent);           // parented to the agent (teardown cascades)
    slot->setSlotState(RingSlot::ASSIGNED);
    _installBearingGuard(slot);                  // theta_min enforced from here on (ICD §3.4)
    slot->setAgent(agent);
    agent->assignSlot(slot);
    _agents->append(agent);
    return true;
}

void OrchestrationManager::proposeAssignment()
{
    // Gather up to three vehicles from the live fleet (ICD §2.1). No activeVehicle (RE2).
    QVector<Vehicle *> vs;
    QmlObjectListModel *model = MultiVehicleManager::instance()->vehicles();
    for (int i = 0; model && i < model->count() && vs.size() < 3; ++i) {
        if (auto *v = qobject_cast<Vehicle *>(model->get(i))) {
            vs.append(v);
        }
    }
    if (vs.isEmpty()) {
        emit advisory(QStringLiteral("proposeAssignment: no vehicles in the fleet"));
        return;
    }

    // Equal-spaced candidate bearings; the solver assigns vehicles to them.
    const int n = vs.size();
    QVector<double> bearings;
    bearings.reserve(n);
    for (int i = 0; i < n; ++i) {
        bearings.append(i * 360.0 / n);
    }

    SlotAssignmentSolver solver;
    const SlotAssignmentSolver::Assignment a = solver.propose(vs, *_ring, bearings);

    // Rebuild the agent set from the proposal.
    _clearAgents();
    QVector<VehicleAgent *> ags;
    for (const QPair<int, double> &pr : a.pairs) {
        Vehicle *v = MultiVehicleManager::instance()->getVehicleById(pr.first);
        if (!v) {
            continue;
        }
        auto *agent = new VehicleAgent(v, this);
        agent->setRing(_ring);
        auto *slot = new RingSlot(agent);
        slot->setSlotState(RingSlot::ASSIGNED);
        slot->setBearingDeg(pr.second);      // initial placement (guard not yet installed)
        _installBearingGuard(slot);          // theta_min enforced for subsequent edits
        slot->setAgent(agent);
        agent->assignSlot(slot);
        _agents->append(agent);
        ags.append(agent);
    }

    // Transit levels, path/ETA, and the staggered commit schedule (ICD §3.6, D7).
    TransitPlanner planner;
    const TransitPlan plan = planner.build(ags, *_ring, kDeltaH, kNominalCruiseMps);
    _schedule = plan.schedule;
    for (int i = 0; i < ags.size(); ++i) {
        if (i < plan.transitLevels.size()) {
            ags[i]->setTransitLevel(plan.transitLevels[i]);
            if (ags[i]->slot()) {
                ags[i]->slot()->setTransitLevel(plan.transitLevels[i]);
            }
        }
        const double pl  = (i < plan.pathLengths.size()) ? plan.pathLengths[i] : 0.0;
        const double eta = (i < plan.etas.size())        ? plan.etas[i]        : 0.0;
        ags[i]->setPlanMetrics(pl, eta);
    }

    emit advisory(QStringLiteral("proposeAssignment: %1 agent(s), total run-in %2 m, %3, theta_min %4 deg")
                      .arg(ags.size())
                      .arg(a.totalPathM, 0, 'f', 0)
                      .arg(a.crossingFree ? QStringLiteral("crossing-free")
                                          : QStringLiteral("CROSSING (flagged, not blocked)"))
                      .arg(_ring->thetaMinDeg(), 0, 'f', 1));
    _setMissionState(ASSIGNING_SLOTS);
}

void OrchestrationManager::reviewPlan()
{
    _setMissionState(PLAN_REVIEW);
}

void OrchestrationManager::runPreflight()
{
    // FailsafeVerifier gates ARM (RE6/D8/D9). armable follows the verdict.
    _setMissionState(PREFLIGHT);

    QVector<Vehicle *> fleet;
    for (int i = 0; i < _agents->count(); ++i) {
        if (auto *agent = qobject_cast<VehicleAgent *>(_agents->get(i))) {
            if (agent->vehicle()) {
                fleet.append(agent->vehicle());
            }
        }
    }

    FailsafeVerifier verifier;
    const FailsafeResult r = verifier.verify(fleet);

    for (const FailsafeFinding &f : r.findings) {
        emit advisory(QStringLiteral("PREFLIGHT %1: vehicle %2  %3  expected %4, actual %5")
                          .arg(f.blocking ? QStringLiteral("BLOCK") : QStringLiteral("note"))
                          .arg(f.vehicleId)
                          .arg(f.param, f.expected, f.actual));
    }

    const bool wasArmable = _armable;
    _armable = r.pass;
    if (_armable != wasArmable) {
        emit armableChanged(_armable);
    }

    emit advisory(QStringLiteral("PREFLIGHT %1 (%2 finding(s))")
                      .arg(r.pass ? QStringLiteral("PASS - armable")
                                  : QStringLiteral("BLOCKED - not armable"))
                      .arg(r.findings.size()));
}

void OrchestrationManager::execute()
{
    if (!_armable) {
        emit advisory(QStringLiteral("execute blocked: not armable (RE6)"));
        return;   // no-op while !armable (ICD §3.1)
    }
    _setMissionState(EXECUTING);

    // Staggered commit on the CommitSchedule offsets (D7). Transit levels were set
    // at proposeAssignment; each agent runs the ICD §7 commit sequence itself.
    for (int i = 0; i < _agents->count(); ++i) {
        auto *agent = qobject_cast<VehicleAgent *>(_agents->get(i));
        if (!agent) {
            continue;
        }
        double tOffset = 0.0;
        for (const QPair<int, double> &e : _schedule.entries) {
            if (agent->vehicle() && e.first == agent->vehicle()->id()) {
                tOffset = e.second;
                break;
            }
        }
        QTimer::singleShot(static_cast<int>(tOffset * 1000.0), agent, [agent]() {
            agent->beginCommit();
        });
    }
}

void OrchestrationManager::authorizeEngagement(bool authorized)
{
    // The second gate (Addendum A.5). Distinct from armable; the RE6 ARM gate is unchanged.
    if (_engageAuthorized == authorized) {
        return;
    }
    _engageAuthorized = authorized;
    emit engageAuthorizedChanged(_engageAuthorized);
    emit advisory(authorized ? QStringLiteral("engagement AUTHORISED (second gate set)")
                             : QStringLiteral("engagement authorisation cleared"));
}

void OrchestrationManager::engageAll(double staggerSeconds)
{
    // Coordinate Engagement (sub=21) on every ON_STATION agent (Addendum A), STAGGERED in time
    // by staggerSeconds between successive agents so the terminal dives do not converge on the
    // shared aimpoint simultaneously (flight-test 2026-09-19 showed <10 ms sync collapsing
    // pairwise separation to ~1 m). No abort path; no autonomous manoeuvre (RE4). Existing
    // single-vehicle abort is untouched.
    if (!_engageAuthorized) {
        emit advisory(QStringLiteral("engageAll blocked: engagement not authorised (Addendum A.5)"));
        return;
    }
    const double stagger = qMax(0.0, staggerSeconds);
    int order = 0;
    for (int i = 0; i < _agents->count(); ++i) {
        auto *agent = qobject_cast<VehicleAgent *>(_agents->get(i));
        if (agent && agent->state() == VehicleAgent::ON_STATION) {
            const int delayMs = static_cast<int>(order * stagger * 1000.0);
            QTimer::singleShot(delayMs, agent, [agent]() { agent->engage(); });
            ++order;
        }
    }
    emit advisory(QStringLiteral("engageAll: %1 ON_STATION agent(s), %2 s stagger (engage window %3 s)")
                      .arg(order)
                      .arg(stagger, 0, 'f', 1)
                      .arg(order > 0 ? (order - 1) * stagger : 0.0, 0, 'f', 1));
}

void OrchestrationManager::engage(int vehicleId)
{
    if (!_engageAuthorized) {
        emit advisory(QStringLiteral("engage blocked: engagement not authorised (Addendum A.5)"));
        return;
    }
    if (auto *agent = _agentForVehicle(vehicleId)) {
        agent->engage();
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
    if (auto *agent = _agentForVehicle(vehicleId)) {
        if (agent->slot()) {
            agent->slot()->setBearingDeg(newBearingDeg);   // theta_min-guarded (ICD §3.4)
        }
        _setMissionState(RESLOTTING);
        // Re-commit from Hold repeats the ICD §7.1 sequence with the new bearing.
        agent->beginCommit();
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
        agent->markLinkLost();   // -> LINK_LOST (ICD §3.2)
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
