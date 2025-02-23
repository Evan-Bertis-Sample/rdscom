"""
                      ____ ___  ____ ____ ____ _  _
                      |__/ |  \ [__  |    |  | |\/|
                      |  \ |__/ ___] |___ |__| |  |

rdscom.py
A one-to-one Python implementation of the rdscom communication library.
Version: 0.1.0
Author: Evan Bertis-Sample
"""

import sys
import time
import struct
import enum
from typing import Any, Callable, Dict, List, Optional, TypeVar, Generic

# ---------------------------------------------------------------------------
#                           VERSION & DEBUG SETTINGS
# ---------------------------------------------------------------------------
RDSCOM_VERSION = "0.1.0"
RDSCOM_DEBUG_ENABLED = True

# ANSI color codes for debugging (if enabled)
RDSCOM_COLOR_PURPLE = "\033[95m"
RDSCOM_COLOR_RETURN = "\033[0m"


def debug_print(fmt: str, *args: Any) -> None:
    if RDSCOM_DEBUG_ENABLED:
        sys.stdout.write(
            RDSCOM_COLOR_PURPLE
            + ("[rdscom.py:%d] " % sys._getframe(1).f_lineno)
            + RDSCOM_COLOR_RETURN
            + (fmt % args)
        )
        sys.stdout.flush()


def debug_println(fmt: str, *args: Any) -> None:
    if RDSCOM_DEBUG_ENABLED:
        sys.stdout.write(
            RDSCOM_COLOR_PURPLE
            + ("[rdscom.py:%d] " % sys._getframe(1).f_lineno)
            + RDSCOM_COLOR_RETURN
            + (fmt % args)
            + "\n"
        )
        sys.stdout.flush()


def debug_print_error(fmt: str, *args: Any) -> None:
    if RDSCOM_DEBUG_ENABLED:
        sys.stderr.write(
            RDSCOM_COLOR_PURPLE
            + ("[rdscom.py:%d] " % sys._getframe(1).f_lineno)
            + RDSCOM_COLOR_RETURN
            + (fmt % args)
        )
        sys.stderr.flush()


def debug_print_errorln(fmt: str, *args: Any) -> None:
    if RDSCOM_DEBUG_ENABLED:
        sys.stderr.write(
            RDSCOM_COLOR_PURPLE
            + ("[rdscom.py:%d] " % sys._getframe(1).f_lineno)
            + RDSCOM_COLOR_RETURN
            + (fmt % args)
            + "\n"
        )
        sys.stderr.flush()


def default_error_callback(stream=sys.stdout) -> Callable[[str], None]:
    return lambda error: stream.write("Error: " + error + "\n")


# ---------------------------------------------------------------------------
#                           RESULT CLASS & UTILITY FUNCTION
# ---------------------------------------------------------------------------
T = TypeVar("T")


class Result(Generic[T]):
    def __init__(
        self, value: Optional[T] = None, error: bool = False, error_message: str = ""
    ):
        self._value = value
        self._error = error
        self._error_message = error_message

    @classmethod
    def ok(cls, value: T) -> "Result[T]":
        return cls(value=value, error=False, error_message="")

    @classmethod
    def errorResult(cls, error_message: str = "") -> "Result[T]":
        return cls(value=None, error=True, error_message=error_message)

    def __bool__(self) -> bool:
        # In C++ the operator bool returns true if no error.
        return not self._error

    def value(self) -> T:
        return self._value

    def is_error(self) -> bool:
        return self._error

    def error(self) -> str:
        return self._error_message

    def __repr__(self) -> str:
        if self._error:
            return f"Result(error: {self._error_message})"
        return f"Result(value: {self._value})"

    @staticmethod
    def check(on_error: Callable[[str], None], *functions: "Result[Any]") -> bool:
        error = False
        errors: List[str] = []
        for res in functions:
            if res.is_error():
                error = True
                errors.append(res.error())
        if error:
            error_message = "\n".join(e for e in errors if e)
            on_error(error_message)
        return error


# ---------------------------------------------------------------------------
#                           DATA FIELDS & PROTOTYPES
# ---------------------------------------------------------------------------


class DataFieldType(enum.Enum):
    UINT8 = 0
    UINT16 = 1
    UINT32 = 2
    UINT64 = 3
    INT8 = 4
    INT16 = 5
    INT32 = 6
    INT64 = 7
    FLOAT = 8
    DOUBLE = 9
    BOOL = 10
    BYTE = 11
    NONE = 12


