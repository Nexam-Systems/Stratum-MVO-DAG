#include "FailsafeVerifier.h"
#include "Vehicle.h"
#include "ParameterManager.h"
#include "Fact.h"

#include <QtCore/QSet>
#include <QtCore/QString>

namespace {
// Required failsafe posture (architecture D8). Values ratified in flight test —
// change them HERE, never the mechanism.
constexpr int kStdfSeqA        = 1;   // approach_with_height = false
constexpr int kStdfSeqB        = 3;   // approach_with_height = false
constexpr int kNavRclActHold   = 1;   // PX4 AUTO_LOITER / Hold
constexpr int kComRclExceptBit = 2;   // COM_RCL_EXCEPT bit 1 (value & 2) exempts RC-loss RTL

Fact *readParam(Vehicle *v, const QString &name)
{
    if (!v || !v->parameterManager()) {
        return nullptr;
    }
    ParameterManager *pm = v->parameterManager();
    if (!pm->parameterExists(ParameterManager::defaultComponentId, name)) {
        return nullptr;
    }
    return pm->getParameter(ParameterManager::defaultComponentId, name);
}

FailsafeFinding missing(int vid, const QString &param, const QString &expected)
{
    return FailsafeFinding{ vid, param, expected, QStringLiteral("(absent / not loaded)"), true };
}
} // namespace

FailsafeResult FailsafeVerifier::verify(const QVector<Vehicle*> &fleet) const
{
    FailsafeResult result;
    result.pass = true;

    QVector<int> sysIds;   // parallel to `fleet`, for the uniqueness pass

    for (Vehicle *v : fleet) {
        if (!v) {
            sysIds.append(-1);
            continue;
        }
        const int vid = v->id();

        // --- STDF_SEQ in {1,3} — deconfliction-critical (D8/R4) ---
        if (Fact *f = readParam(v, QStringLiteral("STDF_SEQ"))) {
            const int seq = f->rawValue().toInt();
            if (seq != kStdfSeqA && seq != kStdfSeqB) {
                result.findings.append({ vid, QStringLiteral("STDF_SEQ"),
                                         QStringLiteral("1 or 3"),
                                         QString::number(seq), true });
            }
        } else {
            result.findings.append(missing(vid, QStringLiteral("STDF_SEQ"), QStringLiteral("1 or 3")));
        }

        // --- NAV_DLL_ACT != 0 — datalink loss must trigger something (D8/R2) ---
        if (Fact *f = readParam(v, QStringLiteral("NAV_DLL_ACT"))) {
            if (f->rawValue().toInt() == 0) {
                result.findings.append({ vid, QStringLiteral("NAV_DLL_ACT"),
                                         QStringLiteral("non-zero (Hold/RTL)"),
                                         QStringLiteral("0"), true });
            }
        } else {
            result.findings.append(missing(vid, QStringLiteral("NAV_DLL_ACT"), QStringLiteral("non-zero")));
        }

        // --- NAV_RCL_ACT — Hold, or COM_RCL_EXCEPT bit 1 set (D8/R3) ---
        {
            Fact *rcl = readParam(v, QStringLiteral("NAV_RCL_ACT"));
            Fact *exc = readParam(v, QStringLiteral("COM_RCL_EXCEPT"));
            if (!rcl) {
                result.findings.append(missing(vid, QStringLiteral("NAV_RCL_ACT"),
                                               QStringLiteral("Hold, or COM_RCL_EXCEPT bit1")));
            } else {
                const int  rclAct   = rcl->rawValue().toInt();
                const bool excepted = exc && (exc->rawValue().toInt() & kComRclExceptBit);
                if (rclAct != kNavRclActHold && !excepted) {
                    result.findings.append({ vid, QStringLiteral("NAV_RCL_ACT"),
                                             QStringLiteral("Hold (1) or COM_RCL_EXCEPT bit1 set"),
                                             QString::number(rclAct), true });
                }
            }
        }

        // --- COM_DLL_EXCEPT — must be an explicit decision (present, not defaulted-away) ---
        if (Fact *f = readParam(v, QStringLiteral("COM_DLL_EXCEPT"))) {
            result.findings.append({ vid, QStringLiteral("COM_DLL_EXCEPT"),
                                     QStringLiteral("explicit decision"),
                                     QString::number(f->rawValue().toInt()), false });   // recorded, non-blocking
        } else {
            result.findings.append(missing(vid, QStringLiteral("COM_DLL_EXCEPT"),
                                           QStringLiteral("explicit decision")));
        }

        // --- RTL_RETURN_ALT — present and positive (stratification is per-vehicle) ---
        if (Fact *f = readParam(v, QStringLiteral("RTL_RETURN_ALT"))) {
            const double alt = f->rawValue().toDouble();
            if (alt <= 0.0) {
                result.findings.append({ vid, QStringLiteral("RTL_RETURN_ALT"),
                                         QStringLiteral("> 0, per-vehicle stratified"),
                                         QString::number(alt), true });
            }
        } else {
            result.findings.append(missing(vid, QStringLiteral("RTL_RETURN_ALT"),
                                           QStringLiteral("> 0, stratified")));
        }

        // --- MAV_SYS_ID — collected for the fleet-wide uniqueness check (D9/R1) ---
        if (Fact *f = readParam(v, QStringLiteral("MAV_SYS_ID"))) {
            sysIds.append(f->rawValue().toInt());
        } else {
            sysIds.append(vid);   // fall back to transport id so a collision still surfaces
            result.findings.append(missing(vid, QStringLiteral("MAV_SYS_ID"),
                                           QStringLiteral("unique across fleet")));
        }
    }

    // --- Fleet-wide MAV_SYS_ID uniqueness (D9/R1) ---
    QSet<int> seen;
    for (int i = 0; i < fleet.size(); ++i) {
        if (i >= sysIds.size() || !fleet[i]) {
            continue;
        }
        const int sid = sysIds[i];
        if (seen.contains(sid)) {
            result.findings.append({ fleet[i]->id(), QStringLiteral("MAV_SYS_ID"),
                                     QStringLiteral("unique across fleet"),
                                     QStringLiteral("duplicate %1").arg(sid), true });
        }
        seen.insert(sid);
    }

    for (const FailsafeFinding &f : result.findings) {
        if (f.blocking) {
            result.pass = false;
            break;
        }
    }
    return result;
}
