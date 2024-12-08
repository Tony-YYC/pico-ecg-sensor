import serial
import datetime
import time

def capture_serial_data():
    # Configure the serial connection
    ser = serial.Serial(
        port='/dev/ttyACM0',
        baudrate=115200,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=1
    )

    # Create a filename with timestamp
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = f"ecg_capture_{timestamp}.csv"
    
    print(f"Starting capture to {filename}")
    print("Press Ctrl+C to stop capturing")

    try:
        with open(filename, 'w') as f:
            while True:
                if ser.in_waiting:
                    line = ser.readline().decode('utf-8').strip()
                    print(line)  # Show on screen
                    f.write(line + '\n')  # Write to file
                    f.flush()  # Ensure data is written immediately
                
    except KeyboardInterrupt:
        print("\nCapture stopped by user")
    finally:
        ser.close()
        print(f"Data saved to {filename}")

if __name__ == "__main__":
    capture_serial_data()
