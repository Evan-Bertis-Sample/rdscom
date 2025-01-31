#include <stdio.h>
#include <vector>
#include <cstddef>

#include "rdscom.hpp"

int main() {
    rdscom::DataPrototype type(0);
    type.addField("id", rdscom::DataFieldType::INT);
    type.addField("name", rdscom::DataFieldType::BYTE);
    type.addField("age", rdscom::DataFieldType::UINT);

    rdscom::DataBuffer buffer(type);
    buffer.setField<int>("id", 1);
    buffer.setField<std::uint8_t>("name", std::uint8_t('A'));
    buffer.setField<std::uint8_t>("age", 20);

    // get the fields
    printf("id: %d\n", buffer.getField<int>("id"));
    printf("name: %c\n", static_cast<char>(buffer.getField<std::uint8_t>("name")));
    printf("age: %u\n", buffer.getField<std::uint8_t>("age"));

    rdscom::Message message(rdscom::MessageType::REQUEST, buffer);
    std::vector<std::uint8_t> serialized = message.serialize();

    printf("Serialized message: ");
    for (std::uint8_t byte : serialized) {
        printf("%c", static_cast<char>(byte));
    }
    printf("\n");

    rdscom::Message deserialized = rdscom::Message::fromSerialized(type, serialized);
    printf("Deserialized message type: %d\n", deserialized.type());
    printf("Deserialized message data:\n");
    printf("id: %d\n", deserialized.data().getField<int>("id"));
    printf("name: %c\n", static_cast<char>(deserialized.data().getField<std::uint8_t>("name")));
    printf("age: %u\n", deserialized.data().getField<std::uint8_t>("age"));

    return 0;
}