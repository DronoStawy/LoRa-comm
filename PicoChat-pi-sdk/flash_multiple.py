#!/usr/bin/env python3
"""
Skrypt do automatycznego wgrywania firmware na wiele Raspberry Pi Pico jednocześnie.
Używa picotool do wykrywania urządzeń i wgrywania firmware.
"""

import subprocess
import re
import sys
import os
from pathlib import Path

def get_picotool_path():
    """Pobiera ścieżkę do picotool z zmiennej środowiskowej."""
    home = os.path.expanduser("~")
    picotool_path = f"{home}/.pico-sdk/picotool/2.2.0-a4/picotool/picotool"
    if not os.path.exists(picotool_path):
        print(f"Nie znaleziono picotool w {picotool_path}")
        print("Sprawdzam czy picotool jest w PATH...")
        try:
            subprocess.run(["picotool", "--version"], check=True, capture_output=True)
            return "picotool"
        except:
            print("Nie znaleziono picotool. Upewnij się, że jest zainstalowany.")
            sys.exit(1)
    return picotool_path

def list_pico_devices(picotool):
    """Wykrywa wszystkie podłączone urządzenia Raspberry Pi Pico."""
    try:
        # Sprawdź urządzenia (także te z USB serial)
        result = subprocess.run(
            [picotool, "info", "-a"],
            capture_output=True,
            text=True
        )
        
        devices = []
        # Parsuj wyjście aby znaleźć urządzenia
        output = result.stdout + result.stderr
        
        # Szukaj wzorców: "device at bus X, address Y"
        for line in output.splitlines():
            # Przykład: "RP2040 device at bus 2, address 4 appears to have a USB serial connection"
            match = re.search(r'bus (\d+), address (\d+)', line, re.IGNORECASE)
            if match:
                bus = match.group(1)
                address = match.group(2)
                devices.append({'bus': bus, 'address': address, 'line': line})
                print(f"  Znaleziono: Bus {bus}, Address {address}")
        
        return devices
        
    except subprocess.CalledProcessError as e:
        print(f"Błąd podczas wykrywania urządzeń: {e}")
        return []
    except FileNotFoundError:
        print(f"Nie można uruchomić picotool: {picotool}")
        return []

def flash_firmware(picotool, firmware_path, device):
    """Wgrywa firmware na konkretne urządzenie."""
    try:
        bus = device['bus']
        address = device['address']
        
        print(f"\n→ Wgrywanie na Bus {bus}, Address {address}...")
        
        # Użyj -f (force) z konkretnym busem i adresem
        result = subprocess.run(
            [picotool, "load", "-f", 
             "--bus", bus, 
             "--address", address,
             firmware_path, "-x"],
            capture_output=True,
            text=True
        )
        
        if result.stdout:
            print(result.stdout)
        if result.stderr:
            print(result.stderr)
        
        if result.returncode == 0:
            print(f"  ✓ Urządzenie Bus {bus}, Address {address} - SUCCESS")
            return True
        else:
            print(f"  ✗ Urządzenie Bus {bus}, Address {address} - FAILED (kod: {result.returncode})")
            return False
            
    except Exception as e:
        print(f"  ✗ Błąd: {e}")
        return False

def flash_all_devices(picotool, firmware_path):
    """Wgrywa firmware na wszystkie wykryte urządzenia po kolei."""
    print("=" * 60)
    print("Skrypt do wgrywania firmware na wiele Raspberry Pi Pico")
    print("=" * 60)
    
    if not os.path.exists(firmware_path):
        print(f"✗ Nie znaleziono pliku firmware: {firmware_path}")
        sys.exit(1)
    
    print(f"\nPlik firmware: {firmware_path}")
    print("\nWykrywanie urządzeń Raspberry Pi Pico...")
    
    devices = list_pico_devices(picotool)
    
    if not devices:
        print("\n✗ Nie znaleziono żadnych urządzeń RP2040/RP2350!")
        print("\nUpewnij się, że:")
        print("  1. Urządzenia są podłączone do komputera przez USB")
        print("  2. Urządzenia mają uruchomiony firmware z obsługą USB")
        sys.exit(1)
    
    print(f"\n✓ Znaleziono {len(devices)} urządzeń(a)")
    print("\nRozpoczynam wgrywanie firmware na wszystkie urządzenia...")
    
    success_count = 0
    failed_count = 0
    
    for device in devices:
        if flash_firmware(picotool, firmware_path, device):
            success_count += 1
        else:
            failed_count += 1
    
    print("\n" + "=" * 60)
    print(f"Podsumowanie:")
    print(f"  ✓ Sukces: {success_count}/{len(devices)} urządzeń")
    print(f"  ✗ Błędy:  {failed_count}/{len(devices)} urządzeń")
    print("=" * 60)
    
    if failed_count > 0:
        sys.exit(1)

def main():
    # Pobierz ścieżkę do picotool
    picotool = get_picotool_path()
    print(f"Używam picotool: {picotool}")
    
    # Ścieżka do firmware (domyślnie PicoChat.uf2 w build/)
    build_dir = Path(__file__).parent / "build"
    firmware_path = build_dir / "PicoChat.uf2"
    
    # Możesz też podać ścieżkę jako argument
    if len(sys.argv) > 1:
        firmware_path = Path(sys.argv[1])
    
    flash_all_devices(picotool, str(firmware_path))

if __name__ == "__main__":
    main()
