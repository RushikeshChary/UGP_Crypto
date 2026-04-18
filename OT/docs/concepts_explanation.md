# OT and OT Extension: Concepts and Implementation

This document provides a clear explanation of the Oblivious Transfer (OT) protocols implemented in this project.

## 1. Oblivious Transfer (OT) Basics
Oblivious Transfer is a fundamental cryptographic building block where:
- **Sender** has two messages $(m_0, m_1)$.
- **Receiver** has a choice bit $b \in \{0, 1\}$.
- **Outcome**: Receiver obtains $m_b$ but learns nothing about $m_{1-b}$. Sender learns nothing about $b$.

---

## 2. SuperSonic OT (The Base Protocol)
SuperSonic OT is a **3-party OT protocol** involving a Sender (P0), a Receiver (P1), and a Helper (P2).

### The Concept (3-Party Privacy)
In traditional 2-party OT, complex public-key cryptography is required. SuperSonic OT simplifies this by using a semi-honest third party (the Helper) who never sees the actual messages but helps "blind" the transfer.

### The Implementation Steps
1. **Blinding**: The Receiver (P1) generates two random keys $(k_0, k_1)$ and sends them to the Sender (P0).
2. **Choice Splitting**: The Receiver splits their choice bit $b$ into two secret shares $s_1, s_2$ such that $s_1 \oplus s_2 = b$.
3. **Sender Phase**: P0 XORs their messages with the keys $(k_0, k_1)$, swaps them based on $s_1$, and sends the ciphertexts to the Helper (P2).
4. **Helper Phase**: P2 swaps the ciphertexts based on $s_2$ and sends only the "first" one to the Receiver.
5. **Recovery**: The Receiver uses the correct key to decrypt the message they chose.

---

## 3. IKNP OT Extension
Base OTs are computationally expensive. **OT Extension** allows us to perform millions of OTs using only a few (e.g., 128) base OTs.

### The Problem: Scale
If we need $10^6$ OTs, running $10^6$ SuperSonic OTs would be slow and require significant network overhead.

### The IKNP Solution
The IKNP protocol (implemented in `ot_extension.hpp`) works as follows:

1. **Role Reversal (Base Phase)**:
   - We perform 128 base OTs where the **Extension Receiver** acts as the **Base Sender**, and the **Extension Sender** acts as the **Base Receiver**.
   - These 128 OTs transfer small "seeds" (128-bit blocks).

2. **Seed Expansion (PRG)**:
   - Both parties use a **Pseudo-Random Generator (PRG)** to expand these short seeds into long bit-strings of length $m$ (the number of OTs needed).
   - We use AES-ECB for this expansion in `prg.hpp`.

3. **Matrix Transposition**:
   - The expanded strings form a matrix of size $m \times 128$.
   - To perform individual OTs, we need the **rows** of this matrix.
   - The `transpose_m_by_128` function flips the matrix so we can access these rows efficiently.

4. **Corrective XORs**:
   - The Receiver sends "correction" vectors $u_i$ to the Sender.
   - This ensures that for every OT $j \in [0, m)$, the Sender and Receiver share a common value only for the bit the Receiver chose.

5. **Encryption (Correlation-Robust Hash)**:
   - We cannot use the raw matrix bits directly as they might be biased.
   - We use `cr_hash` ($H(j, x) = AES(x \oplus j) \oplus (x \oplus j)$) to generate uniform, independent masks for every OT.

---

## 4. Why this implementation is powerful
- **Efficiency**: Bit-matrix transposition and AES-based PRG expansion are extremely fast on modern CPUs with AES-NI instructions.
- **Combined Benefits**: It combines the security of a 3-party base OT (SuperSonic) with the massive scalability of 2-party IKNP extension.
- **Decoupled Verification**: The system allows parties to log their operations for off-line verification, ensuring the protocol was followed correctly without adding network overhead to the core logic.
