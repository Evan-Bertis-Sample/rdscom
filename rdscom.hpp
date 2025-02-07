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
 * ?                               TOC
 * 1. UTILITIES
 * 2. DATA FIELDS & PROTOTYPES
 * 3. MESSAGES
 * 4. COMMUNICATION CHANNELS
 * 5. COMMUNICATION
 *
 *========================================================================**/

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

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
#define RDS_STRINGIFY_HELPER(x) #x
#define RDS_STRINGIFY(x) RDS_STRINGIFY_HELPER(x)

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

    /// @brief Gets the size of a DataFieldType in bytes
    /// @param type The type to get the size of
    /// @return The size of the type in bytes
    static std::size_t getSizeOfType(DataFieldType type) {
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

    /// @brief Gets the size of this DataField in bytes
    /// @return The size of the DataField in bytes
    std::size_t size() const {
        return DataField::getSizeOfType(type);
    }

    //
    // * Operators
    //

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

    /// @brief Creates a DataPrototype from a serialized format
    /// @param serialized A vector of bytes that represents the serialized format
    /// @return A result with the DataPrototype if successful, or an error message
    static Result<DataPrototype> fromSerializedFormat(std::vector<std::uint8_t> serialized) {
        DataPrototype proto;
        if (serialized.size() < 2) {
            return Result<DataPrototype>::errorResult("Serialized data too short");
        }

        proto._identifier = serialized[0];
        std::size_t numFields = serialized[1];
        std::size_t offset = 2;

        for (std::size_t i = 0; i < numFields; i++) {
            if (offset + 1 >= serialized.size()) {
                return Result<DataPrototype>::errorResult("Serialized data too short");
            }

            std::size_t nameSize = serialized[offset];
            offset++;

            if (offset + nameSize + 1 >= serialized.size()) {
                return Result<DataPrototype>::errorResult("Serialized data too short");
            }

            std::string name(serialized.begin() + offset, serialized.begin() + offset + nameSize);
            offset += nameSize;

            DataFieldType type = static_cast<DataFieldType>(serialized[offset]);
            offset++;

            proto._fields[name] = DataField(proto._size, type);
            proto._size += proto._fields[name].size();
        }

        return Result<DataPrototype>::ok(proto);
    }

    /// @brief Serializes the DataPrototype into a vector of bytes
    /// @return A vector of bytes that represents the serialized format
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

    /// @brief Adds a field to the DataPrototype
    /// @param name The name of the field, it will overwrite if it already exists
    /// @param type The type of the field
    /// @return A reference to the DataPrototype
    DataPrototype &addField(const std::string &name, DataFieldType type) {
        if (_fields.find(name) != _fields.end()) {
            _size -= _fields[name].size();
        }
        _fields[name] = DataField(_size, type);
        _size += _fields[name].size();
        return *this;
    }

    /// @brief Finds a field in the DataPrototype
    /// @param name The name of the field to find
    /// @return A Result with the DataField if successful, or an error message
    Result<DataField> findField(const std::string &name) const {
        auto it = _fields.find(name);
        if (it == _fields.end()) {
            std::string errorMessage = "Field not found: ";
            errorMessage += name;
            return Result<DataField>::errorResult(errorMessage);
        }
        return Result<DataField>::ok(it->second);
    }

    /// @brief Returns the size of the DataPrototype in bytes
    /// @return The size of the DataPrototype in bytes
    std::size_t size() const { return _size; }

    /// @brief Returns the number of fields in the DataPrototype
    /// @return The number of fields in the DataPrototype
    std::size_t numFields() const { return _fields.size(); }

    /// @brief Returns the identifier of the DataPrototype
    /// @return The identifier of the DataPrototype, which is used in messages
    std::uint8_t identifier() const { return _identifier; }

    //
    // * Operators
    //

    DataPrototype &operator=(const DataPrototype &other) {
        _identifier = other._identifier;
        _size = other._size;
        _fields = other._fields;
        return *this;
    }

   private:
    // The identifier is used in messages to identify the prototype
    std::uint8_t _identifier = 0;

    // The size of the DataPrototype in bytes
    std::size_t _size = 0;

    // The fields in the DataPrototype
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

    /// @brief Creates a DataBuffer from a prototype and data
    /// @param type The prototype to create the DataBuffer from
    /// @param data The data to put in the DataBuffer
    /// @return A Result with the DataBuffer if successful, or an error message
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

    /// @brief Tries to get a field from the DataBuffer
    /// @tparam T The C++ type to interpret the field as
    /// @param name The name of the field to get
    /// @return A Result with the field if successful, or an error message
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

    /// @brief Attempts to set a field in the DataBuffer
    /// @tparam T The C++ type to interpret the field as
    /// @param name The name of the field to set
    /// @param value The value to set the field to
    /// @return A Result with the value if successful, or an error message
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

    /// @brief Gets the raw data in the DataBuffer
    /// @return A copy of the raw data in the DataBuffer
    std::vector<std::uint8_t> data() const { return _data; }

    /// @brief Returns the size of the DataBuffer in bytes
    /// @return The size of the DataBuffer in bytes
    std::size_t size() const { return _data.size(); }

    /// @brief Returns the DataPrototype of the DataBuffer
    /// @return The DataPrototype of the DataBuffer
    DataPrototype type() const { return _type; }

    //
    // * Operators
    //

    DataBuffer &operator=(const DataBuffer &other) {
        _type = other._type;
        _data = other._data;
        return *this;
    }

   private:
    /// @brief The DataPrototype that describes the format of the data
    DataPrototype _type;

    /// @brief The raw data in the DataBuffer
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

/// @brief Enum for the different types of messages
enum MessageType : std::uint8_t {
    /// @brief A request message, akin to a GET request in HTTP
    REQUEST,
    /// @brief A response message, akin to a POST request in HTTP
    RESPONSE,
    /// @brief An error message, akin to a 500
    ERROR,
};

static const std::size_t numMessageTypes = 3;

/// @brief A struct for containing meta information about a message
struct MessageHeader {
    /// @brief The type of the message
    MessageType type;  // 1 byte

    /// @brief The prototype handle of the message, used for looking up the prototype
    std::uint8_t prototypeHandle;  // 1 byte

    /// @brief The message number of the message, used for identifying the message
    std::uint16_t messageNumber;  // 2 bytes

    MessageHeader() : type(MessageType::REQUEST), prototypeHandle(0), messageNumber(0) {}
    MessageHeader(MessageType type, std::uint8_t prototypeHandle, std::uint16_t messageNumber)
        : type(type), prototypeHandle(prototypeHandle), messageNumber(messageNumber) {}

    /// @brief Takes a serialized message header and creates a MessageHeader
    /// @param serialized The serialized message header
    /// @return The MessageHeader if successful, or an error message
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

    /// @brief Inserts the serialized message header into a vector
    /// @param serialized The vector to insert the serialized message header into
    void insertSerialized(std::vector<std::uint8_t> &serialized) const {
        serialized.push_back(static_cast<std::uint8_t>(type));
        serialized.push_back(prototypeHandle);
        serialized.push_back(static_cast<std::uint8_t>(messageNumber >> 8));
        serialized.push_back(static_cast<std::uint8_t>(messageNumber));
    }
};

/// @brief A representation of a message, which is sent over a communication channel
/// @note Contains a header and a DataBuffer, which holds the data
class Message {
   public:
    Message() : _header(MessageHeader()), _buffer(DataBuffer()) {}
    Message(const Message &other) : _header(other._header), _buffer(other._buffer) {}

    Message(const MessageHeader &header, const DataBuffer &data) : _header(header), _buffer(data) {
        serveMessageConstructorWarnings(data.type().identifier(), header.type);
    }

    Message(MessageType type, const DataBuffer &data) : _header(MessageHeader(type, data.type().identifier(), _messageNumber++)), _buffer(data) {
        serveMessageConstructorWarnings(data.type().identifier(), type);
    }

    Message(MessageType type, const DataPrototype &proto) : _header(MessageHeader(type, proto.identifier(), _messageNumber++)), _buffer(DataBuffer(proto)) {
        serveMessageConstructorWarnings(proto.identifier(), type);
    }

    Message(MessageType type, const DataPrototype &proto, std::uint16_t messageNumber) : _header(MessageHeader(type, proto.identifier(), messageNumber)), _buffer(DataBuffer(proto)) {
        serveMessageConstructorWarnings(proto.identifier(), type);
    }

    Message(MessageType type, const DataBuffer &data, std::uint16_t messageNumber) : _header(MessageHeader(type, data.type().identifier(), messageNumber)), _buffer(data) {
        serveMessageConstructorWarnings(data.type().identifier(), type);
    }

    /// @brief Creates a response message from a request message and data
    /// @param request The request message to respond to
    /// @param data The DataBuffer to put in the response
    /// @return The response message
    static Message createResponse(const Message &request, const DataBuffer &data) {
        return Message(MessageType::RESPONSE, data, request._header.messageNumber);
    }

    /// @brief Creates a response message from a request message and prototype
    /// @param request The request message to respond to
    /// @param proto The prototype to create the response from
    /// @return The response message
    static Message createResposne(const Message &request, const DataPrototype &proto) {
        return Message(MessageType::RESPONSE, proto, request._header.messageNumber);
    }

    /// @brief Gets the prototype handle from a serialized message
    /// @param serialized The serialized message
    /// @return The prototype handle if successful, or an error message
    static std::uint8_t getPrototypeHandleFromBuffer(const std::vector<std::uint8_t> &serialized) {
        if (serialized.size() <= Message::_preambleSize) {
            return RESERVED_ERROR_PROTOTYPE;
        }

        return serialized[Message::_preambleSize];
    }

    /// @brief Creates a message from a serialized format
    /// @param proto The prototype to create the message from
    /// @param serialized The serialized message
    /// @return The message if successful, or an error message
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

        // check if the end sequence is correct
        for (std::size_t i = 0; i < Message::_endSequenceSize; i++) {
            if (serialized[serialized.size() - Message::_endSequenceSize + i] != Message::_endSequence[i]) {
                return Result<Message>::errorResult("Invalid end sequence");
            }
        }

        Result<MessageHeader> headerRes = MessageHeader::fromSerialized(std::vector<std::uint8_t>(serialized.begin() + Message::_preambleSize, serialized.begin() + Message::_preambleSize + 4));
        if (headerRes.isError()) {
            return Result<Message>::errorResult("Failed to create message header");
        }

        MessageHeader header = headerRes.value();

        // now check that the payload is the correct size
        std::size_t expectedSize = Message::_completeHeaderSize + proto.size() + Message::_completeEndSequenceSize;
        if (serialized.size() != expectedSize) {
            std::stringstream ss;
            ss << "Message size mismatch, expected: " << expectedSize << ", got: " << serialized.size();
            return Result<Message>::errorResult(ss.str());
        }

        std::vector<std::uint8_t> data = std::vector<std::uint8_t>(serialized.begin() + Message::_completeHeaderSize, serialized.end() - Message::_completeEndSequenceSize);

        Result<DataBuffer> bufferRes = DataBuffer::createFromPrototype(proto, data);

        if (bufferRes.isError()) {
            return Result<Message>::errorResult("Failed to create data buffer");
        }

        return Result<Message>::ok(Message(header, bufferRes.value()));
    }

    /// @brief Serializes the message into a vector of bytes
    /// @return A vector of bytes that represents the serialized message
    std::vector<std::uint8_t> serialize() const {
        std::vector<std::uint8_t> serialized;
        serialized.insert(serialized.end(), Message::_preamble, Message::_preamble + Message::_preambleSize);
        _header.insertSerialized(serialized);
        std::vector<std::uint8_t> data = _buffer.data();
        serialized.insert(serialized.end(), data.begin(), data.end());
        serialized.insert(serialized.end(), Message::_endSequence, Message::_endSequence + Message::_endSequenceSize);
        return serialized;
    }

    /// @brief Gets a field from the DataBuffer in the message
    /// @tparam T The C++ type to interpret the field as
    /// @param name The name of the field to get
    /// @return The field if successful, or an error message
    template <typename T>
    Result<T> getField(const std::string &name) const {
        return _buffer.getField<T>(name);
    }

    /// @brief The same as getField, but sets the field instead
    /// @tparam T The C++ type to interpret the field as
    /// @param name The name of the field to set
    /// @param value The value to set the field to
    /// @return A Result with the value if successful, or an error message
    template <typename T>
    Result<T> setField(const std::string &name, T value) {
        return _buffer.setField(name, value);
    }

    /// @brief Returns the type of the message (REQUEST, RESPONSE, ERROR)
    /// @return The type of the message
    MessageType type() const { return _header.type; }

    /// @brief Returns a reference to the DataBuffer in the message
    /// @return The DataBuffer in the message
    DataBuffer &data() { return _buffer; }

    /// @brief Returns a const reference to the DataBuffer in the message
    /// @return The DataBuffer in the message
    std::uint16_t messageNumber() const { return _header.messageNumber; }

    /// @brief Prints the message to an output stream
    /// @param output The output stream to print the message to
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

    //
    // * Operators
    //

    Message &operator=(const Message &other) {
        _header = other._header;
        _buffer = other._buffer;
        return *this;
    }

    bool operator==(const Message &other) const {
        return _header.type == other._header.type && _header.prototypeHandle == other._header.prototypeHandle && _header.messageNumber == other._header.messageNumber && _buffer.data() == other._buffer.data();
    }

    bool operator!=(const Message &other) const { return !(*this == other); }

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

    /// @brief Prints warnings for message constructors, as they can be dangerous
    /// @param prototypeIdentifier The identifier of the prototype in the message
    /// @param type The type of the message
    void serveMessageConstructorWarnings(std::uint8_t prototypeIdentifier, MessageType type) {
        if (prototypeIdentifier == RESERVED_ERROR_PROTOTYPE) {
            RDSCOM_DEBUG_PRINT_ERRORLN("Invalid prototype please make sure to pass in a number to the DataPrototype constructor and do not use the identifier %d\n", RESERVED_ERROR_PROTOTYPE);
        }

        if (type == MessageType::RESPONSE) {
            RDSCOM_DEBUG_PRINT_ERRORLN("Creating a response message, this is not recommended, as it is not clear what the response is to. Please use the Message::Response() builder instead\n");
        }
    }
};