class DataField:
    def __init__(self, offset: int = 0, type_: DataFieldType = DataFieldType.NONE):
        self.offset = offset
        self.type = type_

    @classmethod
    def get_size_of_type(cls, type_: DataFieldType) -> int:
        if type_ in (
            DataFieldType.UINT8,
            DataFieldType.INT8,
            DataFieldType.BOOL,
            DataFieldType.BYTE,
        ):
            return 1
        elif type_ in (DataFieldType.UINT16, DataFieldType.INT16):
            return 2
        elif type_ in (DataFieldType.UINT32, DataFieldType.INT32, DataFieldType.FLOAT):
            return 4
        elif type_ in (DataFieldType.UINT64, DataFieldType.INT64, DataFieldType.DOUBLE):
            return 8
        else:
            return 0

    def size(self) -> int:
        return DataField.get_size_of_type(self.type)

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, DataField):
            return NotImplemented
        return self.offset == other.offset and self.type == other.type

    def __ne__(self, other: object) -> bool:
        return not (self == other)


# Reserved identifier for error prototypes
RESERVED_ERROR_PROTOTYPE = 80


class DataPrototype:
    def __init__(self, identifier: int = RESERVED_ERROR_PROTOTYPE):
        self._identifier = identifier  # Should be 0-255
        self._size = 0
        self._fields: Dict[str, DataField] = {}

    @classmethod
    def from_serialized_format(cls, serialized: bytes) -> Result["DataPrototype"]:
        proto = cls()
        if len(serialized) < 2:
            return Result.errorResult("Serialized data too short")
        proto._identifier = serialized[0]
        num_fields = serialized[1]
        offset = 2
        for i in range(num_fields):
            if offset >= len(serialized):
                return Result.errorResult("Serialized data too short")
            name_size = serialized[offset]
            offset += 1
            if offset + name_size + 1 > len(serialized):
                return Result.errorResult("Serialized data too short")
            name_bytes = serialized[offset : offset + name_size]
            try:
                name = name_bytes.decode("utf-8")
            except UnicodeDecodeError:
                name = name_bytes.decode("latin1")
            offset += name_size
            type_val = serialized[offset]
            offset += 1
            field_type = DataFieldType(type_val)
            proto._fields[name] = DataField(proto._size, field_type)
            proto._size += proto._fields[name].size()
        return Result.ok(proto)

    def serialize_format(self) -> bytearray:
        serialized = bytearray()
        serialized.append(self._identifier)
        serialized.append(len(self._fields))
        for name, field in self._fields.items():
            name_bytes = name.encode("utf-8")
            serialized.append(len(name_bytes))
            serialized.extend(name_bytes)
            serialized.append(field.type.value)
        return serialized

    def add_field(self, name: str, field_type: DataFieldType) -> "DataPrototype":
        if name in self._fields:
            self._size -= self._fields[name].size()
        self._fields[name] = DataField(self._size, field_type)
        self._size += self._fields[name].size()
        return self

    def find_field(self, name: str) -> Result[DataField]:
        if name not in self._fields:
            return Result.errorResult("Field not found: " + name)
        return Result.ok(self._fields[name])

    def size(self) -> int:
        return self._size

    def num_fields(self) -> int:
        return len(self._fields)

    def identifier(self) -> int:
        return self._identifier
    
    def field_names(self) -> List[str]:
        return list(self._fields.keys())

    def __repr__(self) -> str:
        return f"DataPrototype(identifier={self._identifier}, size={self._size}, fields={self._fields})"


# Mapping from DataFieldType to struct format characters
_type_format: Dict[DataFieldType, str] = {
    DataFieldType.UINT8: "B",
    DataFieldType.INT8: "b",
    DataFieldType.BOOL: "?",
    DataFieldType.BYTE: "B",
    DataFieldType.UINT16: "H",
    DataFieldType.INT16: "h",
    DataFieldType.UINT32: "I",
    DataFieldType.INT32: "i",
    DataFieldType.FLOAT: "f",
    DataFieldType.UINT64: "Q",
    DataFieldType.INT64: "q",
    DataFieldType.DOUBLE: "d",
}


