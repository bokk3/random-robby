---
trigger: always_on
---

# Automated Build & Deterministic Verification Protocol

1. **Compile Before Completion**:
   - No C++ task group is complete until UnrealBuildTool (`UBT`) compiles the target with zero warnings (`bTreatWarningsAsErrors = true`).
2. **Headless Automation Smoke Tests**:
   - Every core mathematical system must include a lightweight `IMPLEMENT_SIMPLE_AUTOMATION_TEST` in `Source/DungeoneerTests/`:
     - `CompassQuantizerTest`: Verifies all 8 compass octants normalize to identical scalar speeds.
     - `ChronoLeechConservationTest`: Verifies player speed loss equals zombie speed gain across boundary values (`0.0f` to `MaxSlow`).
     - `TradeTwoPhaseCommitTest`: Verifies simulated packet drops or inventory full states cleanly roll back both party inventories without item duplication.