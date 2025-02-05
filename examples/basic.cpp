#include <stdio.h>
#include <vector>
#include <cstddef>
#include <string>

#include "rdscom.hpp"

int main() {
    // Create a prototype for our message
    rdscom::DataPrototype type(0);
    type.addField("id", rdscom::DataFieldType::INT8)
        .addField("name", rdscom::DataFieldType::BYTE)
        .addField("age", rdscom::DataFieldType::UINT8);

    // Create a buffer with the prototype, which actually holds the data
    rdscom::DataBuffer buffer(type);

    // Set the initial fields in this buffer, which is the data we will serialize
    bool error = rdscom::check( // checks for errors in the Result<T> type
        rdscom::defaultErrorCallback(std::cerr), // callback for errors
        buffer.setField<std::uint8_t>("id", 1),
        buffer.setField<std::uint8_t>("name", std::uint8_t('A')),
        buffer.setField<std::uint8_t>("age", 20)
    );
    if (error) {
        std::cerr << "Error setting field\n";
        return 1;
    }

    // Create our message object, which holds some meta information about
    // the message, and the buffer with the data
    rdscom::Message message(rdscom::MessageType::REQUEST, buffer);

    // Lets check that that we can actually set a field in the message
    error = rdscom::check(
        rdscom::defaultErrorCallback(std::cerr),
        message.setField<std::uint8_t>("id", 20)
    );
    if (error) {
        std::cerr << "Error setting field\n";
        return 1;
    }

    // Serialize the message into a binary payload
    std::vector<std::uint8_t> serialized = message.serialize();

    // Create a new message object from the serialized data
    rdscom::Result<rdscom::Message> deserializedRes = rdscom::Message::fromSerialized(type, serialized);
    if (deserializedRes.isError()) {
        std::cerr << "Error deserializing message\n";
        std::cerr << deserializedRes.error() << "\n";
        return 1;
    }
    rdscom::Message deserialized = deserializedRes.value();

    // Now print out the messsages nicely, so we can check that they are the same manually
    std::cout << "Original message:\n";
    message.printClean(std::cout);
    std::cout << "Deserialized message:\n";
    deserialized.printClean(std::cout);

    // Check that the messages are the same
    if (message != deserialized) {
        std::cerr << "Messages are not equal\n";
        return 1;
    }

    return 0;
}