class DataBuffer:
    def __init__(self, proto: Optional[DataPrototype] = None):
        if proto is None:
            # Create an "empty" buffer.
            self._type = DataPrototype()
            self._data = bytearray()
        else:
            self._type = proto
            self._data = bytearray(proto.size())

    @classmethod
    def create_from_prototype(
        cls, proto: DataPrototype, data: bytes
    ) -> Result["DataBuffer"]:
        if proto.identifier() == RESERVED_ERROR_PROTOTYPE:
            return Result.errorResult("Invalid prototype: " + str(proto.identifier()))
        if len(data) != proto.size():
            return Result.errorResult(
                f"Data size mismatch, expected: {proto.size()}, got: {len(data)}"
            )
        buffer = cls(proto)
        buffer._data = bytearray(data)
        return Result.ok(buffer)

    def get_field(self, name: str) -> Result[Any]:
        # Look up the field in the prototype.
        field_res = self._type.find_field(name)
        if field_res.is_error():
            return Result.errorResult(field_res.error())
        field = field_res.value()
        expected_size = field.size()
        # Get the appropriate format string.
        try:
            fmt = _type_format[field.type]
        except KeyError:
            return Result.errorResult("Unsupported field type for field: " + name)
        if struct.calcsize(fmt) != expected_size:
            return Result.errorResult("Field size mismatch: " + name)
        # Unpack from the data buffer (using native endianness)
        try:
            # struct.unpack returns a tuple
            value = struct.unpack_from(fmt, self._data, field.offset)[0]
        except struct.error as e:
            return Result.errorResult("Struct unpack error: " + str(e))
        return Result.ok(value)

    def set_field(self, name: str, value: Any) -> Result[Any]:
        field_res = self._type.find_field(name)
        if field_res.is_error():
            return Result.errorResult(field_res.error())
        field = field_res.value()
        expected_size = field.size()
        try:
            fmt = _type_format[field.type]
        except KeyError:
            return Result.errorResult("Unsupported field type for field: " + name)
        if struct.calcsize(fmt) != expected_size:
            return Result.errorResult("Field size mismatch: " + name)
        try:
            packed = struct.pack(fmt, value)
        except struct.error as e:
            return Result.errorResult("Struct pack error: " + str(e))
        # Copy the packed bytes into the data buffer.
        self._data[field.offset : field.offset + expected_size] = packed
        return Result.ok(value)

    def data(self) -> bytearray:
        return self._data

    def size(self) -> int:
        return len(self._data)

    def type(self) -> DataPrototype:
        return self._type

    def __repr__(self) -> str:
        return f"DataBuffer(type={self._type}, data={list(self._data)})"


# ---------------------------------------------------------------------------
#                           MESSAGES
# ---------------------------------------------------------------------------


class MessageType(enum.IntEnum):
    REQUEST = 0
    RESPONSE = 1
    ERROR = 2

    @staticmethod
    def to_string(type_: "MessageType") -> str:
        if type_ == MessageType.REQUEST:
            return "Request"
        elif type_ == MessageType.RESPONSE:
            return "Response"
        elif type_ == MessageType.ERROR:
            return "Error"
        return "Unknown"
    
    @staticmethod
    def from_string(type_str: str) -> "MessageType":
        if type_str == "Request":
            return MessageType.REQUEST
        elif type_str == "Response":
            return MessageType.RESPONSE
        elif type_str == "Error":
            return MessageType.ERROR
        return MessageType.REQUEST


class MessageHeader:
    def __init__(
        self,
        type_: MessageType = MessageType.REQUEST,
        prototype_handle: int = 0,
        message_number: int = 0,
    ):
        self.type = type_
        self.prototype_handle = prototype_handle  # 0-255
        self.message_number = message_number  # 0-65535

    @classmethod
    def from_serialized(cls, serialized: bytes) -> Result["MessageHeader"]:
        if len(serialized) < 4:
            return Result.errorResult("Message too short: " + str(len(serialized)))
        type_ = MessageType(serialized[0])
        prototype_handle = serialized[1]
        # Combine two bytes into a 16-bit integer (big-endian as in the C++ code)
        message_number = (serialized[2] << 8) | serialized[3]
        return Result.ok(cls(type_, prototype_handle, message_number))

    def insert_serialized(self, serialized: bytearray) -> None:
        serialized.append(self.type.value)
        serialized.append(self.prototype_handle)
        serialized.append((self.message_number >> 8) & 0xFF)
        serialized.append(self.message_number & 0xFF)

    def __repr__(self) -> str:
        return f"MessageHeader(type={self.type}, prototype_handle={self.prototype_handle}, message_number={self.message_number})"


