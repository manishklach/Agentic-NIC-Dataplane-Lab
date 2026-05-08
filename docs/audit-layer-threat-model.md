# Audit Layer Threat Model

This document defines the threat model for the reasoning and audit log path in an agentic NIC system.

## Why This Exists

The reasoning log is valuable only if operators know:

- who can read it
- who can export it
- who can verify it
- who cannot forge, suppress, or rewrite it

Without that clarity, “hardware-isolated reasoning logs” are just marketing language.

## Assets

The audit layer protects:

- decision records for local agent actions
- guardian approvals, rewrites, and rejections
- fail-safe transitions
- tenant-scoped mutation history
- integrity metadata for log segments

## Actors

### Trusted

- NIC-local guardian and audit pipeline
- host-side attestation or verification tool with read-only access
- cluster control plane with approved export privileges

### Partially Trusted

- host OS and kernel
- hypervisor
- tenant control-plane agents

### Untrusted

- tenant workloads
- compromised user-space services
- host processes without explicit log access
- external observers on the network fabric

## Read Access Model

The recommended model is:

- `NIC-local write authority only`
- `host-readable with explicit privilege`
- `tenant-visible only through filtered exports`

That means:

- the NIC writes log records
- the host can request log reads through a controlled interface
- tenants do not read raw global logs
- multi-tenant environments expose only scoped, redacted, or aggregated records

## Threats

### 1. Host forgery

The host tries to invent or alter a reasoning record after the fact.

Mitigations:

- signed or MAC-protected records
- hash-chained log segments
- NIC-side sequence generation

### 2. Host suppression

The host tries to hide a guardian rejection or fail-safe event.

Mitigations:

- append-only NIC-side retention window
- export checkpoints with sequence continuity
- attestation of last committed segment

### 3. Cross-tenant disclosure

One tenant learns about another tenant’s queueing or policy behavior through the logs.

Mitigations:

- tenant scoping
- redacted exports
- per-tenant view materialization instead of global raw reads

### 4. Replay or truncation

An attacker replays older records or truncates the visible tail.

Mitigations:

- monotonic counters
- signed segment boundaries
- verifier checks for sequence gaps

### 5. Side-channel leakage

Even if payloads are hidden, timing or policy events may leak information about other tenants.

Mitigations:

- event aggregation for tenant-visible views
- policy-domain isolation
- minimum export granularity

## Operational Answer To “Who Can Read The Logs?”

The repo should assume this default answer:

- platform operators can read the raw log
- verification tooling can validate integrity
- tenants can read only filtered tenant-scoped exports
- the host OS cannot forge or silently suppress records

That is the minimum viable trust posture for enterprise use.
