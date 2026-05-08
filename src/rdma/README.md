# RDMA Starter

This directory holds a minimal verbs-based starter for a bulk transfer path.

Goals:

- keep the queue pair and memory registration flow visible
- make room for a registration-cost benchmark
- provide a simple shape for later `WRITE`, `READ`, or `SEND` microbenchmarks

The current sample is deliberately incomplete and should be extended with proper queue-pair state transitions and peer connection setup.
