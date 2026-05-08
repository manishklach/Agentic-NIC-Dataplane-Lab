# Safety and Guardrails

Agentic behavior is valuable only if it stays inside a hard operational envelope. This document defines the `Guardian Layer` that sits between NIC-local reasoning and actual dataplane changes.

## Guardian Goals

The guardian exists to ensure that local autonomy cannot:

- break tenant isolation
- violate minimum connectivity guarantees
- starve critical control traffic
- disable audit visibility
- create unstable oscillations in queueing or routing policy

## Safety Model

Every proposed action should be checked against:

1. `Connectivity Invariants`
- keep management reachability intact
- never drop mandatory control traffic classes
- preserve an emergency recovery path

2. `Isolation Invariants`
- tenant A cannot consume tenant B reserved minimums
- one service class cannot remove another service class from the schedule entirely
- policy changes are scoped to the flows and queues the agent is allowed to govern

3. `Rate and Magnitude Limits`
- queue-weight changes are bounded
- pacing changes are clamped
- reclassification frequency is rate-limited
- fallback mode is triggered after repeated rejected actions or instability

4. `Audit Invariants`
- every proposed mutation gets a record
- guardian rejections are logged too
- logs cannot be disabled by normal host actions

## Recommended Enforcement Modes

- `Approve`: action is safe as requested
- `Rewrite`: action is too broad or too aggressive, but a clamped version is safe
- `Reject`: action violates a hard rule
- `Fail-safe`: repeated instability or invalid actions force deterministic policy

## Deterministic Fallback

The repo should explicitly assume a fallback mode that:

- restores a static queue profile
- disables autonomous mutations temporarily
- keeps traffic on safe baseline rules
- continues emitting audit events so operators know fallback occurred

## Tail-Latency Protection Requirement

The guardian should not only prevent catastrophic actions. It should also avoid becoming the source of a latency regression itself.

That means the repo should eventually demonstrate:

- guardian preemption happens before queue collapse, not after
- high-priority service classes keep their SLO envelope during intervention
- repeated rewrites or rejections do not create oscillation or scheduler thrash

The practical validation path should include `bpftrace` or equivalent tracing around:

- guardian wakeups
- dataplane mutation attempts
- fail-safe transitions
- run-queue delay for protected services

## Implementation Shapes

Possible enforcement surfaces include:

- hardware rule tables
- P4 or equivalent pipeline constraints
- eBPF/XDP policy guards
- firmware-level state machines
- signed host-provided policy capsules interpreted by the NIC

The exact implementation can vary, but the invariant is the same: the guardian must be non-bypassable from the perspective of the local agent.
