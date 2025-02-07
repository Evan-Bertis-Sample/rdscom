
# rdscom

A simple and flexible communication library for C++, originally designed for use in Robot Design Studio (RDS) @ Northwestern University.

rdscom is a single-header communication library tailored for embedded systems. It focuses on flexibility and simplicity, providing robust support for message serialization/deserialization, reliable communication (with acknowledgments and retries), and customizable data structures. Additionally, a complete Python version is available, enabling seamless communication between embedded systems and computer-based applications.

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Concepts and Classes Breakdown](#concepts-and-classes-breakdown)
- [Getting Started](#getting-started)
  - [Requirements](#requirements)
  - [Installation](#installation)
- [Usage](#usage)
  - [C++ Example](#cpp-example)
  - [Python Example](#python-example)
- [Contributing](#contributing)
- [License](#license)
- [Contact](#contact)

---

## Overview

rdscom is divided into several modules, each addressing a specific aspect of communication in embedded systems. The library is designed to let you define custom data structures, serialize/deserialize messages, and manage reliable communication across different channels. Whether you are developing in C++ or Python, the same core concepts apply.

---

## Features

- **Header-Only Library (C++)**: Simply include `rdscom.hpp` in your project.
- **Customizable Data Structures**: Define your own data prototypes and fields for seamless serialization.
- **Robust Message Handling**: Supports multiple message types with built-in error handling.
- **Abstract Communication Channels**: Easily swap out communication implementations (e.g., DummyChannel, Serial, etc.).
- **Reliable Messaging**: Built-in support for message acknowledgment and retries.
- **Python Implementation**: Use the Python version for rapid prototyping or desktop-to-embedded communication.

---

## Concepts and Classes Breakdown

The rdscom library is organized into several key areas. Below is a detailed breakdown of the concepts and the classes that make up the library:

### 1. Utilities

- **`Result<T>`**  
  A template (or generic) class used to encapsulate the result of an operation. It holds either a value (indicating success) or an error flag with an error message. This class allows you to handle errors in a controlled manner without using exceptions.

- **`check` Function**  
  A helper function that takes one or more `Result` objects and an error callback. It aggregates any error messages from the provided results and invokes the callback if any errors occurred.

- **`default_error_callback`**  
  Returns a function that writes error messages to a specified output stream (e.g., `std::cerr` in C++ or `sys.stderr` in Python).

- **Debug Macros/Functions**  
  When debugging is enabled, debug printing functions output messages with colorized prefixes (in C++ via macros and in Python via functions like `debug_print`).

### 2. Data Fields & Prototypes

- **`DataFieldType`**  
  An enumeration defining the different types of data fields (e.g., `UINT8`, `INT16`, `FLOAT`, `DOUBLE`, etc.). These types determine the size and interpretation of each field.

- **`DataField`**  
  Represents a single data field within a data structure. It stores the offset (position) and the type of the field. It also provides a method to calculate the size (in bytes) of the field based on its type.

- **`DataPrototype`**  
  Defines the structure (or blueprint) for a data buffer. It is a collection of `DataField` instances and holds metadata such as an identifier and total size. Methods include:
  - Adding a new field (with automatic offset calculation).
  - Serializing the prototype format.
  - Searching for a specific field.

- **`DataBuffer`**  
  An instance of a `DataPrototype` that holds actual data. It provides methods to set and retrieve field values by name, ensuring proper serialization and deserialization of data.

### 3. Messages

- **`MessageType`**  
  An enumeration of message types, including `REQUEST`, `RESPONSE`, and `ERROR`. This distinguishes between the different kinds of messages that can be exchanged.

- **`MessageHeader`**  
  Encapsulates the meta-information for a message, such as the message type, prototype handle, and a unique message number. It handles both serialization and deserialization of header information.

- **`Message`**  
  The primary class for encapsulating a communication message. It contains:
  - A `MessageHeader` (with information like type and message number).
  - A `DataBuffer` holding the actual message payload.
  
  It provides methods to:
  - Serialize and deserialize entire messages.
  - Set and get individual fields from the data buffer.
  - Create response messages from requests.
  - Print a clean, human-readable version of the message.

### 4. Communication Channels

- **`CommunicationChannel` (Abstract Class)**  
  Defines an interface for communication channels. It requires two methods:
  - `receive`: To get incoming data.
  - `send`: To transmit a message.

- **`DummyChannel`**  
  A concrete implementation of `CommunicationChannel` used primarily for testing. It simulates sending and receiving messages by storing data in an internal buffer.

- *(Arduino-Specific)* **`SerialCommunicationChannel`**  
  When using Arduino, this implementation uses `HardwareSerial` to read and write data.

### 5. Communication Interface

- **`CommunicationInterfaceOptions`**  
  Encapsulates configuration options for the communication interface. These options include:
  - Maximum number of retries for sending a message.
  - Timeout duration for each retry.
  - A time function (to track when messages are sent and received).

- **`CommunicationInterface`**  
  Provides the high-level functionality to manage communication. It:
  - Uses a `CommunicationChannel` to send and receive messages.
  - Maintains a registry of data prototypes.
  - Allows you to add callbacks to handle specific message types.
  - Manages acknowledgments and retries through periodic calls to its `tick()` method.
  - Exposes helper methods like `listen()` and `send_message()`.

---

## Getting Started

### Requirements

- **For C++**: A C++11 (or later) compiler.
- **For Python**: Python 3.x.

### Installation

#### C++ Version

1. **Clone the Repository**:
   ```sh
   git clone https://github.com/evan-bertis-sample/rdscom.git
   ```
2. **Include in Your Project**:
   - Simply include the header file `rdscom.hpp` in your source files.
   - Define `RDSCOM_ARDUINO` if using the Arduino-specific implementations.

#### Python Version

1. **Clone the Repository** (or download the Python module):
   ```sh
   git clone https://github.com/evan-bertis-sample/rdscom.git
   ```
2. **Import the Module**:
   - Use the provided `rdscom.py` in your Python projects:
     ```python
     from rdscom import CommunicationInterface, DummyChannel, DataPrototype, DataFieldType, Message, MessageType
     ```

---

## Usage

### C++ Example

Below is a brief example of how to set up prototypes, register callbacks, and send/receive messages in C++:

```cpp
#include "rdscom.hpp"
#include <iostream>
#include <chrono>

using namespace rdscom;

#define MESSAGE_TYPE_PERSON 0
#define MESSAGE_TYPE_CAR 1

#define NUM_RETRIES 3
#define RETRY_DELAY 2000

DummyChannel g_channel;
CommunicationInterfaceOptions options {
    NUM_RETRIES, RETRY_DELAY,
    []() -> std::uint32_t {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
    }
};

CommunicationInterface g_com(g_channel, options);

void onPersonMessage(const Message &message) {
    std::cout << "Received person message\n";
    message.printClean(std::cout);

    Message response(MessageType::REQUEST, g_com.getPrototype(MESSAGE_TYPE_CAR).value());
    bool check = rdscom::check(
        rdscom::defaultErrorCallback(std::cerr),
        response.setField<std::uint8_t>("make", 1),
        response.setField<std::uint8_t>("model", 2),
        response.setField<std::uint16_t>("year", 2020)
    );

    if (check) {
        std::cerr << "Error setting fields\n";
        return;
    }
    
    // Send a response, expecting an acknowledgment.
    g_com.sendMessage(response, true);
}

void onCarMessage(const Message &message) {
    std::cout << "Received car message\n";
    message.printClean(std::cout);

    // Create a response message of type PERSON.
    Message response = Message::createResponse(message, g_com.getPrototype(MESSAGE_TYPE_PERSON).value());
    bool check = rdscom::check(
        rdscom::defaultErrorCallback(std::cerr),
        response.setField<std::int8_t>("id", 1),
        response.setField<std::uint8_t>("age", 30)
    );

    if (check) {
        std::cerr << "Error setting fields\n";
        return;
    }

    g_com.sendMessage(response);
}

int main() {
    g_com.addPrototype(
           DataPrototype(MESSAGE_TYPE_PERSON)
               .addField("id", DataFieldType::INT8)
               .addField("age", DataFieldType::UINT8))
        .addPrototype(
            DataPrototype(MESSAGE_TYPE_CAR)
                .addField("make", DataFieldType::BYTE)
                .addField("model", DataFieldType::BYTE)
                .addField("year", DataFieldType::UINT16));

    g_com.addCallback(
        MESSAGE_TYPE_PERSON,
        MessageType::RESPONSE,
        onPersonMessage
    );

    g_com.addCallback(
        MESSAGE_TYPE_CAR,
        MessageType::REQUEST,
        onCarMessage
    );

    Message msg(MessageType::REQUEST, g_com.getPrototype(MESSAGE_TYPE_CAR).value());
    bool error = rdscom::check(
        rdscom::defaultErrorCallback(std::cerr),
        msg.setField<std::uint8_t>("make", 1),
        msg.setField<std::uint8_t>("model", 2),
        msg.setField<std::uint16_t>("year", 2020)
    );

    if (error) {
        std::cerr << "Error setting fields\n";
        return 1;
    }

    g_com.sendMessage(msg, true);

    while (true) {
        g_com.tick();
        // Wait for 1 second.
        auto start = std::chrono::high_resolution_clock::now();
        while (std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::high_resolution_clock::now() - start).count() < 1) { }
        if (g_com.timeSinceLastRecieved() > 2000) {
            std::cerr << "No messages received in 2 seconds -- this shouldn't happen in this program\n";
            return 1;
        }
    }
}
```

### Python Example

This Python example mirrors the C++ usage. Save it (e.g., as `example.py`) and run it with Python 3.

```python
#!/usr/bin/env python3
import sys
import time
from rdscom import (
    CommunicationInterface, DummyChannel, DataPrototype,
    DataFieldType, Message, MessageType, CommunicationInterfaceOptions,
    check, default_error_callback
)

# Constants
MESSAGE_TYPE_PERSON = 0
MESSAGE_TYPE_CAR = 1
NUM_RETRIES = 3
RETRY_DELAY = 2000  # in milliseconds

# Create a dummy communication channel.
g_channel = DummyChannel()

# Define a time function that returns the current time in milliseconds.
def current_millis() -> int:
    return int(time.time() * 1000)

# Create communication options.
options = CommunicationInterfaceOptions(NUM_RETRIES, RETRY_DELAY, current_millis)

# Instantiate the communication interface.
g_com = CommunicationInterface(g_channel, options)

# Callback for person messages.
def on_person_message(message: Message) -> None:
    print("Received person message")
    message.print_clean(sys.stdout)

    proto_car = g_com.get_prototype(MESSAGE_TYPE_CAR)
    if proto_car.is_error():
        print("Error: missing car prototype", file=sys.stderr)
        return

    response = Message.from_type_and_proto(MessageType.REQUEST, proto_car.value())
    res_make = response.set_field("make", 1)
    res_model = response.set_field("model", 2)
    res_year = response.set_field("year", 2020)
    if Result.check(default_error_callback(sys.stderr), res_make, res_model, res_year):
        print("Error setting fields", file=sys.stderr)
        return

    # Send response with acknowledgment.
    g_com.send_message(response, ack_required=True)

# Callback for car messages.
def on_car_message(message: Message) -> None:
    print("Received car message")
    message.print_clean(sys.stdout)

    proto_person = g_com.get_prototype(MESSAGE_TYPE_PERSON)
    if proto_person.is_error():
        print("Error: missing person prototype", file=sys.stderr)
        return

    # Create a response message of type RESPONSE.
    response = Message.create_response(message, DataPrototype(proto_person.value().identifier())
                                        .add_field("id", DataFieldType.INT8)
                                        .add_field("age", DataFieldType.UINT8))
    res_id = response.set_field("id", 1)
    res_age = response.set_field("age", 30)
    if Result.check(default_error_callback(sys.stderr), res_id, res_age):
        print("Error setting fields", file=sys.stderr)
        return

    g_com.send_message(response)

def main() -> int:
    # Add prototypes.
    g_com.add_prototype(
        DataPrototype(MESSAGE_TYPE_PERSON)
        .add_field("id", DataFieldType.INT8)
        .add_field("age", DataFieldType.UINT8)
    )
    g_com.add_prototype(
        DataPrototype(MESSAGE_TYPE_CAR)
        .add_field("make", DataFieldType.BYTE)
        .add_field("model", DataFieldType.BYTE)
        .add_field("year", DataFieldType.UINT16)
    )

    # Register callbacks.
    g_com.add_callback(MESSAGE_TYPE_PERSON, MessageType.RESPONSE, on_person_message)
    g_com.add_callback(MESSAGE_TYPE_CAR, MessageType.REQUEST, on_car_message)

    # Create and send an initial car message.
    car_proto = g_com.get_prototype(MESSAGE_TYPE_CAR)
    if car_proto.is_error():
        print("Error: missing car prototype", file=sys.stderr)
        return 1

    msg = Message.from_type_and_proto(MessageType.REQUEST, car_proto.value())
    res_make = msg.set_field("make", 1)
    res_model = msg.set_field("model", 2)
    res_year = msg.set_field("year", 2020)
    if Result.check(default_error_callback(sys.stderr), res_make, res_model, res_year):
        print("Error setting fields", file=sys.stderr)
        return 1

    g_com.send_message(msg, ack_required=True)

    # Main loop.
    while True:
        g_com.tick()
        time.sleep(1)
        if g_com.time_since_last_received() > 2000:
            print("No messages received in 2 seconds -- this shouldn't happen in this program", file=sys.stderr)
            return 1

if __name__ == "__main__":
    sys.exit(main())
```




## License

This project is licensed under the [MIT License](LICENSE).


## Contact

- **Author**: Evan Bertis-Sample  
- **Email**: [esample21@gmail.com](mailto:esample21@gmail.com) or [evanbertis-sample2026@u.northwestern.edu](mailto:evanbertis-sample2026@u.northwestern.edu)  