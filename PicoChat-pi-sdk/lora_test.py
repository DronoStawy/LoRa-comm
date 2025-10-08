#!/usr/bin/env python3
"""
LoRa Communication Test Tool
Tests packet-based communication with integrity checking
"""

import serial
import time
import sys
import threading
from collections import defaultdict

class LoRaCommTester:
    def __init__(self, port_tx, port_rx, baudrate=115200, packet_size=8):
        self.port_tx = port_tx
        self.port_rx = port_rx
        self.baudrate = baudrate
        self.packet_size = packet_size
        
        # Statistics
        self.packets_sent = 0
        self.bytes_sent = 0
        self.packets_received = 0
        self.bytes_received = 0
        self.received_sequences = []
        self.duplicates = 0
        self.running = False
        
    def receiver_thread(self, ser_rx):
        """Thread that receives data and analyzes packets"""
        buffer = bytearray()
        
        while self.running:
            try:
                if ser_rx.in_waiting > 0:
                    data = ser_rx.read(ser_rx.in_waiting)
                    if data:
                        buffer.extend(data)
                        self.bytes_received += len(data)
                else:
                    time.sleep(0.001)  # Small delay to prevent busy wait
            except Exception as e:
                print(f"\nReceiver error: {e}")
                break
                
                # Process complete packets
                while len(buffer) >= self.packet_size:
                    packet = buffer[:self.packet_size]
                    buffer = buffer[self.packet_size:]
                    
                    # Try to extract sequence number
                    try:
                        # Assuming first 4 bytes contain sequence in ASCII
                        seq_str = packet[:4].decode('ascii', errors='ignore')
                        if seq_str.isdigit():
                            seq = int(seq_str)
                            if seq in self.received_sequences:
                                self.duplicates += 1
                            else:
                                self.received_sequences.append(seq)
                    except:
                        pass
                    
                    self.packets_received += 1
    
    def run_interactive_test(self, total_packets=100, delay_ms=50):
        """Run interactive test with detailed statistics"""
        try:
            print(f"\n{'='*60}")
            print(f"LoRa Communication Test")
            print(f"{'='*60}")
            print(f"TX Port: {self.port_tx}")
            print(f"RX Port: {self.port_rx}")
            print(f"Baudrate: {self.baudrate}")
            print(f"Packet Size: {self.packet_size} bytes")
            print(f"Total Packets: {total_packets}")
            print(f"Delay between packets: {delay_ms}ms")
            print(f"{'='*60}\n")
            
            # Open serial ports
            ser_tx = serial.Serial(self.port_tx, self.baudrate, timeout=1)
            ser_rx = serial.Serial(self.port_rx, self.baudrate, timeout=1)
            
            # Clear buffers
            time.sleep(0.5)
            ser_tx.reset_input_buffer()
            ser_tx.reset_output_buffer()
            ser_rx.reset_input_buffer()
            ser_rx.reset_output_buffer()
            time.sleep(0.5)
            
            # Start receiver thread
            self.running = True
            rx_thread = threading.Thread(target=self.receiver_thread, args=(ser_rx,))
            rx_thread.daemon = True
            rx_thread.start()
            
            print("Starting transmission...\n")
            start_time = time.time()
            
            # Send packets with sequence numbers
            for seq in range(total_packets):
                # Create packet with sequence number padded to 4 chars + filler
                seq_str = f"{seq:04d}"
                filler = "X" * (self.packet_size - 4)
                packet = (seq_str + filler).encode('ascii')[:self.packet_size]
                
                ser_tx.write(packet)
                self.packets_sent += 1
                self.bytes_sent += len(packet)
                
                # Show progress every 10 packets
                if (seq + 1) % 10 == 0:
                    elapsed = time.time() - start_time
                    success_rate = (self.packets_received / self.packets_sent * 100) if self.packets_sent > 0 else 0
                    throughput = (self.bytes_received / elapsed / 1024) if elapsed > 0 else 0
                    
                    print(f"Progress: {seq+1}/{total_packets} packets | "
                          f"Sent: {self.packets_sent} | "
                          f"Received: {self.packets_received} | "
                          f"Success: {success_rate:.1f}% | "
                          f"Throughput: {throughput:.2f} KB/s")
                
                # Wait before next packet
                time.sleep(delay_ms / 1000.0)
            
            # Wait for remaining packets
            print("\nWaiting for remaining packets...")
            time.sleep(2)
            
            # Stop receiver
            self.running = False
            rx_thread.join(timeout=1)
            
            # Calculate final statistics
            elapsed_time = time.time() - start_time
            self.print_results(elapsed_time)
            
            # Close ports
            ser_tx.close()
            ser_rx.close()
            
        except serial.SerialException as e:
            print(f"\nError: {e}")
            sys.exit(1)
        except KeyboardInterrupt:
            print("\n\nTest interrupted by user")
            self.running = False
            sys.exit(0)
    
    def print_results(self, elapsed_time):
        """Print detailed test results"""
        print(f"\n{'='*60}")
        print(f"TEST RESULTS")
        print(f"{'='*60}")
        print(f"Duration:          {elapsed_time:.2f} seconds")
        print(f"\nPackets:")
        print(f"  Sent:            {self.packets_sent}")
        print(f"  Received:        {self.packets_received}")
        print(f"  Lost:            {self.packets_sent - self.packets_received}")
        print(f"  Duplicates:      {self.duplicates}")
        
        if self.packets_sent > 0:
            success_rate = (self.packets_received / self.packets_sent) * 100
            loss_rate = ((self.packets_sent - self.packets_received) / self.packets_sent) * 100
            print(f"  Success Rate:    {success_rate:.2f}%")
            print(f"  Loss Rate:       {loss_rate:.2f}%")
        
        print(f"\nBytes:")
        print(f"  Sent:            {self.bytes_sent} ({self.bytes_sent/1024:.2f} KB)")
        print(f"  Received:        {self.bytes_received} ({self.bytes_received/1024:.2f} KB)")
        print(f"  Lost:            {self.bytes_sent - self.bytes_received} bytes")
        
        if elapsed_time > 0:
            tx_throughput = (self.bytes_sent / elapsed_time) / 1024
            rx_throughput = (self.bytes_received / elapsed_time) / 1024
            print(f"\nThroughput:")
            print(f"  TX:              {tx_throughput:.2f} KB/s ({tx_throughput*8/1000:.3f} Mbps)")
            print(f"  RX:              {rx_throughput:.2f} KB/s ({rx_throughput*8/1000:.3f} Mbps)")
        
        # Analyze sequence gaps
        if len(self.received_sequences) > 0:
            self.received_sequences.sort()
            missing = []
            for i in range(self.packets_sent):
                if i not in self.received_sequences:
                    missing.append(i)
            
            if missing:
                print(f"\nMissing packet sequences:")
                # Show first 20 missing
                if len(missing) <= 20:
                    print(f"  {missing}")
                else:
                    print(f"  First 20: {missing[:20]}")
                    print(f"  ... and {len(missing)-20} more")
        
        print(f"{'='*60}\n")

def main():
    if len(sys.argv) < 3:
        print("Usage: python lora_test.py <TX_PORT> <RX_PORT> [PACKETS] [DELAY_MS]")
        print("\nExample:")
        print("  python lora_test.py /dev/cu.usbmodem1101 /dev/cu.usbmodem1201 100 50")
        print("  python lora_test.py COM3 COM4 200 30")
        print("\nParameters:")
        print("  TX_PORT   - Serial port for transmitting")
        print("  RX_PORT   - Serial port for receiving")
        print("  PACKETS   - Number of packets to send (default: 100)")
        print("  DELAY_MS  - Delay between packets in ms (default: 50)")
        sys.exit(1)
    
    port_tx = sys.argv[1]
    port_rx = sys.argv[2]
    packets = int(sys.argv[3]) if len(sys.argv) > 3 else 100
    delay = int(sys.argv[4]) if len(sys.argv) > 4 else 50
    
    tester = LoRaCommTester(port_tx, port_rx, baudrate=115200, packet_size=8)
    tester.run_interactive_test(total_packets=packets, delay_ms=delay)

if __name__ == "__main__":
    main()
