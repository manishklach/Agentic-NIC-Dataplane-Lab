# Reasoning Log Design

If an autonomous NIC changes its own dataplane behavior, operators need to know what changed and why. This document describes a hardware-isolated reasoning log for agent-driven dataplane decisions.

## Why The Log Matters

Without an audit trail, “agentic” networking becomes difficult to trust:

- debugging is harder
- tenant disputes are harder to resolve
- safety incidents are harder to investigate
- operators cannot distinguish a bug from an intended adaptive action

The log is therefore not optional. It is part of the architecture.

## Design Requirements

The reasoning log should be:

- append-only in normal operation
- host-readable for diagnosis
- not host-forgeable
- scoped by tenant and policy domain
- efficient enough to record fast local decisions without stalling the dataplane

## Event Model

Each event should contain:

- timestamp or monotonic sequence
- observed local state summary
- action proposal
- guardian decision
- applied dataplane mutation
- tenant or queue scope
- reason code or policy identifier

Example event classes:

- `QUEUE_WEIGHT_ADJUST`
- `FLOW_CLASS_ESCALATE`
- `BULK_PATH_THROTTLE`
- `GUARDIAN_REJECT`
- `FAILSAFE_ENTER`
- `FAILSAFE_EXIT`

## Trust Model

The host may read the log, export it, or correlate it with software telemetry, but it should not be able to silently rewrite or suppress it.

Possible trust anchors:

- dedicated NIC-side memory region
- signed log records
- hash-chained event segments
- periodic host-visible checkpoints with integrity metadata

## Read Access Question

One of the most important unresolved operational questions is simple:

`Who can read the reasoning logs?`

The recommended answer is:

- platform operators can read raw logs
- verification tooling can validate integrity
- tenants should receive only filtered or tenant-scoped exports
- ordinary host processes should not gain blanket raw-log access

The fuller threat model for that read path lives in:

- [`./audit-layer-threat-model.md`](./audit-layer-threat-model.md)

## Minimal Patent-Relevant Claim Shape

A strong claim direction is not just “the NIC logs events.” It is:

- a NIC-local autonomous decision engine
- a deterministic validation shell
- and a tamper-resistant reasoning log that records both proposed and applied actions

That combination is much more differentiated than generic telemetry.
