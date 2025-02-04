#ifndef __RDSCOM_H__
#define __RDSCOM_H__

/**========================================================================
 *
 *                       ____ ___  ____ ____ ____ _  _
 *                       |__/ |  \ [__  |    |  | |\/|
 *                       |  \ |__/ ___] |___ |__| |  |
 *
 *                                  rdscom
 * A single header communication library for embedded systems in C, with
 * a focus on flexibility and simplicity, designed for Robot Design Studio
 * at Northwestern University.
 *
 * Also comes with a python version, allowing for easy communication between
 * embedded systems and computers.
 *
 * ?                                ABOUT
 * @author         :  Evan Bertis-Sample
 * @email          :  esample21@gmail.com or
 *                    evanbertis-sample2026@u.northwestern.edu
 * @repo           :  https://github.com/evan-bertis-sample/rdscom.git
 * @createdOn      :  1/30/2025
 * @description    :  Communication library for embedded systems in C
 *
 *========================================================================**/

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <string>
#include <tuple>
#include <utility>

#define RDSCOM_VERSION "0.1.0"
#define RDSCOM_DEBUG_ENABLED 1

namespace rdscom {

/**========================================================================
 *                           UTILITIES
 *========================================================================**/

template <typename T>
class Result {
   public:
    static Result<T> ok(T value) { return Result<T>(value); }
    static Result<T> errorResult() { return Result<T>(true); }
    static Result<T> errorResult(std::string errorMessage) { return Result<T>(true, errorMessage); }

    operator bool() const { return !_error; }

    Result() : _error(true) {}

    Result<T> operator=(const Result<T> &other) {
        _value = other._value;
        _error = other._error;
        _errorMessage = other._errorMessage;
        return *this;
    }

    T value() const { return _value; }
    bool isError() const { return _error; }
    std::string error() const { return _errorMessage; }

   private:
    T _value;
    bool _error;
    std::string _errorMessage;