class Message:
    # Static members
    _preamble: bytes = b"RRR"  # 3 bytes
    _preamble_size: int = len(_preamble)
    _end_sequence: bytes = b"END"  # 3 bytes
    _end_sequence_size: int = len(_end_sequence)
    _complete_header_size: int = _preamble_size + 4  # 4 bytes for header
    _complete_end_sequence_size: int = _end_sequence_size
    _message_number: int = 0  # auto-incremented

    def __init__(self, header: MessageHeader, buffer: DataBuffer):
        self._header = header
        self._buffer = buffer
        # Serve warnings if needed.
        self._serve_message_constructor_warnings(
            self._buffer.type().identifier(), self._header.type
        )

    @classmethod
    def _next_message_number(cls) -> int:
        cls._message_number = (cls._message_number + 1) % 0x10000
        return cls._message_number

    @classmethod
    def from_type_and_data(
        cls,
        msg_type: MessageType,
        data: DataBuffer,
        message_number: Optional[int] = None,
    ) -> "Message":
        if message_number is None:
            message_number = cls._next_message_number()
        header = MessageHeader(msg_type, data.type().identifier(), message_number)
        return cls(header, data)

    @classmethod
    def from_type_and_proto(
        cls,
        msg_type: MessageType,
        proto: DataPrototype,
        message_number: Optional[int] = None,
    ) -> "Message":
        if message_number is None:
            message_number = cls._next_message_number()
        header = MessageHeader(msg_type, proto.identifier(), message_number)
        buffer = DataBuffer(proto)
        return cls(header, buffer)

    @classmethod
    def create_response(cls, request: "Message", data: DataBuffer) -> "Message":
        header = MessageHeader(
            MessageType.RESPONSE, data.type().identifier(), request.message_number()
        )
        return cls(header, data)

    @classmethod
    def get_prototype_handle_from_buffer(cls, serialized: bytes) -> int:
        if len(serialized) <= cls._complete_header_size:
            return RESERVED_ERROR_PROTOTYPE
        # The prototype handle is the second byte after the preamble.
        return serialized[cls._preamble_size + 1]

    @classmethod
    def from_serialized(
        cls, proto: DataPrototype, serialized: bytes
    ) -> Result["Message"]:
        if proto.identifier() == RESERVED_ERROR_PROTOTYPE:
            return Result.errorResult("Invalid prototype")
        if len(serialized) <= cls._preamble_size:
            return Result.errorResult("Message too short: " + str(len(serialized)))
        # Check preamble
        if serialized[0 : cls._preamble_size] != cls._preamble:
            return Result.errorResult("Invalid preamble")
        # Check end sequence
        if serialized[-cls._end_sequence_size :] != cls._end_sequence:
            return Result.errorResult("Invalid end sequence")
        # Extract header bytes
        header_bytes = serialized[cls._preamble_size : cls._preamble_size + 4]
        header_res = MessageHeader.from_serialized(header_bytes)
        if header_res.is_error():
            return Result.errorResult("Failed to create message header")
        header = header_res.value()
        expected_size = (
            cls._complete_header_size + proto.size() + cls._complete_end_sequence_size
        )
        if len(serialized) != expected_size:
            return Result.errorResult(
                f"Message size mismatch, expected: {expected_size}, got: {len(serialized)}"
            )
        data_bytes = serialized[
            cls._complete_header_size : -cls._complete_end_sequence_size
        ]
        buffer_res = DataBuffer.create_from_prototype(proto, data_bytes)
        if buffer_res.is_error():
            return Result.errorResult("Failed to create data buffer")
        return Result.ok(cls(header, buffer_res.value()))

    def serialize(self) -> bytearray:
        serialized = bytearray()
        serialized.extend(Message._preamble)
        self._header.insert_serialized(serialized)
        serialized.extend(self._buffer.data())
        serialized.extend(Message._end_sequence)
        return serialized

    def get_field(self, name: str) -> Result[Any]:
        return self._buffer.get_field(name)

    def set_field(self, name: str, value: Any) -> Result[Any]:
        return self._buffer.set_field(name, value)

    def type(self) -> MessageType:
        return self._header.type

    def data(self) -> DataBuffer:
        return self._buffer

    def message_number(self) -> int:
        return self._header.message_number

    def print_clean(self, output=sys.stdout) -> None:
        serialized = self.serialize()
        output.write("Message: \n")
        output.write(
            "  Preamble: "
            + serialized[0 : Message._preamble_size].decode("utf-8", errors="replace")
            + "\n"
        )
        header_bytes = serialized[
            Message._preamble_size : Message._complete_header_size
        ]
        header_hex = " ".join(f"{b:02x}" for b in header_bytes)
        output.write("  Header: " + header_hex + "\n")
        data_bytes = serialized[
            Message._complete_header_size : Message._complete_header_size
            + self._buffer.size()
        ]
        data_hex = " ".join(f"{b:02x}" for b in data_bytes)
        output.write("  Data: " + data_hex + "\n")
        end_seq = serialized[-Message._end_sequence_size :].decode(
            "utf-8", errors="replace"
        )
        output.write("  End Sequence: " + end_seq + "\n")

    def _serve_message_constructor_warnings(
        self, prototype_identifier: int, msg_type: MessageType
    ) -> None:
        if prototype_identifier == RESERVED_ERROR_PROTOTYPE:
            debug_print_errorln(
                "Invalid prototype: please make sure to pass in a number to the DataPrototype constructor and do not use the identifier %d",
                RESERVED_ERROR_PROTOTYPE,
            )
        if msg_type == MessageType.RESPONSE:
            debug_print_errorln(
                "Creating a response message directly is not recommended. Please use Message.create_response() instead."
            )

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, Message):
            return NotImplemented
        return (
            self._header.type == other._header.type
            and self._header.prototype_handle == other._header.prototype_handle
            and self._header.message_number == other._header.message_number
            and self._buffer.data() == other._buffer.data()
        )

    def __ne__(self, other: object) -> bool:
        return not (self == other)

    def __repr__(self) -> str:
        return f"Message(header={self._header}, buffer={self._buffer})"


