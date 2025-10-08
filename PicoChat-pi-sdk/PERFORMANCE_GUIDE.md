# PicoChat Performance Guide

## Optimized Configuration Summary

After extensive testing and optimization, the system has been tuned from **50% data loss** to **98-100% success rate** with improved throughput.

## Performance Modes

### Mode 1: RELIABLE (Default)
**Best for: Production, critical data, zero data loss required**

- **Success Rate:** 100%
- **Throughput:** 0.16 KB/s (1.28 Kbps)
- **Recommended Delay:** 25ms between packets
- **Configuration:** Already enabled in `packets/packets.hpp`

```cpp
#define MODE_RELIABLE  // Uncomment this line
// #define MODE_HIGH_PERFORMANCE  // Comment this line
```

**Test Command:**
```bash
python3 controlled_test.py /dev/tty.usbmodem101 /dev/tty.usbmodem1101 100 25
```

---

### Mode 2: HIGH_PERFORMANCE
**Best for: Maximum throughput, 2% data loss acceptable**

- **Success Rate:** 98%
- **Throughput:** 0.17 KB/s (1.36 Kbps)
- **Recommended Delay:** 23ms between packets
- **Configuration:** Edit `packets/packets.hpp`

```cpp
// #define MODE_RELIABLE  // Comment this line
#define MODE_HIGH_PERFORMANCE  // Uncomment this line
```

**Test Command:**
```bash
python3 controlled_test.py /dev/tty.usbmodem101 /dev/tty.usbmodem1101 100 23
```

---

## Performance Comparison

| Delay | Success Rate | Throughput | Improvement | Use Case |
|-------|--------------|------------|-------------|----------|
| 35ms  | 100%         | 0.13 KB/s  | baseline    | Initial config |
| **25ms**  | **100%**     | **0.16 KB/s** | **+23%**    | **RELIABLE MODE** ✅ |
| **23ms**  | **98%**      | **0.17 KB/s** | **+31%**    | **HIGH_PERFORMANCE MODE** ⚡ |
| 20ms  | 91%          | 0.18 KB/s  | +38%        | Too aggressive ⚠️ |

---

## Current Hardware Configuration

### LoRa Parameters
- **Frequency:** 2.4 GHz
- **Spreading Factor:** SF5
- **Bandwidth:** 812.5 kHz
- **Output Power:** 10 dBm
- **CRC:** Enabled

### Packet Structure
- **Payload Size:** 8 bytes
- **Packet Size:** 10 bytes (2 byte header + 8 byte payload)
- **ACK Packet Size:** 5 bytes (2 byte header + 3 byte payload)

### Protocol Settings
- **ACK Timeout:** 100ms
- **ACK Retries:** 2
- **TX Processing Delay:** 3ms
- **CSMA:** Disabled (removed for better ACK performance)

---

## Testing Results Timeline

### Initial State (Before Optimization)
- ❌ **50-58% data loss**
- 🐌 **0.08 KB/s effective throughput**
- ⚠️ CSMA bugs causing ACK timeouts

### After Optimization
- ✅ **98-100% success rate** (depending on mode)
- 🚀 **0.16-0.17 KB/s throughput**
- ⚡ **+23% to +31% improvement**
- 🎯 Stable ACK protocol

---

## Key Optimizations Applied

1. **Fixed CSMA Bug:** Dead code in `doCSMA()` was never updating timer
2. **Removed CSMA:** Immediate transmission improved ACK response time
3. **Optimized ACK Timing:** Reduced timeout from 300ms → 100ms
4. **Circular Buffer:** 512-byte buffer prevents serial overflow
5. **Fixed Packet Sizes:** All packets use same size for reliable reception
6. **Tuned Packet Delay:** Found optimal 23-25ms sweet spot

---

## How to Switch Modes

1. Open `packets/packets.hpp`
2. Find the "PERFORMANCE MODE CONFIGURATION" section
3. Comment/uncomment the desired mode
4. Recompile and upload to both devices:
   ```bash
   # Compile
   ninja -C build
   
   # Upload to device 1 (in BOOTSEL mode)
   cp build/PicoChat.uf2 /Volumes/RP2040/
   
   # Upload to device 2 (in BOOTSEL mode)
   cp build/PicoChat.uf2 /Volumes/RP2040/
   ```

---

## Production Recommendations

### For Critical Applications
- Use **MODE_RELIABLE** (25ms delay)
- Zero data loss required
- Predictable performance
- Suitable for command/control applications

### For High-Throughput Applications
- Use **MODE_HIGH_PERFORMANCE** (23ms delay)
- 2% data loss acceptable
- 31% faster than baseline
- Suitable for sensor data streaming

### For Experimental/Testing
- Try delays between 20-25ms
- Monitor success rate vs throughput trade-off
- Adjust based on your specific requirements

---

## Troubleshooting

### Lower Success Rate Than Expected
- Check physical distance between modules
- Verify both devices have same firmware version
- Ensure proper antenna connection
- Try increasing delay by 2-5ms

### Timeout Issues
- Verify `ACK_TIMEOUT_MS` is set to 100ms
- Check that both devices are using same `PACKET_SIZE`
- Ensure delay ≥ 20ms in test scripts

### Serial Buffer Overflow
- Current buffer: 512 bytes
- If seeing data loss at serial level, increase `SERIAL_BUFFER_SIZE`

---

## Future Optimization Possibilities

1. **Larger Payloads:** Test 16, 32, or 64 byte payloads (requires variable packet size support)
2. **Pipeline ACKs:** Send next packet while waiting for ACK (more complex protocol)
3. **Adaptive Timing:** Dynamically adjust delay based on success rate
4. **Higher Bandwidth:** Try 1625 kHz (LR1121 supports this on 2.4GHz)
5. **Different SF:** Experiment with SF6 for different range/speed trade-offs

---

## Credits

Optimized through systematic testing and debugging on:
- **Hardware:** Raspberry Pi Pico + LR1121 LoRa modules
- **Date:** October 2025
- **Initial Performance:** 50% data loss
- **Final Performance:** 98-100% success rate, +23-31% throughput improvement
