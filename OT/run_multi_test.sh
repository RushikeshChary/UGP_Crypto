#!/bin/bash

# Kill any existing instances
pkill supersonic_ot_test || true

# Number of iterations
ITERATIONS=${1:-20}

echo "Starting Supersonic OT Multi-Iteration Test with $ITERATIONS iterations..."

# Start P2 (Helper)
./supersonic_ot_test p2 $ITERATIONS > p2.log 2>&1 &
P2_PID=$!

# Start P0 (Sender)
./supersonic_ot_test p0 $ITERATIONS > p0.log 2>&1 &
P0_PID=$!

# Start P1 (Receiver) and wait for it
./supersonic_ot_test p1 $ITERATIONS
P1_RET=$?

# Cleanup
kill $P2_PID $P0_PID 2>/dev/null || true

if [ $P1_RET -eq 0 ]; then
    echo "Test script finished."
else
    echo "Test script FAILED with return code $P1_RET."
fi
