# AF_XDP Starter

This directory holds a minimal starter for an `AF_XDP` userspace fast path.

Goals:

- establish socket and queue ownership cleanly
- make zero-copy capability checks explicit
- leave room for an XDP classifier and queue-specific workers
- document the missing UMEM, fill-ring, completion-ring, and wakeup mechanics plainly

Files:

- `main.c`: userspace socket bring-up skeleton
- `xdp_pass.c`: minimal XDP companion program

The current code is intentionally a scaffold, not a finished dataplane. The next meaningful step is a real UMEM-backed receive loop plus XSKMAP redirect plumbing.