std::uint8_t Message::_preamble[3] = {(std::uint8_t)'R', (std::uint8_t)'D', (std::uint8_t)'S'};
std::size_t Message::_preambleSize = 3;
std::size_t Message::_completeHeaderSize = Message::_preambleSize + sizeof(MessageHeader);
std::uint16_t Message::_messageNumber = 0;
std::uint8_t Message::_endSequence[3] = {(std::uint8_t)'E', (std::uint8_t)'N', (std::uint8_t)'D'};
std::size_t Message::_endSequenceSize = 3;
std::size_t Message::_completeEndSequenceSize = Message::_endSequenceSize;

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

class DummyChannel : public CommunicationChannel {
   public:
    std::vector<std::uint8_t> receive() override {
        if (_data.empty()) {
            return {};
        }

        std::vector<std::uint8_t> data = _data;
        _data.clear();
        return data;
    }

    void send(const rdscom::Message &message) override {
        std::vector<std::uint8_t> serialized = message.serialize();
        _data.insert(_data.end(), serialized.begin(), serialized.end());
    }

   private:
    std::vector<std::uint8_t> _data;
};

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

class CommunicationInterfaceOptions {
   public:
    std::uint8_t maxRetries = 3;
    std::uint64_t retryTimeout = 1000;
    std::function<std::uint64_t()> timeFunction;

