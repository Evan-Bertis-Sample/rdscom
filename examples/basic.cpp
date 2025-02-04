#include <stdio.h>
#include <vector>
#include <cstddef>
#include <string>

#define RDSCOM_SOCKETS
#include "rdscom.hpp"


int main() {
    rdscom::DataPrototype type(0);
    type.addField("id", rdscom::DataFieldType::INT8);
    type.addField("name", rdscom::DataFieldType::BYTE);
    type.addField("age", rdscom::DataFieldType::UINT8);

    rdscom::DataBuffer buffer(type);

    bool error = rdscom::check(
        rdscom::defaultErrorCallback(std::cerr),
        buffer.setField<std::uint8_t>("id", 1),
        buffer.setField<std::uint8_t>("name", std::uint8_t('A')),
        buffer.setField<std::uint8_t>("age", 20)
        // buffer.setField<std::uint8_t>("pain", 20)
    );

    if (error) {
        std::cerr << "Error setting field\n";
        return 1;
    }

    rdscom::Message message(rdscom::MessageType::REQUEST, buffer);

    error = rdscom::check(
        rdscom::defaultErrorCallback(std::cerr),
        message.setField<std::uint8_t>("id", 20)
    );

    if (error) {
        std::cerr << "Error setting field\n";
        return 1;
    }

    std::cout << "Original message:\n";
    message.printClean(std::cout);

    std::vector<std::uint8_t> serialized = message.serialize();
    rdscom::Result<rdscom::Message> deserializedRes = rdscom::Message::fromSerialized(type, serialized);
    if (deserializedRes.isError()) {
        std::cerr << "Error deserializing message\n";
        std::cerr << deserializedRes.error() << "\n";
        return 1;
    }

    rdscom::Message deserialized = deserializedRes.value();

    std::cout << "Deserialized message:\n";
    deserialized.printClean(std::cout);

    return 0;
}