# ---------------------------------------------------------------------------
#                    COMMUNICATION CHANNELS
# ---------------------------------------------------------------------------

from abc import ABC, abstractmethod


class CommunicationChannel(ABC):
    @abstractmethod
    def receive(self) -> bytearray:
        pass

    @abstractmethod
    def send(self, message: Message) -> None:
        pass


# DummyChannel implementation for testing
class DummyChannel(CommunicationChannel):
    def __init__(self):
        self._data = bytearray()

    def receive(self) -> bytearray:
        if not self._data:
            return bytearray()
        debug_println("[DummyChannel] Received data of size %d", len(self._data))
        data = self._data[:]
        self._data.clear()
        return data

    def send(self, message: Message) -> None:
        serialized = message.serialize()
        self._data.extend(serialized)


# (If you were on Arduino you would create a SerialCommunicationChannel here.)

# ---------------------------------------------------------------------------
#                         COMMUNICATION INTERFACE
# ---------------------------------------------------------------------------
# Type aliases for clarity.
CallBackList = List[Callable[[Message], None]]
CallBackMap = Dict[int, CallBackList]


class CommunicationInterfaceOptions:
    def __init__(
        self,
        max_retries: int = 3,
        retry_timeout: int = 1000,
        time_function: Optional[Callable[[], int]] = None,
    ):
        self.max_retries = max_retries
        self.retry_timeout = retry_timeout
        if time_function is None:
            # Default time function: milliseconds since epoch.
            self.time_function = lambda: int(time.time() * 1000)
            debug_print_errorln(
                "No time function set for CommunicationInterfaceOptions, using default time.time() based function"
            )
        else:
            self.time_function = time_function