    Result(T value) : _value(value), _error(false), _errorMessage("") {}
    Result(bool error) : _error(error), _errorMessage(""), _value(T()) {}
    Result(bool error, std::string errorMessage) : _error(error), _errorMessage(errorMessage) {}
};

template <typename OnError, typename... Fs>
bool check(OnError onError, Fs &&...functions) {
    // check if any of the functions return an error
    bool error = false;
    std::vector<std::string> errors;
    std::initializer_list<int>{(error |= functions.isError(), 0)...};
    std::initializer_list<int>{(errors.push_back(functions.error()), 0)...};

    if (error) {
        // concatenate the error messages into one string
        std::string errorMessage;
        for (const std::string &error : errors) {
            if (error.empty()) {
                continue;
            }
            errorMessage += error;
            errorMessage += "\n";
        }

        onError(errorMessage.c_str());

        return true;
    }

    return false;
}

std::function<void(const char *)> defaultErrorCallback(std::ostream &stream) {
    return [&stream](const char *error) { stream << "Error: " << error << std::endl; };
}

#ifdef RDSCOM_DEBUG_ENABLED
#define RDSCOM_COLOR_PURPLE "\033[95m"
#define RDSCOM_COLOR_RETURN "\033[0m"
#define RDSCOM_LINE __LINE__
#define RDS_STRINGIFY(x) #x

#define RDSCOM_PREFIX RDSCOM_COLOR_PURPLE "[rdscom:" RDS_STRINGIFY(RDSCOM_LINE) "] " RDSCOM_COLOR_RETURN

#define RDSCOM_DEBUG_PRINT(fmt, ...) \
    fprintf(stdout, RDSCOM_PREFIX fmt, ##__VA_ARGS__)

#define RDSCOM_DEBUG_PRINTLN(fmt, ...) \
    fprintf(stdout, RDSCOM_PREFIX fmt "\n", ##__VA_ARGS__)

#define RDSCOM_DEBUG_PRINT_ERROR(fmt, ...) \
    fprintf(stderr, RDSCOM_PREFIX fmt, ##__VA_ARGS__)

#define RDSCOM_DEBUG_PRINT_ERRORLN(fmt, ...) \
    fprintf(stderr, RDSCOM_PREFIX fmt "\n", ##__VA_ARGS__)

#else
#define RDSCOM_DEBUG_PRINT(fmt, ...)
#define RDSCOM_DEBUG_PRINTLN(fmt, ...)
#define RDSCOM_DEBUG_PRINT_ERROR(fmt, ...)
#define RDSCOM_DEBUG_PRINT_ERRORLN(fmt, ...)
#endif

/**========================================================================
 *                           DATA FIELDS & PROTOTYPES
 *
 * Data Fields are the building blocks of the Data Prototype. You can think
 * of a Data Field as a single field in a struct. The Data Prototype is a
 * collection of Data Fields, and is the blueprint for the Data Buffer.
 * In this way, a Data Buffer is like an instance of a Data Prototype,
 * and actually contains data. Data Prototypes just describe the format of
 * the data.
 *
 * This abstraction allows for easy serialization and deserialization of
 * arbitrary data structures, and is the foundation of the Message class,
 * which uses a Data Buffer to store data, and send it over a communication
 * channel.
 *
 *========================================================================**/

/// @brief Enum for the different types of data fields
enum DataFieldType {
    UINT8,
    UINT16,
    UINT32,
    UINT64,
    INT8,
    INT16,
    INT32,
    INT64,
    FLOAT,
    DOUBLE,
    BOOL,
    BYTE,
    NONE
};

/// @brief DataField class for a single field in a Data Prototype
/// This is essentially a fat pointer.
/// It doesn't really make too much sense to use this class
/// directly, as it is mostly used internally.
struct DataField {
   public:
    std::size_t offset;
    DataFieldType type;

    DataField(std::size_t offset, DataFieldType type)
        : offset(offset), type(type) {}
    DataField() : offset(0), type(DataFieldType::NONE) {}
    DataField(const DataField &other)
        : offset(other.offset), type(other.type) {}

    std::size_t size() const {
        switch (type) {
            case DataFieldType::UINT8:
            case DataFieldType::INT8:
            case DataFieldType::BOOL:
            case DataFieldType::BYTE:
                return 1;
            case DataFieldType::UINT16:
            case DataFieldType::INT16:
                return 2;
            case DataFieldType::UINT32:
            case DataFieldType::INT32:
            case DataFieldType::FLOAT:
                return 4;
            case DataFieldType::UINT64:
            case DataFieldType::INT64:
            case DataFieldType::DOUBLE:
                return 8;
            default:
                return 0;
        }
    }

    DataField &operator=(const DataField &other) {
        offset = other.offset;
        type = other.type;
        return *this;
    }

    bool operator==(const DataField &other) const {
        return offset == other.offset && type == other.type;
    }

    bool operator!=(const DataField &other) const { return !(*this == other); }
};

/// @brief Reserved identifier prototype for when the identifier is not set
/// @details This just generally indicates if the prototype is not set
const std::uint8_t RESERVED_ERROR_PROTOTYPE = 80;

/// @brief DataPrototype class for a collection of DataFields
/// And describes the format of a DataBuffer
class DataPrototype {
   public:
    DataPrototype() : _identifier(RESERVED_ERROR_PROTOTYPE) {}
    DataPrototype(std::uint8_t identifier) : _identifier(identifier) {}
    DataPrototype(const DataPrototype &other)
        : _identifier(other._identifier),
          _size(other._size),
          _fields(other._fields) {}

    DataPrototype(std::vector<std::uint8_t> serialized) {
        _identifier = static_cast<std::uint8_t>(serialized[0]);
        std::size_t index = 1;
        std::size_t numFields = static_cast<std::size_t>(serialized[index]);
        index++;
        for (std::size_t i = 0; i < numFields; i++) {
            std::size_t nameSize = static_cast<std::size_t>(serialized[index]);
            index++;
            std::string name;
            for (std::size_t j = 0; j < nameSize; j++) {
                name.push_back(static_cast<char>(serialized[index]));
                index++;
            }
            DataFieldType type = static_cast<DataFieldType>(serialized[index]);
            index++;
            _fields[name] = DataField(_size, type);
            _size += _fields[name].size();
        }
    }

    DataPrototype &operator=(const DataPrototype &other) {
        _identifier = other._identifier;
        _size = other._size;
        _fields = other._fields;
        return *this;
    }

    DataPrototype addField(const std::string &name, DataFieldType type) {
        _fields[name] = DataField(_size, type);
        _size += _fields[name].size();
        return *this;
    }

    Result<DataField> findField(const std::string &name) const {
        auto it = _fields.find(name);
        if (it == _fields.end()) {
            std::string errorMessage = "Field not found: ";
            errorMessage += name;
            return Result<DataField>::errorResult(errorMessage);
        }
        return Result<DataField>::ok(it->second);
    }

    std::size_t size() const { return _size; }
    std::size_t numFields() const { return _fields.size(); }
    std::uint8_t identifier() const { return _identifier; }

    std::vector<std::uint8_t> serializeFormat() const {
        std::vector<std::uint8_t> serialized;
        serialized.push_back(static_cast<std::uint8_t>(_identifier));
        serialized.push_back(static_cast<std::uint8_t>(_fields.size()));
        for (const auto &field : _fields) {
            serialized.push_back(static_cast<std::uint8_t>(field.first.size()));
            serialized.insert(serialized.end(), field.first.begin(), field.first.end());
            serialized.push_back(static_cast<std::uint8_t>(field.second.type));
        }
        return serialized;
    }

   private:
    std::uint8_t _identifier = 0;
    std::size_t _size = 0;
    std::map<std::string, DataField> _fields;  // field name -> field
};

/// @brief DataBuffer is an instance of a DataPrototype
/// and uses the DataPrototype to describe the format of the data
/// it contains.
class DataBuffer {
   public:
    DataBuffer() : _type(DataPrototype()) {}

    DataBuffer(const DataPrototype &type) : _type(type) {
        _data.resize(_type.size());
    }

    DataBuffer(const DataBuffer &other) : _type(other._type), _data(other._data) {}

    static Result<DataBuffer> createFromPrototype(const DataPrototype &type, std::vector<std::uint8_t> data) {
        if (type.identifier() == RESERVED_ERROR_PROTOTYPE) {
            std::string errorMessage = "Invalid prototype: ";
            errorMessage += type.identifier();
            return Result<DataBuffer>::errorResult(errorMessage);
        }

        if (data.size() != type.size()) {
            std::string errorMessage = "Data size mismatch, expected: ";
            errorMessage += type.size();
            errorMessage += ", got: ";
            errorMessage += data.size();
            return Result<DataBuffer>::errorResult(errorMessage);
        }

        DataBuffer buffer(type);
        buffer._data = data;
        return Result<DataBuffer>::ok(buffer);
    }

    DataBuffer &operator=(const DataBuffer &other) {
        _type = other._type;
        _data = other._data;
        return *this;
    }

    template <typename T>
    Result<T> getField(const std::string &name) const {
        static_assert(std::is_arithmetic<T>::value, "getField only supports arithmetic types and std::uint8_t");
        static_assert(sizeof(T) <= sizeof(int64_t), "getField only supports types up to 64 bits");

        Result<DataField> fieldRes = _type.findField(name);
        if (fieldRes.isError()) {
            return Result<T>::errorResult(fieldRes.error());
        }

        DataField field = fieldRes.value();

        if (sizeof(T) != field.size()) {
            std::string errorMessage = "Field size mismatch: ";
            errorMessage += name;
            return Result<T>::errorResult(errorMessage);
        }

        T value;
        std::memcpy(&value, _data.data() + field.offset, sizeof(T));
        return Result<T>::ok(value);
    }

    template <typename T>
    Result<T> setField(const std::string &name, T value) {
        static_assert(std::is_arithmetic<T>::value, "setField only supports arithmetic types and std::uint8_t");
        static_assert(sizeof(T) <= sizeof(int64_t), "setField only supports types up to 64 bits");

        Result<DataField> fieldRes = _type.findField(name);
        if (fieldRes.isError()) {
            return Result<T>::errorResult(fieldRes.error());
        }

        DataField field = fieldRes.value();

        if (sizeof(T) != field.size()) {
            std::string errorMessage = "Field size mismatch: ";
            errorMessage += name;
            return Result<T>::errorResult(errorMessage);
        }

        std::memcpy(_data.data() + field.offset, &value, sizeof(T));
        return Result<T>::ok(value);
    }

    std::vector<std::uint8_t> data() const { return _data; }
    std::size_t size() const { return _data.size(); }
    DataPrototype type() const { return _type; }

   private:
    DataPrototype _type;
    std::vector<std::uint8_t> _data;
};

/**========================================================================
 *                           MESSAGES
 *
 * The Message class is a wrapper around a DataBuffer, and adds a type
 * to the message, which can be REQUEST, RESPONSE, or ERROR. This allows
 * for easy communication between embedded systems, and is the foundation
 * of the communication library.
 *
 *========================================================================**/

enum MessageType {
    REQUEST,
    RESPONSE,
    ERROR,
};

typedef struct MessageHeader {
    MessageType type;
    std::uint8_t prototypeHandle;
    std::uint16_t messageNumber;

    MessageHeader() : type(MessageType::REQUEST), prototypeHandle(0), messageNumber(0) {}
    MessageHeader(MessageType type, std::uint8_t prototypeHandle, std::uint16_t messageNumber)
        : type(type), prototypeHandle(prototypeHandle), messageNumber(messageNumber) {}

    static Result<MessageHeader> fromSerialized(const std::vector<std::uint8_t> &serialized) {
        if (serialized.size() < 4) {
            std::string errorMessage = "Message too short: ";
            errorMessage += serialized.size();
            return Result<MessageHeader>::errorResult(errorMessage);
        }

        MessageType type = static_cast<MessageType>(serialized[0]);
        std::uint8_t prototypeHandle = serialized[1];
        std::uint16_t messageNumber = static_cast<std::uint16_t>(serialized[2] << 8 | serialized[3]);

        return Result<MessageHeader>::ok(MessageHeader(type, prototypeHandle, messageNumber));
    }

    void insertSerialized(std::vector<std::uint8_t> &serialized) const {
        serialized.push_back(static_cast<std::uint8_t>(type));
        serialized.push_back(prototypeHandle);
        serialized.push_back(static_cast<std::uint8_t>(messageNumber >> 8));
        serialized.push_back(static_cast<std::uint8_t>(messageNumber & 0xFF));
    }

} MessageHeader;

class Message {
   public:
    Message() : _header(MessageHeader()), _buffer(DataBuffer()) {}
    Message(const Message &other) : _header(other._header), _buffer(other._buffer) {}
    Message(const MessageHeader &header, const DataBuffer &data) : _header(header), _buffer(data) {}
    Message(MessageType type, const DataBuffer &data) : _header(MessageHeader(type, data.type().identifier(), _messageNumber++)), _buffer(data) {}
    Message(MessageType type, const DataPrototype &proto) : _header(MessageHeader(type, proto.identifier(), _messageNumber++)), _buffer(DataBuffer(proto)) {}

    static std::uint8_t getPrototypeHandleFromBuffer(const std::vector<std::uint8_t> &serialized) {
        if (serialized.size() <= Message::_preambleSize) {
            return RESERVED_ERROR_PROTOTYPE;
        }

        return serialized[Message::_preambleSize];
    }

    static Result<Message> fromSerialized(const DataPrototype &proto, const std::vector<std::uint8_t> &serialized) {
        if (proto.identifier() == RESERVED_ERROR_PROTOTYPE) {
            return Result<Message>::errorResult("Invalid prototype");
        }

        if (serialized.size() <= Message::_preambleSize) {
            std::string errorMessage = "Message too short: ";
            errorMessage += serialized.size();
            return Result<Message>::errorResult(errorMessage);
        }

        // check if the preamble is correct
        for (std::size_t i = 0; i < Message::_preambleSize; i++) {
            if (serialized[i] != Message::_preamble[i]) {
                return Result<Message>::errorResult("Invalid preamble");
            }
        }

        Result<MessageHeader> headerRes = MessageHeader::fromSerialized(std::vector<std::uint8_t>(serialized.begin() + Message::_preambleSize, serialized.begin() + Message::_preambleSize + 4));
        if (headerRes.isError()) {
            return Result<Message>::errorResult("Failed to create message header");
        }

        MessageHeader header = headerRes.value();

        Result<DataBuffer> bufferRes = DataBuffer::createFromPrototype(proto, std::vector<std::uint8_t>(
                                                                                  serialized.begin() + 1,
                                                                                  serialized.end() - Message::_endSequenceSize));

        if (bufferRes.isError()) {
            return Result<Message>::errorResult("Failed to create data buffer");
        }

        // check if the end sequence is correct
        for (std::size_t i = 0; i < Message::_endSequenceSize; i++) {
            if (serialized[serialized.size() - Message::_endSequenceSize + i] != Message::_endSequence[i]) {
                return Result<Message>::errorResult("Invalid end sequence");
            }
        }

        return Result<Message>::ok(Message(header, bufferRes.value()));
    }

    MessageType type() const { return _header.type; }
    DataBuffer &data() { return _buffer; }

    template <typename T>
    Result<T> getField(const std::string &name) const {
        return _buffer.getField<T>(name);
    }

    template <typename T>
    Result<T> setField(const std::string &name, T value) {
        return _buffer.setField(name, value);
    }

    std::vector<std::uint8_t> serialize() const {
        std::vector<std::uint8_t> serialized;
        serialized.insert(serialized.end(), Message::_preamble, Message::_preamble + Message::_preambleSize);
        _header.insertSerialized(serialized);
        std::vector<std::uint8_t> data = _buffer.data();
        serialized.insert(serialized.end(), data.begin(), data.end());
        serialized.insert(serialized.end(), Message::_endSequence, Message::_endSequence + Message::_endSequenceSize);
        return serialized;
    }

    void printClean(std::ostream &output) const {
        std::vector<std::uint8_t> serialized = serialize();
        output << "Message: \n";
        output << "  Preamble: ";
        for (std::size_t i = 0; i < _preambleSize; i++) {
            output << static_cast<char>(serialized[i]);
        }
        output << "\n";
        output << "  Header: ";
        output << "    ";
        for (std::size_t i = 0; i < _completeHeaderSize; i++) {
            output << static_cast<char>(serialized[i + _preambleSize]);
        }
        output << "\n";

        output << "  Data: ";
        for (std::size_t i = 0; i < _buffer.size(); i++) {
            output << static_cast<char>(serialized[i + _completeHeaderSize]);
        }
        output << "\n";

        output << "  End Sequence: ";
        for (std::size_t i = 0; i < _endSequenceSize; i++) {
            output << static_cast<char>(serialized[i + _completeHeaderSize + _buffer.size()]);
        }
        output << "\n";
    }

   private:
    MessageHeader _header;
    DataBuffer _buffer;

    static std::uint8_t _preamble[3];
    static std::size_t _preambleSize;
    static std::size_t _completeHeaderSize;
    static std::uint16_t _messageNumber;
    static std::uint8_t _endSequence[3];
    static std::size_t _endSequenceSize;
    static std::size_t _completeEndSequenceSize;
};

std::uint8_t Message::_preamble[3] = {(std::uint8_t)'R', (std::uint8_t)'D', (std::uint8_t)'S'};
std::size_t Message::_preambleSize = 3;
std::size_t Message::_completeHeaderSize = sizeof(Message::_preamble) + sizeof(MessageHeader);
std::uint16_t Message::_messageNumber = 0;
std::uint8_t Message::_endSequence[3] = {(std::uint8_t)'E', (std::uint8_t)'N', (std::uint8_t)'D'};
std::size_t Message::_endSequenceSize = 3;
std::size_t Message::_completeEndSequenceSize = sizeof(Message::_endSequence);

/**========================================================================
 *                       COMMUNICATION CHANNELS
 *
 * Communication channels are the way that messages are sent and received.BOOL
 * This is just an abstract class, and is meant to be implemented by the
 * user. The user can implement a communication channel for UART, SPI, I2C,
 * or any other communication protocol.
 *
 *========================================================================**/

class CommunicationChannel {
   public:
    CommunicationChannel() {}
    virtual std::vector<std::uint8_t> receive() = 0;
    virtual void send(const Message &message) = 0;
};

#ifdef RDSCOM_ARDUINO  // Arduino specific code
#include <Arduino.h>

class SerialCommunicationChannel : public CommunicationChannel {
   public:
    SerialCommunicationChannel(HardwareSerial &serial) : _serial(serial) {}

    std::vector<std::uint8_t> receive() override {
        std::vector<std::uint8_t> data;
        while (_serial.available()) {
            data.push_back(static_cast<std::uint8_t>(_serial.read()));
        }
        return data;
    }

    void send(const Message &message) override {
        std::vector<std::uint8_t> serialized = message.serialize();
        for (std::uint8_t byte : serialized) {
            _serial.write(byte);
        }
    }

   private:
    HardwareSerial &_serial;
};

#endif

/**========================================================================
 *                           COMMUNICATION
 *
 * This is the main class that the user will interact with. It utilizes a
 * communication channel, but performs additional logic on top of it to
 * guarantee that messages are sent and received correctly. This is
 * an implementation of TCP-like communication, but is much simpler.
 *
 * It also adds the ability to add callbacks to messages, which can be
 * used to handle messages of a certain type.
 *
 *========================================================================**/

typedef std::vector<std::function<void(const Message &message)>> CallBackList;
typedef std::map<std::uint8_t, CallBackList> CallBackMap;

class CommunicationInterface {
   public:
    CommunicationInterface(CommunicationChannel &channel) : _channel(channel) {}

    CommunicationInterface &addRxCallback(std::uint8_t type, std::function<void(const Message &message)> callback) {
        if (_rxCallbacks.find(type) == _rxCallbacks.end()) {
            _rxCallbacks[type] = CallBackList();
        }

        _rxCallbacks[type].push_back(callback);
        return *this;
    }

    CommunicationInterface &addTxCallback(std::uint8_t type, std::function<void(const Message &message)> callback) {
        if (_txCallbacks.find(type) == _txCallbacks.end()) {
            _txCallbacks[type] = CallBackList();
        }

        _txCallbacks[type].push_back(callback);
        return *this;
    }

    CommunicationInterface &addPrototype(const DataPrototype &proto) {
        std::uint8_t handle = proto.identifier();
        if (handle == RESERVED_ERROR_PROTOTYPE) {
            RDSCOM_DEBUG_PRINT_ERRORLN("Invalid prototype");
            return *this;
        }
        _prototypes[handle] = proto;
        return *this;
    }

    void listen() {
        std::vector<std::uint8_t> data = _channel.receive();
        if (data.size() == 0) {
            return;
        }

        RDSCOM_DEBUG_PRINTLN("Received message");
        std::uint8_t protoHandle = Message::getPrototypeHandleFromBuffer(data);

        if (_prototypes.find(protoHandle) == _prototypes.end()) {
            RDSCOM_DEBUG_PRINT_ERRORLN("No prototype found for message");
            return;
        }

        DataPrototype proto = _prototypes[protoHandle];

        Message message = Message::fromSerialized(proto, data).value();

        if (_rxCallbacks.find(message.type()) != _rxCallbacks.end()) {
            for (const auto &callback : _rxCallbacks[message.type()]) {
                callback(message);
            }
        }
    }

   private:
    CommunicationChannel &_channel;
    CallBackMap _rxCallbacks;
    CallBackMap _txCallbacks;

    std::map<std::uint8_t, DataPrototype> _prototypes;
};

}  // namespace rdscom

#endif  // __RDSCOM_H__