    CommunicationInterfaceOptions(std::uint8_t maxRetries, std::uint32_t retryTimeout, std::function<std::uint64_t()> timeFunction) : maxRetries(maxRetries), retryTimeout(retryTimeout), timeFunction(timeFunction) {}
    CommunicationInterfaceOptions() : maxRetries(3), retryTimeout(1000), timeFunction([]() { 
        RDSCOM_DEBUG_PRINT_ERRORLN("No time function set for CommunicationInterfaceOptions, please set a time function");
        return 0; }) {}
#ifdef RDSCOM_ARDUINO
    CommunicationInterfaceOptions(std::uint8_t maxRetries, std::uint32_t retryTimeout) : maxRetries(maxRetries), retryTimeout(retryTimeout), timeFunction(millis) {}
    CommunicationInterfaceOptions() : maxRetries(3), retryTimeout(1000), timeFunction(millis) {}
#endif
};

class CommunicationInterface {
   public:
    CommunicationInterface(CommunicationChannel &channel) : _channel(channel), _options(), _rxCallbacks(), _txCallbacks(), _errCallbacks() {}
    CommunicationInterface(CommunicationChannel &channel, CommunicationInterfaceOptions options) : _channel(channel), _options(options), _rxCallbacks(), _txCallbacks(), _errCallbacks() {}

