# Supersonic OT - Base Implementation

This directory contains the Base Supersonic OT implementation.

## Compilation

To compile the supersonic OT test driver, run:

```bash
g++ -std=gnu++20 \
    -O2 \
    -Wall -Wextra \
    -pthread \
    -mavx -mavx2 \
    supersonic_ot.cpp \
    -o supersonic_ot_test \
    -lboost_system
```

## Running Tests

### Manual Execution
You can run the three parties manually in separate terminals:
```bash
./supersonic_ot_test p2 [iterations] # Start Helper first
./supersonic_ot_test p0 [iterations] # Start Sender
./supersonic_ot_test p1 [iterations] # Start Receiver
```
`iterations` is optional and defaults to 20.

### Automated Testing
For easier verification, use the provided bash script:
```bash
./run_multi_test.sh [iterations]
```
This script runs all three parties in the background and reports if all tests passed from the Receiver's perspective.
