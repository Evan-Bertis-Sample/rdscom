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

    rdscom::check(
        [](const char *error) {
            printf("Error: %s\n", error);
            return 1;
        },
        buffer.setField<int>("id", 1),
        buffer.setField<std::uint8_t>("name", std::uint8_t('A')),
        buffer.setField<std::uint8_t>("age", 20),
        buffer.setField<std::uint8_t>("pain", 20)
    );



    // get the fields
    printf("id: %d\n", buffer.getField<int>("id").value());
    printf("name: %c\n", static_cast<char>(buffer.getField<std::uint8_t>("name").value()));
    printf("age: %u\n", buffer.getField<std::uint8_t>("age").value());

    rdscom::Message message(rdscom::MessageType::REQUEST, buffer);

    message.setField<int>("id", 1);

    std::vector<std::uint8_t> serialized = message.serialize();

    message.printClean(std::cout);;

    rdscom::Result<rdscom::Message> deserializedRes = rdscom::Message::fromSerialized(type, serialized);

    if (deserializedRes.isError()) {
        printf("Error deserializing message\n");
        printf("Error: %s\n", deserializedRes.error());
        return 1;
    }

    rdscom::Message deserialized = deserializedRes.value();

    printf("Deserialized message type: %d\n", deserialized.type());
    printf("Deserialized message data:\n");
    printf("id: %d\n", deserialized.data().getField<int>("id").value());
    printf("name: %c\n", static_cast<char>(deserialized.data().getField<std::uint8_t>("name").value()));
    printf("age: %u\n", deserialized.data().getField<std::uint8_t>("age").value());

    return 0;
}