    CommunicationInterface &addCallback(std::uint8_t type, MessageType msgType, std::function<void(const Message &message)> callback) {
        CallBackMap &map = getMap(msgType);
        if (map.find(type) == map.end()) {
            map[type] = CallBackList();
        }
        map[type].push_back(callback);
        return *this;
    }

    CommunicationInterface &addPrototype(const DataPrototype &proto) {
        std::uint8_t handle = proto.identifier();
        if (handle == RESERVED_ERROR_PROTOTYPE) {
            RDSCOM_DEBUG_PRINT_ERRORLN("Invalid prototype please make sure to pass in a number to the DataPrototype constructor and do not use the identifier %d\n", RESERVED_ERROR_PROTOTYPE);
            return *this;
        }
        _prototypes[handle] = proto;
        return *this;
    }

    /// @brief Handles all of the logic for the communication interface
    /// @details This function should be called in the main loop of the program
    void tick() {
        listen();
        // now check for acks
        std::vector<std::uint16_t> toRemove(_acksNeeded.size());
        for (auto &ack : _acksNeeded) {
            SentMessage &message = ack.second;
            if (_options.timeFunction() - message.timeSent > _options.retryTimeout) {
                if (message.numRetries < _options.maxRetries) {
                    sendMessage(message.message, false);
                    message.timeSent = _options.timeFunction();
                    message.numRetries++;
                } else {
                    toRemove.push_back(ack.first);
                }
            }
        }

        for (std::uint16_t messageNumber : toRemove) {
            RDSCOM_DEBUG_PRINTLN("Removing message number %d -- failed to get an ack before the timeout", messageNumber);
            _acksNeeded.erase(messageNumber);
        }
    }