class CommunicationInterface:
    class SentMessage:
        def __init__(self, message: Message, time_sent: int, num_retries: int):
            self.message = message
            self.time_sent = time_sent
            self.num_retries = num_retries

    def __init__(
        self,
        channel: CommunicationChannel,
        options: Optional[CommunicationInterfaceOptions] = None,
    ):
        self._channel = channel
        self._options = (
            options if options is not None else CommunicationInterfaceOptions()
        )
        self._rx_callbacks: CallBackMap = {}
        self._tx_callbacks: CallBackMap = {}
        self._err_callbacks: CallBackMap = {}
        self._prototypes: Dict[int, DataPrototype] = {}
        self._acks_needed: Dict[int, CommunicationInterface.SentMessage] = {}
        self._last_message_time: int = 0
        self._on_failure_callbacks : CallBackMap = {} # for when messages fail to send

    def add_callback(
        self, type_: int, msg_type: MessageType, callback: Callable[[Message], None]
    ) -> "CommunicationInterface":
        callback_map = self._get_map(msg_type)
        if type_ not in callback_map:
            callback_map[type_] = []
        callback_map[type_].append(callback)
        return self

    def add_prototype(self, proto: DataPrototype) -> "CommunicationInterface":
        handle = proto.identifier()
        if handle == RESERVED_ERROR_PROTOTYPE:
            debug_print_errorln(
                "Invalid prototype: please make sure to pass in a number to the DataPrototype constructor and do not use the identifier %d",
                RESERVED_ERROR_PROTOTYPE,
            )
            return self
        self._prototypes[handle] = proto
        return self

    def tick(self) -> None:
        self.listen()
        # now check for acks
        to_remove: List[int] = []
        current_time = self._options.time_function()
        for message_number, sent_msg in self._acks_needed.items():
            time_since_sent = current_time - sent_msg.time_sent
            if time_since_sent > self._options.retry_timeout:
                if sent_msg.num_retries < self._options.max_retries:
                    debug_println(
                        "Retrying message number %d after %d ms",
                        message_number,
                        time_since_sent,
                    )
                    self.send_message(sent_msg.message, ack_required=False)
                    sent_msg.time_sent = self._options.time_function()
                    sent_msg.num_retries += 1
                else:
                    to_remove.append(message_number)
        for message_number in to_remove:
            debug_println(
                "Removing message number %d -- failed to get an ack before the timeout",
                message_number,
            )

            # call the on_failure callback
            if message_number in self._on_failure_callbacks.keys():
                self._on_failure_callbacks[message_number]()

            del self._acks_needed[message_number]

    def listen(self) -> None:
        data = self._channel.receive()
        if not data:
            return
        proto_handle = Message.get_prototype_handle_from_buffer(data)
        if proto_handle not in self._prototypes:
            debug_print_errorln(
                "No prototype found for message with handle %d", proto_handle
            )
            return
        proto = self._prototypes[proto_handle]
        message_res = Message.from_serialized(proto, data)
        if message_res.is_error():
            debug_print_errorln(
                "Failed to deserialize message: %s", message_res.error()
            )
            return
        message = message_res.value()
        debug_println(
            "Received message of prototype %d, message number %d, message type %d",
            proto_handle,
            message.message_number(),
            message.type(),
        )
        self._last_message_time = self._options.time_function()
        if message.type() == MessageType.RESPONSE:
            if message.message_number() in self._acks_needed:
                del self._acks_needed[message.message_number()]
        callback_map = self._get_map(message.type())
        if proto_handle in callback_map:
            for callback in callback_map[proto_handle]:
                callback(message)

    def send_message(self, message: Message, ack_required: bool = False, on_failure : callable = None) -> None:
        self._channel.send(message)
        if ack_required and message.type() == MessageType.REQUEST:
            self._acks_needed[message.message_number()] = (
                CommunicationInterface.SentMessage(
                    message, self._options.time_function(), 0
                )
            )

            # call the tx callbacks, if this is the first time the message is being sent
            acks_needed = self._acks_needed[message.message_number()]
            if acks_needed.num_retries == 0:
                callback_map = self._get_map(message.type())
                if message.data().type().identifier() in callback_map:
                    print("Calling tx callbacks")
                    for callback in callback_map[message.data().type().identifier()]:
                        callback(message)

                # also add the on_failure callback
                self._on_failure_callbacks[message.message_number()] = on_failure
        elif ack_required and message.type() == MessageType.RESPONSE:
            debug_print_errorln(
                "You cannot require an ack for a response message, as a response is the ack"
            )

        # call the tx callbacks

    def get_prototype(self, identifier: int) -> Result[DataPrototype]:
        if identifier not in self._prototypes:
            return Result.errorResult("Prototype not found")
        return Result.ok(self._prototypes[identifier])

    def time_since_last_received(self) -> int:
        return self._options.time_function() - self._last_message_time

    def _get_map(self, msg_type: MessageType) -> CallBackMap:
        if msg_type == MessageType.REQUEST:
            return self._rx_callbacks
        elif msg_type == MessageType.RESPONSE:
            return self._tx_callbacks
        elif msg_type == MessageType.ERROR:
            return self._err_callbacks
        return self._rx_callbacks
