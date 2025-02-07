"""
comm.py

An example using the Python rdscom library.
This example mirrors the provided C++ example.
"""

import sys
import time
from rdscom import (
    Result,
    default_error_callback,
    DataFieldType,
    DataPrototype,
    DataBuffer,
    MessageType,
    Message,
    DummyChannel,
    CommunicationInterfaceOptions,
    CommunicationInterface,
)

# ---------------------------------------------------------------------------
#                           CONSTANTS
# ---------------------------------------------------------------------------
MESSAGE_TYPE_PERSON = 0
MESSAGE_TYPE_CAR = 1

NUM_RETRIES = 3
RETRY_DELAY = 2000  # milliseconds

# ---------------------------------------------------------------------------
#                           GLOBAL OBJECTS
# ---------------------------------------------------------------------------
# Create a dummy communication channel.
g_channel = DummyChannel()

# Define a time function that returns the current time in milliseconds.
def current_millis() -> int:
    return int(time.time() * 1000)

# Create options using the number of retries, the retry delay, and our time function.
g_options = CommunicationInterfaceOptions(NUM_RETRIES, RETRY_DELAY, current_millis)

# Create the communication interface.
g_com = CommunicationInterface(g_channel, g_options)

# ---------------------------------------------------------------------------
#                           CALLBACKS
# ---------------------------------------------------------------------------
def on_person_message(message: Message) -> None:
    print("Received person message")
    message.print_clean(sys.stdout)

    # Get the car prototype from the communication interface.
    proto_car_result = g_com.get_prototype(MESSAGE_TYPE_CAR)
    if proto_car_result.is_error():
        print("Error: missing prototype for car", file=sys.stderr)
        return

    # Create a response message (of type REQUEST) with the car prototype.
    response = Message.from_type_and_proto(MessageType.REQUEST, proto_car_result.value())

    # Set the fields: make, model, and year.
    res_make = response.set_field("make", 1)
    res_model = response.set_field("model", 2)
    res_year = response.set_field("year", 2020)
    if Result.check(default_error_callback(sys.stderr), res_make, res_model, res_year):
        print("Error setting fields", file=sys.stderr)
        return

    # Send the response message and require an acknowledgment.
    g_com.send_message(response, ack_required=True)


def on_car_message(message: Message) -> None:
    print("Received car message")
    message.print_clean(sys.stdout)

    # Get the person prototype.
    proto_person_result = g_com.get_prototype(MESSAGE_TYPE_PERSON)
    if proto_person_result.is_error():
        print("Error: missing prototype for person", file=sys.stderr)
        return

    # Create a response message of type RESPONSE.
    # Note: The C++ code passes a prototype, but in our Python version
    # Message.create_response expects a DataBuffer. We build a DataBuffer from the prototype.
    response = Message.create_response(message, DataBuffer(proto_person_result.value()))

    # Set the fields: id and age.
    res_id = response.set_field("id", 1)
    res_age = response.set_field("age", 30)
    if Result.check(default_error_callback(sys.stderr), res_id, res_age):
        print("Error setting fields", file=sys.stderr)
        return

    # Send the response (ack not required for a RESPONSE message).
    g_com.send_message(response)


# ---------------------------------------------------------------------------
#                           MAIN
# ---------------------------------------------------------------------------
def main() -> int:
    # Add prototypes to the communication interface.
    # Prototype for a "person" message.
    g_com.add_prototype(
        DataPrototype(MESSAGE_TYPE_PERSON)
        .add_field("id", DataFieldType.INT8)
        .add_field("age", DataFieldType.UINT8)
    )
    # Prototype for a "car" message.
    g_com.add_prototype(
        DataPrototype(MESSAGE_TYPE_CAR)
        .add_field("make", DataFieldType.BYTE)
        .add_field("model", DataFieldType.BYTE)
        .add_field("year", DataFieldType.UINT16)
    )

    # Register callbacks.
    # When a RESPONSE for a person message is received, call on_person_message.
    g_com.add_callback(MESSAGE_TYPE_PERSON, MessageType.RESPONSE, on_person_message)
    # When a REQUEST for a car message is received, call on_car_message.
    g_com.add_callback(MESSAGE_TYPE_CAR, MessageType.REQUEST, on_car_message)

    # Create an initial message (of type REQUEST) using the car prototype.
    car_proto_result = g_com.get_prototype(MESSAGE_TYPE_CAR)
    if car_proto_result.is_error():
        print("Error: missing prototype for car", file=sys.stderr)
        return 1
    msg = Message.from_type_and_proto(MessageType.REQUEST, car_proto_result.value())

    # Set the fields for the car message.
    res_make = msg.set_field("make", 1)
    res_model = msg.set_field("model", 2)
    res_year = msg.set_field("year", 2020)
    if Result.check(default_error_callback(sys.stderr), res_make, res_model, res_year):
        print("Error setting fields", file=sys.stderr)
        return 1

    # Send the initial message with ack required.
    g_com.send_message(msg, ack_required=True)

    # Main loop.
    while True:
        # Process any incoming messages and handle acks/retries.
        g_com.tick()

        # Sleep for a bit.
        time.sleep(1)

        # Check if no messages have been received for more than 2 seconds.
        if g_com.time_since_last_received() > 2000:
            print("No messages received in 2 seconds -- this shouldn't happen in this program", file=sys.stderr)
            return 1

    # (In a real program you might break out of the loop; here it runs indefinitely.)
    return 0


if __name__ == "__main__":
    sys.exit(main())
