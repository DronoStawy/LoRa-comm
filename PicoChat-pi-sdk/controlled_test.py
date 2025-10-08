#!/usr/bin/env python3
"""
Controlled LoRa Throughput Test
Sends data at controlled rate and measures success
"""

import serial
import time
import sys
import threading

class ControlledTester:
    def __init__(self, port_tx, port_rx, baudrate=115200):
        self.port_tx = port_tx
        self.port_rx = port_rx
        self.baudrate = baudrate
        self.bytes_received = 0
        self.running = False
        
    def receiver_thread(self, ser_rx):
        """Thread that receives and counts data"""
        while self.running:
            try:
                if ser_rx.in_waiting > 0:
                    data = ser_rx.read(ser_rx.in_waiting)
                    if data:
                        self.bytes_received += len(data)
                else:
                    time.sleep(0.01)
            except Exception as e:
                print(f"\nRX Error: {e}")
                break
    
    def run_test(self, chunk_size=8, chunks=100, delay_ms=200):
        """
        Run controlled throughput test
        
        Args:
            chunk_size: Bytes per chunk (matches PAYLOAD_SIZE)
            chunks: Number of chunks to send
            delay_ms: Milliseconds between chunks (should be > ACK_TIMEOUT)
        """
        try:
            print(f"\n{'='*60}")
            print(f"CONTROLLED THROUGHPUT TEST")
            print(f"{'='*60}")
            print(f"TX Port:       {self.port_tx}")
            print(f"RX Port:       {self.port_rx}")
            print(f"Chunk Size:    {chunk_size} bytes")
            print(f"Total Chunks:  {chunks}")
            print(f"Delay:         {delay_ms}ms")
            print(f"Total Data:    {chunk_size * chunks} bytes")
            print(f"Expected Time: ~{chunks * delay_ms / 1000:.1f}s")
            print(f"{'='*60}\n")
            
            # Open ports
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
            self.bytes_received = 0
            rx_thread = threading.Thread(target=self.receiver_thread, args=(ser_rx,))
            rx_thread.daemon = True
            rx_thread.start()
            
            print("Starting transmission...\n")
            start_time = time.time()
            bytes_sent = 0
            
            # Create test pattern
            test_chunk = b'X' * chunk_size
            
            # Send chunks with controlled delay
            for i in range(chunks):
                ser_tx.write(test_chunk)
                bytes_sent += chunk_size
                
                # Progress every 10 chunks
                if (i + 1) % 10 == 0:
                    elapsed = time.time() - start_time
                    success_rate = (self.bytes_received / bytes_sent * 100) if bytes_sent > 0 else 0
                    print(f"Progress: {i+1}/{chunks} | "
                          f"Sent: {bytes_sent}B | "
                          f"Received: {self.bytes_received}B | "
                          f"Success: {success_rate:.1f}%")
                
                # Wait between chunks - crucial for ACK protocol
                time.sleep(delay_ms / 1000.0)
            
            # Wait for remaining data
            print("\nWaiting for remaining packets...")
            time.sleep(2)
            
            # Stop receiver
            self.running = False
            rx_thread.join(timeout=1)
            
            # Calculate results
            elapsed_time = time.time() - start_time
            
            print(f"\n{'='*60}")
            print(f"RESULTS")
            print(f"{'='*60}")
            print(f"Duration:      {elapsed_time:.2f} seconds")
            print(f"Bytes Sent:    {bytes_sent} ({bytes_sent/1024:.2f} KB)")
            print(f"Bytes Received: {self.bytes_received} ({self.bytes_received/1024:.2f} KB)")
            print(f"Data Loss:     {bytes_sent - self.bytes_received} bytes ({(bytes_sent - self.bytes_received)/bytes_sent*100:.2f}%)")
            
            if bytes_sent > 0:
                success_rate = (self.bytes_received / bytes_sent) * 100
                print(f"Success Rate:  {success_rate:.2f}%")
            
            if elapsed_time > 0:
                tx_throughput = (bytes_sent / elapsed_time) / 1024
                rx_throughput = (self.bytes_received / elapsed_time) / 1024
                print(f"\nTX Throughput: {tx_throughput:.2f} KB/s")
                print(f"RX Throughput: {rx_throughput:.2f} KB/s")
            
            print(f"{'='*60}\n")
            
            # Close ports
            ser_tx.close()
            ser_rx.close()
            
        except Exception as e:
            print(f"\nError: {e}")
            import traceback
            traceback.print_exc()

def main():
    if len(sys.argv) < 3:
        print("Usage: python3 controlled_test.py <TX_PORT> <RX_PORT> [CHUNKS] [DELAY_MS]")
        print("\nExample:")
        print("  python3 controlled_test.py /dev/tty.usbmodem101 /dev/tty.usbmodem1101")
        print("  python3 controlled_test.py /dev/tty.usbmodem101 /dev/tty.usbmodem1101 200 150")
        print("\nParameters:")
        print("  CHUNKS    - Number of 8-byte chunks (default: 100)")
        print("  DELAY_MS  - Delay between chunks, should be > ACK_TIMEOUT (default: 200ms)")
        sys.exit(1)
    
    port_tx = sys.argv[1]
    port_rx = sys.argv[2]
    chunks = int(sys.argv[3]) if len(sys.argv) > 3 else 100
    delay = int(sys.argv[4]) if len(sys.argv) > 4 else 200
    
    tester = ControlledTester(port_tx, port_rx)
    tester.run_test(chunk_size=8, chunks=chunks, delay_ms=delay)

if __name__ == "__main__":
    main()
