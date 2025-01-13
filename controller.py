import serial
import time

# USB CDC settings
SERIAL_PORT = '/dev/ttyACM0'
BAUD_RATE = 115200

# Function to send a new MUX configuration
def set_mux_configuration(config):
    """
    Sends a MUX configuration to the Pi Pico.

    :param config: List of tuples [(in_pin, out_pin), ...] representing the pin mappings
    """
    # Validate input
    if len(config) != 8:
        print("Error: Configuration must have exactly 8 pin mappings.")
        return

    for in_pin, out_pin in config:
        if not (0 <= in_pin <= 30) or not (0 <= out_pin <= 30):
            print(f"Error: Pin numbers must be between 0 and 30. Invalid pair: ({in_pin}, {out_pin})")
            return
        if in_pin == out_pin:
            print(f"Error: Input and output pins cannot be the same. Invalid pair: ({in_pin}, {out_pin})")
            return

    # Format the configuration string
    config_str = "[" + ",".join(f"({in_pin},{out_pin})" for in_pin, out_pin in config) + "]\n"

    # Open the serial connection and send the configuration
    try:
        with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
            ser.write(config_str.encode())
            print(f"Sent configuration: {config_str.strip()}")

            # Wait for the response
            time.sleep(0.1)
            response = ser.read_all().decode()
            print(f"Response: {response}")
    except serial.SerialException as e:
        print(f"Error: {e}")

# Example usage
if __name__ == "__main__":
    # Define the MUX configuration
    mux_config = [
        (6, 2),
        (3, 7),
        (4, 8),
        (5, 9),
        (10, 14),
        (11, 15),
        (12, 16),
        (17, 13)
    ]

    # Send the configuration
    set_mux_configuration(mux_config)