    /// @brief Listens for messages on the communication channel
    /// @details This function will not handle acks, it literally just listens for messages
    /// and calls the appropriate callbacks
    /// If you want to handle acks, you should call tick() instead, which calls listen() and handles acks
    void listen() {
            std::vector<std::uint8_t> data = _channel.receive();
        if (data.size() == 0) {
            return;
        }

        std::uint8_t protoHandle = Message::getPrototypeHandleFromBuffer(data);

        if (_prototypes.find(protoHandle) == _prototypes.end()) {
            RDSCOM_DEBUG_PRINT_ERRORLN("No prototype found for message");
            return;
        }

        DataPrototype proto = _prototypes[protoHandle];
        RDSCOM_DEBUG_PRINTLN("Received message of type %d", proto.identifier());

        Message message = Message::fromSerialized(proto, data).value();
        _lastMessageTime = _options.timeFunction();

        if (message.type() == MessageType::RESPONSE) {
            if (_acksNeeded.find(message.messageNumber()) != _acksNeeded.end()) {
                _acksNeeded.erase(message.messageNumber());
            }
        }

        if (_rxCallbacks.find(message.type()) != _rxCallbacks.end()) {
            RDSCOM_DEBUG_PRINTLN("Calling callbacks for message type %d", message.type());
            for (const auto &callback : _rxCallbacks[message.type()]) {
                callback(message);
            }
        }
    }

    void sendMessage(const Message &message, bool ackRequired = true) {
        _channel.send(message);

        if (ackRequired && message.type() == MessageType::REQUEST) {
            _acksNeeded[message.messageNumber()] = SentMessage{message, 0, 0};
        } else if (ackRequired && message.type() == MessageType::RESPONSE) {
            RDSCOM_DEBUG_PRINT_ERRORLN("You cannot require an ack for a response message, as a response is the ack");
        }
    }

    Result<DataPrototype> getPrototype(std::uint8_t identifier) {
        // do we have the prototype?
        if (_prototypes.find(identifier) == _prototypes.end()) {
            return Result<DataPrototype>::errorResult("Prototype not found");
        }

        return Result<DataPrototype>::ok(_prototypes[identifier]);
    }

    std::uint64_t timeSinceLastRecieved() const { return _options.timeFunction() - _lastMessageTime; }

   private:
    struct SentMessage {
        Message message;
        std::uint32_t timeSent;
        std::uint8_t numRetries;
    };

    CallBackMap &getMap(MessageType type) {
        switch (type) {
            case MessageType::REQUEST:
                return _rxCallbacks;
            case MessageType::RESPONSE:
                return _txCallbacks;
            case MessageType::ERROR:
                return _errCallbacks;
        }

        return _rxCallbacks;
    }

    CommunicationChannel &_channel;
    CommunicationInterfaceOptions _options;
    CallBackMap _rxCallbacks;
    CallBackMap _txCallbacks;
    CallBackMap _errCallbacks;
    std::uint64_t _lastMessageTime = 0;

    std::map<std::uint8_t, DataPrototype> _prototypes;
    std::map<std::uint16_t, CommunicationInterface::SentMessage> _acksNeeded;  // message number -> message
};

}  // namespace rdscom

#endif  // __RDSCOM_H__