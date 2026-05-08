# AF_XDP Starter

This directory holds a minimal starter for an `AF_XDP` userspace fast path.

Goals:

- establish socket and queue ownership cleanly
- make zero-copy capability checks explicit
- leave room for an XDP classifier and queue-specific workers

The current `main.c` is intentionally a scaffold, not a finished dataplane.
