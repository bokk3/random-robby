---
trigger: model_decision
description: "Governs local split-screen viewport management, OnlineSubsystem parties, friend lists, chat, and atomic player/vendor trading replication."
---

# Split-Screen & Online Multiplayer Invariants

1. **Dual Co-Op Parity (Local Split-Screen + Online)**:
   - Every player-facing system (HUD, Flashlight, Sensory Post-Process, Vendor UI, Resting Ground state) must query `ULocalPlayer` and `APlayerState` rather than assuming `GetFirstPlayerController()` or player index 0.
   - Sensory post-process effects (Dark Vision, Hearing Pulses) must be scoped per-viewport via ViewExtension / per-camera PostProcessVolume blend weights so Player 1's sensory buffs never bleed into Player 2's split-screen viewport.
2. **Social & Session Layer**:
   - Use UE `CommonUser` / `CommonSession` and `OnlineSubsystem` (OSSv2 compatible) for Party invites, Friend List presence polling, and text/voice Chat channels.
3. **Transactional Trading (Player-to-Player & Vendor)**:
   - Never mutate inventory arrays directly from client RPCs.
   - All trades (at Vendors or between party members in Resting Grounds) must execute through a server-authoritative two-phase commit (`FTradeTransaction`: Lock Offer -> Validate Item GUIDs & Currency -> Atomic Swap -> Client Ack).