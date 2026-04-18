# Supersonic OT & Extension Implementation

This directory contains the implementation of the 3-party Supersonic Oblivious Transfer (OT) protocol and its 2-party IKNP-based OT Extension.

## 📂 Directory Structure

- **`include/`**: C++ header files (`.hpp`).
- **`src/`**: Core protocol implementation and utilities.
- **`tests/`**: Test drivers and integration tests.
- **`scripts/`**: Bash scripts for automated testing and execution.
- **`docs/`**: Detailed explanations and cryptographic concepts.
- **`logs/`**: Runtime logs and debugging data (process output and internal state).

## 🚀 Getting Started

### 1. Prerequisites
- `g++` (supporting C++20)
- `boost_system` library
- Linux environment

### 2. Base Supersonic OT
To run the automated multi-iteration test for the base OT:
```bash
./scripts/run_multi_test.sh [iterations]
```
- **Iterations**: Defaults to 20.
- **Logs**: Check `logs/p0.log`, `logs/p1.log`, `logs/p2.log`.

### 3. OT Extension
To run the OT extension test (IKNP extension from Supersonic base OTs):
```bash
./scripts/run_multi_test_ext.sh [number_of_ots]
```
- **Number of OTs**: Defaults to 1024.
- **Logs**:
    - Process output: `logs/p0.log`, `logs/p1.log`, `logs/p2.log`.
    - Verification data: `logs/p0_data.log`, `logs/p1_data.log`.

## 🛠 Manual Compilation

If you wish to compile manually, use the following commands from the `OT/` root:

**Supersonic OT Test Driver:**
```bash
g++ -std=gnu++20 -O2 -Wall -Wextra -pthread -mavx -mavx2 \
    src/supersonic_ot.cpp -o supersonic_ot_test -lboost_system
```

**OT Extension Test Driver:**
```bash
g++ -std=gnu++20 -O2 -Wall -Wextra -pthread -mavx -mavx2 -maes \
    tests/ot_extension_test.cpp -o ot_extension_test -lboost_system
```

**Verification Utility:**
```bash
g++ -std=gnu++20 -O2 -Wall -Wextra src/verify_ot.cpp -o verify_ot
```

## 📝 Troubleshooting
If tests fail, refer to the logs in the `logs/` directory. The sender and receiver's perspectives are captured in `p0.log` and `p1.log`, while the helper's is in `p2.log`. Internal data consistency is checked using the `verify_ot` utility against `logs/p0_data.log` and `logs/p1_data.log`.
