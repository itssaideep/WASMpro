import subprocess
import sys
import time
import serial

esptool = r"C:\Users\Sai Deep\.platformio\packages\tool-esptoolpy\esptool.py"
bin_file = r".pio\build\nodemcuv2\firmware.bin"
port = "COM6"
baud = "115200"

print("=" * 60)
print("SECUREWASM EDGE — ESP8266 ONE-SHOT FLASHER")
print("=" * 60)
print(f"Target Port: {port}")
print(f"Firmware:    {bin_file}")
print("=" * 60)

cmd = [
    sys.executable,
    esptool,
    "--port", port,
    "--baud", baud,
    "write_flash",
    "0x0",
    bin_file
]

print("Starting flashing attempt...")
res = subprocess.run(cmd)

if res.returncode == 0:
    print("\n" + "=" * 60)
    print("SUCCESS: Firmware flashed to ESP8266!")
    print("Opening Serial Monitor at 115200 baud to read validation output...")
    print("=" * 60)
    time.sleep(1)
    try:
        s = serial.Serial(port, 115200, timeout=3)
        t_end = time.time() + 6
        while time.time() < t_end:
            line = s.readline()
            if line:
                print(line.decode("utf-8", errors="replace").strip())
        s.close()
    except Exception as e:
        print(f"Serial Monitor note: {e}")
else:
    print("\nUpload did not sync.")
    sys.exit(1)
