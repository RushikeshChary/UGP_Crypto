#!/bin/bash

# Configuration
M=${1:-1024}

# Compile
g++ -std=gnu++20 -O2 -Wall -Wextra -pthread -mavx -mavx2 -maes ot_extension_test.cpp -o ot_extension_test -lboost_system

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

# Compile Verification Utility
g++ -std=gnu++20 -O2 -Wall -Wextra verify_ot.cpp -o verify_ot

# Run P2 (Helper)
./ot_extension_test p2 "$M" > p2.log 2>&1 &
P2_PID=$!

# Run P0 (Sender)
./ot_extension_test p0 "$M" > p0.log 2>&1 &
P0_PID=$!

# Run P1 (Receiver)
./ot_extension_test p1 "$M" > p1.log 2>&1
P1_STATUS=$?

# Cleanup background processes
kill $P2_PID $P0_PID 2>/dev/null

# Final Verification
if [ $P1_STATUS -eq 0 ]; then
    ./verify_ot
    VERIFY_STATUS=$?
    if [ $VERIFY_STATUS -eq 0 ]; then
        echo "OT Extension Test PASSED (Integrated Verification)"
    else
        echo "OT Extension Test FAILED during verification"
    fi
else
    cat p1.log
    echo "OT Extension Test FAILED (Protocol Execution)"
fi

exit $P1_STATUS
