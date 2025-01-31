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

#define RDS_COM_VERSION "0.1.0"
#define RDS_DEBUG_ENABLED 1

namespace rdscom {

/**========================================================================
 *                           UTILITIES
 *========================================================================**/

template <typename T>
class Result {
   public:
    static Result<T> ok(T value) { return Result<T>(value); }
    static Result<T> error() { return Result<T>(true); }
    static Result<T> error(const char *errorMessage) { return Result<T>(true, errorMessage); }

    Result() : _error(true) {}

    Result<T> operator=(const Result<T> &other) {
        _value = other._value;
        _error = other._error;
        _errorMessage = other._errorMessage;
        return *this;
    }

    T value() const { return _value; }
    bool isError() const { return _error; }

   private:
    T _value;
    bool _error;
    const char *_errorMessage;

    Result(T value) : _value(value), _error(false) {}
    Result(bool error) : _error(error) {}
    Result(bool error, const char *errorMessage) : _error(error), _errorMessage(errorMessage) {}
};

#ifdef RDS_DEBUG_ENABLED
#define RDS_COLOR_PURPLE "\033[95m"
#define RDS_COLOR_RETURN "\033[0m"
#define RDS_LINE __LINE__

#define RDS_PREFIX RDS_COLOR_PURPLE "[rdscom:" RDS_LINE "] " RDS_COLOR_RETURN

#define RDS_DEBUG_PRINT(fmt, ...) \
    fprintf(stdout, RDS_PREFIX fmt, ##__VA_ARGS__)

#define RDS_DEBUG_PRINTLN(fmt, ...) \
    fprintf(stdout, RDS_PREFIX fmt "\n", ##__VA_ARGS__)

#define RDS_DEBUG_PRINT_ERROR(fmt, ...) \
    fprintf(stderr, RDS_PREFIX fmt, ##__VA_ARGS__)

#define RDS_DEBUG_PRINT_ERRORLN(fmt, ...) \
    fprintf(stderr, RDS_PREFIX fmt "\n", ##__VA_ARGS__)

#else
#define RDS_DEBUG_PRINT(fmt, ...)
#define RDS_DEBUG_PRINTLN(fmt, ...)
#define RDS_DEBUG_PRINT_ERROR(fmt, ...)
#define RDS_DEBUG_PRINT_ERRORLN(fmt, ...)
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
    INT,
    UINT,
    FLOAT,
    DOUBLE,
    BYTE,
    BOOL,
    NONE,
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
            case DataFieldType::INT:
                return sizeof(int);
            case DataFieldType::UINT:
                return sizeof(unsigned int);
            case DataFieldType::FLOAT:
                return sizeof(float);
            case DataFieldType::DOUBLE:
                return sizeof(double);
            case DataFieldType::BYTE:
                return sizeof(std::uint8_t);
            case DataFieldType::BOOL:
                return sizeof(bool);
            case DataFieldType::NONE:
                return 0;
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

    static DataPrototype fromSerialized(std::vector<std::uint8_t> serialized) {
        return DataPrototype(serialized);
    }

    DataPrototype addField(const std::string &name, DataFieldType type) {
        _fields[name] = DataField(_size, type);
        _size += _fields[name].size();
        return *this;
    }

    Result<DataField> findField(const std::string &name) const {
        auto it = _fields.find(name);
        if (it == _fields.end()) {
            return Result<DataField>::error("Field not found");
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
            return Result<DataBuffer>::error("Invalid prototype");
        }

        if (data.size() != type.size()) {
            return Result<DataBuffer>::error("Data size mismatch");
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
            return Result<T>::error("Field not found");
        }

        DataField field = fieldRes.value();

        if (sizeof(T) != field.size()) {
            // std::cerr << "Field size mismatch" << std::endl;
            return Result<T>::error("Field size mismatch");
        }

        T value;
        std::memcpy(&value, _data.data() + field.offset, sizeof(T));
        return Result<T>::ok(value);
    }

    template <typename T>
    void setField(const std::string &name, T value) {
        static_assert(std::is_arithmetic<T>::value, "setField only supports arithmetic types and std::uint8_t");
        static_assert(sizeof(T) <= sizeof(int64_t), "setField only supports types up to 64 bits");

        Result<DataField> fieldRes = _type.findField(name);
        if (fieldRes.isError()) {
            return;
        }

        DataField field = fieldRes.value();

        if (sizeof(T) != field.size()) {
            // std::cerr << "Field size mismatch" << std::endl;
            return;
        }

        std::memcpy(_data.data() + field.offset, &value, sizeof(T));
    }

    std::vector<std::uint8_t> data() const { return _data; }
    std::size_t size() const { return _data.size(); }

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

class Message {
   public:
    Message() : _type(MessageType::REQUEST), _buffer(DataPrototype()) {}
    Message(const Message &other) : _type(other._type), _buffer(other._buffer) {}
    Message(MessageType type, const DataBuffer &data) : _type(type), _buffer(data) {}

    static Result<Message> fromSerialized(const DataPrototype &proto, const std::vector<std::uint8_t> &serialized) {
        if (proto.identifier() == RESERVED_ERROR_PROTOTYPE) {
            return Result<Message>::error("Invalid prototype");
        }

        if (serialized.size() == 0) {
            return Result<Message>::error("Empty message");
        }

        MessageType type = static_cast<MessageType>(serialized[0]);
        Result<DataBuffer> bufferRes = DataBuffer::createFromPrototype(proto, std::vector<std::uint8_t>(serialized.begin() + 1, serialized.end()));
        if (bufferRes.isError()) {
            return Result<Message>::error("Failed to create data buffer");
        }

        return Result<Message>::ok(Message(type, bufferRes.value()));
    }

    MessageType type() const { return _type; }
    DataBuffer &data() { return _buffer; }

    template <typename T>
    Result<T> getField(const std::string &name) const {
        return _buffer.getField<T>(name);
    }

    template <typename T>
    void setField(const std::string &name, T value) {
        _buffer.setField(name, value);
    }

    std::vector<std::uint8_t> serialize() const {
        std::vector<std::uint8_t> serialized;
        serialized.push_back(static_cast<std::uint8_t>(_type));
        std::vector<std::uint8_t> data = _buffer.data();
        serialized.insert(serialized.end(), data.begin(), data.end());
        return serialized;
    }

   private:
    MessageType _type;
    DataBuffer _buffer;
};

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
    virtual std::vector<std::uint8_t> receive() = 0;
    virtual void send(const Message &message) = 0;
};

class TestLossyChannel : public CommunicationChannel {
   public:
    TestLossyChannel() : _lossRate(0) {
        TestLossyChannel other = TestLossyChannel(0, *this);
        _other = other;
    }

    TestLossyChannel operator=(const TestLossyChannel &other) {
        _lossRate = other._lossRate;
        _other = other._other;
        return *this;
    }

    TestLossyChannel(double lossRate, TestLossyChannel &other) : _lossRate(lossRate), _other(other) {};

    operator CommunicationChannel &() { return *this; }

    std::vector<std::uint8_t> receive() {
        std::vector<std::uint8_t> received;
        for (std::uint8_t byte : _rxBuffer) {
            if (rand() % 100 < _lossRate * 100) {
                continue;
            }
            received.push_back(byte);
        }
        return received;
    }

    void send(const Message &message) {
        std::vector<std::uint8_t> serialized = message.serialize();
        _other._rxBuffer = serialized;
    }

   private:
    double _lossRate;
    TestLossyChannel &_other;
    std::vector<std::uint8_t> _rxBuffer;
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

class CommunicationInterface {
   public:
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

   private:
    CommunicationChannel &_channel;
    CallBackMap _rxCallbacks;
    CallBackMap _txCallbacks;
}

}  // namespace rdscom

#endif  // __RDSCOM_H__