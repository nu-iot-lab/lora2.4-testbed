import os
import serial
import sys

if len(sys.argv) != 3:
    print("Usage: python3 serial_to_file.py <serial_port> <output_file>")
    sys.exit(1)

# Get the serial port and output file from the command-line arguments
serial_port = sys.argv[1]
output_file = sys.argv[2]

ser = None
try:
    ser = serial.Serial(serial_port, 115200)
except Exception as e:
    print("Serial Error:", e)
    sys.exit(1)

# Open the file in append mode if it exists, otherwise in write mode
mode = 'a' if os.path.exists(output_file) else 'w'
try:
    f = open(output_file, mode)
except Exception as e:
    print("File Error:", e)
    sys.exit(1)

def close_file():
    global f
    print('Closing file')
    try:
        f.close()
    except Exception as e:
        print('Could not close file', e)

timeout_count = 0  # Counter for "RXtimeout" occurrences
timeout_limit = 2000  # Limit for consecutive "RXtimeout"

while True:
    try:
        state = ser.readline().decode('ascii').strip()
        print(state)

        if "RXTimeout" in state:
            timeout_count += 1
            if timeout_count >= timeout_limit:
                print("Error: 'RXtimeout' has occurred 10 times in a row.")
                break
        else:
            timeout_count = 0

        # Write to the file
        f.write(state + '\n')
    except Exception as e:
        print("Error during reading process:", e)
        break

try:
    ser.flush()
    f.flush()
finally:
    close_file()

quit()
