#pragma once

#include <QtCore/QVector>

#include "OrchestrationTypes.h"

class Vehicle;

/// FailsafeVerifier — ICD §3.7 / architecture D8, D9; the gate for RE6.
/// Reads back the required failsafe parameter block on every participating vehicle
/// and checks fleet-wide MAV_SYS_ID uniqueness, returning a per-vehicle,
/// per-parameter finding list. The MECHANISM — read back, compare, block, name the
/// offender — is the contract; the numeric expectations are ratified in flight test.
class FailsafeVerifier
{
public:
    FailsafeResult verify(const QVector<Vehicle*>& fleet) const;
};
