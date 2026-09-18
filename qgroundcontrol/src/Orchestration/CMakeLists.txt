# ============================================================================
# Orchestration Module  (STRATUM Multi-Vehicle Orchestration — MVO)
# Ground-side coordination of up to three vehicles onto a standoff ring.
# Additive to the baseline: this module adds sources to the app target and
# reads the existing per-vehicle API. It changes no single-vehicle file (RE1).
# Contract: NXM-SW-ICD-STRATUM-MVO-001 §9.1
# ============================================================================

target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        OrchestrationManager.cc
        OrchestrationManager.h
        OrchestrationTypes.h
        VehicleAgent.cc
        VehicleAgent.h
        StandoffRing.cc
        StandoffRing.h
        RingSlot.cc
        RingSlot.h
        SlotAssignmentSolver.cc
        SlotAssignmentSolver.h
        TransitPlanner.cc
        TransitPlanner.h
        FailsafeVerifier.cc
        FailsafeVerifier.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
