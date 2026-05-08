# Multi-Tenant Agent Quotas

If multiple tenants or service domains share one SmartNIC or DPU, local autonomy must not become a new source of unfairness. This document outlines a tenant-aware quota model for agentic NIC control.

## Problem

An autonomous local agent could otherwise optimize for the loudest or hottest workload and accidentally:

- starve low-volume but latency-sensitive traffic
- let one tenant consume disproportionate queue or pacing resources
- bias bulk traffic against tenants without agentic acceleration privileges

## Design Goals

- preserve tenant isolation
- bound the scope of local optimization
- keep minimum service guarantees explicit
- make quota violations observable in the audit log

## Recommended Quota Dimensions

Each tenant or service domain should have bounded access to:

- queue share
- shaping or pacing headroom
- bulk-transfer budget
- number of simultaneous adaptive actions
- control-plane mutation rate

## Agent Authority Model

The agent should not own global dataplane freedom. It should own:

- per-tenant optimization within a quota envelope
- temporary class promotion under bounded rules
- local queue rebalancing that does not cross reserved minimums

The guardian should reject any action that:

- takes a tenant below its minimum reservation
- exceeds mutation rate limits
- repeatedly oscillates between states
- violates isolation scopes

## Example Policy Shape

- tenant A: low-latency inference, high priority, limited bulk allowance
- tenant B: background checkpointing, lower priority, high bulk allowance
- tenant C: shared retrieval service, protected minimum latency budget

The local agent may borrow unused headroom, but only:

- for a limited time window
- below a bounded cap
- with a logged reason and reversion policy

## Auditability

Quota-aware events should record:

- tenant identifier
- resource dimension affected
- requested delta
- approved delta
- reason for rejection or clamp

This turns fairness into something measurable rather than aspirational.
