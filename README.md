
- **EmulNet:** Emulates network message sending/receiving between peers.
- **P2P Layer:** Implements the membership protocol.
- **Application:** Simulates global time, starts/stops peers, and drives simulation.

---

## Features Implemented

- Peer joins via `JOINREQ` / `JOINREP` handshake.
- SWIM-style heartbeating with failure detection.
- Membership list maintenance with timestamps.
- Node failure detection with timeout mechanism.
- Message loss tolerance with high detection accuracy.

---

## How to Run

### Requirements

- C++11 or later
- Linux/macOS (preferred)
- GCC 4.7+ or Clang
- Make

### Build & Run

```bash
# Clone repository
git clone https://github.com/your-username/mp1-membership-protocol.git
cd mp1-membership-protocol

# Compile
make

# Run simulation
./mp1
