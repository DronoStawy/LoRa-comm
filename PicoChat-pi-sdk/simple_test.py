#!/usr/bin/env python3
"""
Simple Serial Test - Check if devices are responding
"""

import serial
import time
import sys

def test_echo(port, baudrate=115200):
    """Test if device echoes data"""
    try:
        print(f"\n{'='*60}")
        print(f"Testing port: {port}")
        print(f"{'='*60}\n")
        
        ser = serial.Serial(port, baudrate, timeout=2)
        time.sleep(1)
        
        # Clear buffers
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        print("Sending test data...")
        test_data = b"Hello123"
        ser.write(test_data)
        print(f"Sent: {test_data}")
        
        # Wait for response
        time.sleep(1)
        
        if ser.in_waiting > 0:
            received = ser.read(ser.in_waiting)
            print(f"Received: {received}")
            print(f"Bytes received: {len(received)}")
        else:
            print("No response received!")
        
        ser.close()
        
    except Exception as e:
        print(f"Error: {e}")

def monitor_port(port, baudrate=115200, duration=5):
    """Monitor port for any incoming data"""
    try:
        print(f"\n{'='*60}")
        print(f"Monitoring port: {port} for {duration} seconds")
        print(f"{'='*60}\n")
        
        ser = serial.Serial(port, baudrate, timeout=0.1)
        time.sleep(0.5)
        
        print("Listening for data...")
        start = time.time()
        total_bytes = 0
        
        while time.time() - start < duration:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                total_bytes += len(data)
                print(f"Received {len(data)} bytes: {data[:50]}...")  # Show first 50 bytes
        
        print(f"\nTotal bytes received: {total_bytes}")
        ser.close()
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage:")
        print("  Test echo:    python3 simple_test.py <PORT>")
        print("  Monitor:      python3 simple_test.py <PORT> monitor")
        print("\nExample:")
        print("  python3 simple_test.py /dev/tty.usbmodem101")
        print("  python3 simple_test.py /dev/tty.usbmodem1101 monitor")
        sys.exit(1)
    
    port = sys.argv[1]
    
    if len(sys.argv) > 2 and sys.argv[2] == "monitor":
        monitor_port(port)
    else:
        test_echo(port)
