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

#include <map>
#include <string>
#include <iostream>
#include <vector>
#include <cstddef>
#include <memory>
#include <cstring>
#include <cstdint>

namespace rdscom {

enum DataFieldType {
    INT,
    UINT,
    FLOAT,
    DOUBLE,
    BYTE,
    BOOL,
    NONE,
};

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

class DataPrototype {
   public:
    DataPrototype(std::uint8_t packetType) : _packetType(packetType) {}
    DataPrototype(const DataPrototype &other)
        : _packetType(other._packetType),
          _size(other._size),
          _fields(other._fields) {}

    DataPrototype(std::vector<std::uint8_t> serialized) {
        _packetType = static_cast<std::uint8_t>(serialized[0]);
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
        _packetType = other._packetType;
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

    DataField findField(const std::string &name) const {
        auto it = _fields.find(name);
        if (it == _fields.end()) {
            return DataField();
        }
        return it->second;
    }

    std::size_t size() const { return _size; }
    std::size_t numFields() const { return _fields.size(); }
    std::uint8_t packetType() const { return _packetType; }

    std::vector<std::uint8_t> serializeFormat() const {
        std::vector<std::uint8_t> serialized;
        serialized.push_back(static_cast<std::uint8_t>(_packetType));
        serialized.push_back(static_cast<std::uint8_t>(_fields.size()));
        for (const auto &field : _fields) {
            serialized.push_back(static_cast<std::uint8_t>(field.first.size()));
            serialized.insert(serialized.end(), field.first.begin(), field.first.end());
            serialized.push_back(static_cast<std::uint8_t>(field.second.type));
        }
        return serialized;
    }

   private:
    std::uint8_t _packetType = 0;
    std::size_t _size = 0;
    std::map<std::string, DataField> _fields;  // field name -> field
};

class DataBuffer {
   public:
    DataBuffer(const DataPrototype &type) : _type(type) {
        _data.resize(_type.size());
    }

    DataBuffer(const DataBuffer &other) : _type(other._type), _data(other._data) {}

    DataBuffer &operator=(const DataBuffer &other) {
        _type = other._type;
        _data = other._data;
        return *this;
    }

    template <typename T>
    T getField(const std::string &name) const {
        static_assert(std::is_arithmetic<T>::value, "getField only supports arithmetic types and std::uint8_t");

        static_assert(sizeof(T) <= sizeof(int64_t), "getField only supports types up to 64 bits");

        DataField field = _type.findField(name);

        if (sizeof(T) != field.size()) {
            // std::cerr << "Field size mismatch" << std::endl;
            return T();
        }

        T value;
        std::memcpy(&value, _data.data() + field.offset, sizeof(T));
        return value;
    }

    template <typename T>
    void setField(const std::string &name, T value) {
        static_assert(std::is_arithmetic<T>::value, "setField only supports arithmetic types and std::uint8_t");
        static_assert(sizeof(T) <= sizeof(int64_t), "setField only supports types up to 64 bits");

        DataField field = _type.findField(name);

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

enum MessageType {
    REQUEST,
    RESPONSE,
    ERROR,
};

class Message {
   public:
    Message(MessageType type, const DataBuffer &data)
        : _type(type), _buffer(data) {}

    Message(const Message &other) : _type(other._type), _buffer(other._buffer) {}
    Message(std::vector<std::uint8_t> serialized) : _buffer(DataPrototype(0)) {
        _type = static_cast<MessageType>(serialized[0]);
        std::vector<std::uint8_t> data(serialized.begin() + 1, serialized.end());
        _buffer = DataBuffer(DataPrototype(data));
        _buffer.data() = data;
    }

    static Message fromSerialized(std::vector<std::uint8_t> serialized) {
        return Message(serialized);
    }

    MessageType type() const { return _type; }
    DataBuffer &data() { return _buffer; }

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

}  // namespace rdscom

#endif  // __RDSCOM_H__