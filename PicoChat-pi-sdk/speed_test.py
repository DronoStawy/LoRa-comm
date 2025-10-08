#!/usr/bin/env python3
"""
Serial Port Throughput Measurement Tool
Measures data throughput between two serial ports
"""

import serial
import time
import sys
import threading

class SerialThroughputTester:
    def __init__(self, port_tx, port_rx, baudrate=115200, test_duration=10):
        self.port_tx = port_tx
        self.port_rx = port_rx
        self.baudrate = baudrate
        self.test_duration = test_duration
        self.bytes_received = 0
        self.running = False
        
    def receiver_thread(self, ser_rx):
        """Thread that receives data and counts bytes"""
        while self.running:
            if ser_rx.in_waiting > 0:
                data = ser_rx.read(ser_rx.in_waiting)
                self.bytes_received += len(data)
    
    def run_test(self):
        """Run the throughput test"""
        try:
            # Open serial ports
            print(f"Opening ports...")
            print(f"TX Port: {self.port_tx}")
            print(f"RX Port: {self.port_rx}")
            print(f"Baudrate: {self.baudrate}")
            
            ser_tx = serial.Serial(self.port_tx, self.baudrate, timeout=1)
            ser_rx = serial.Serial(self.port_rx, self.baudrate, timeout=1)
            
            # Clear buffers
            ser_tx.reset_input_buffer()
            ser_tx.reset_output_buffer()
            ser_rx.reset_input_buffer()
            ser_rx.reset_output_buffer()
            
            # Prepare test data (1KB chunk)
            test_data = b'X' * 1024
            
            # Start receiver thread
            self.running = True
            self.bytes_received = 0
            rx_thread = threading.Thread(target=self.receiver_thread, args=(ser_rx,))
            rx_thread.start()
            
            print(f"\nStarting throughput test for {self.test_duration} seconds...")
            print("Sending data...\n")
            
            start_time = time.time()
            bytes_sent = 0
            
            # Send data continuously for the test duration
            while time.time() - start_time < self.test_duration:
                ser_tx.write(test_data)
                bytes_sent += len(test_data)
                
                # Print progress every second
                elapsed = time.time() - start_time
                if int(elapsed) > int(elapsed - 0.1):
                    throughput_tx = (bytes_sent / elapsed) / 1024
                    throughput_rx = (self.bytes_received / elapsed) / 1024
                    print(f"Time: {elapsed:.1f}s | TX: {throughput_tx:.2f} KB/s | RX: {throughput_rx:.2f} KB/s", end='\r')
            
            # Stop receiver thread
            self.running = False
            rx_thread.join()
            
            # Calculate final results
            elapsed_time = time.time() - start_time
            
            print("\n\n" + "="*60)
            print("THROUGHPUT TEST RESULTS")
            print("="*60)
            print(f"Test Duration:     {elapsed_time:.2f} seconds")
            print(f"Bytes Sent:        {bytes_sent:,} bytes ({bytes_sent/1024/1024:.2f} MB)")
            print(f"Bytes Received:    {self.bytes_received:,} bytes ({self.bytes_received/1024/1024:.2f} MB)")
            print(f"Data Loss:         {bytes_sent - self.bytes_received:,} bytes ({((bytes_sent - self.bytes_received)/bytes_sent*100):.2f}%)")
            print(f"\nTX Throughput:     {(bytes_sent/elapsed_time)/1024:.2f} KB/s ({(bytes_sent/elapsed_time)*8/1000000:.2f} Mbps)")
            print(f"RX Throughput:     {(self.bytes_received/elapsed_time)/1024:.2f} KB/s ({(self.bytes_received/elapsed_time)*8/1000000:.2f} Mbps)")
            print("="*60)
            
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

def main():
    if len(sys.argv) < 3:
        print("Usage: python serial_throughput.py <TX_PORT> <RX_PORT> [BAUDRATE] [DURATION]")
        print("\nExample:")
        print("  python serial_throughput.py /dev/ttyUSB0 /dev/ttyUSB1 115200 10")
        print("  python serial_throughput.py COM3 COM4 9600 5")
        sys.exit(1)
    
    port_tx = sys.argv[1]
    port_rx = sys.argv[2]
    baudrate = int(sys.argv[3]) if len(sys.argv) > 3 else 115200
    duration = int(sys.argv[4]) if len(sys.argv) > 4 else 10
    
    tester = SerialThroughputTester(port_tx, port_rx, baudrate, duration)
    tester.run_test()

if __name__ == "__main__":
    main()