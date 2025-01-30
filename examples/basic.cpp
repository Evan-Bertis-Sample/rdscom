#include <stdio.h>

#include "rdscom.hpp"

int main() {
    rdscom::DataPrototype type(0);
    type.addField("id", rdscom::DataFieldType::INT);
    type.addField("name", rdscom::DataFieldType::BYTE);
    type.addField("age", rdscom::DataFieldType::UINT);

    rdscom::DataBuffer buffer(type);
    buffer.setField<int>("id", 1);
    buffer.setField<std::byte>("name", std::byte('A'));
    buffer.setField<std::uint8_t>("age", 20);

    rdscom::Message message(rdscom::MessageType::REQUEST, buffer);
    std::vector<std::byte> serialized = message.serialize();

    printf("Serialized message: ");
    for (std::byte byte : serialized) {
        printf("%c", static_cast<char>(byte));
    }
    printf("\n");

    rdscom::Message deserialized = rdscom::Message::fromSerialized(serialized);
    printf("Deserialized message type: %d\n", deserialized.type());
    printf("Deserialized message data:\n");
    printf("id: %d\n", deserialized.data().getField<int>("id"));
    printf("name: %c\n", static_cast<char>(deserialized.data().getField<std::byte>("name")));
    printf("age: %u\n", deserialized.data().getField<std::uint8_t>("age"));

    return 0;
}