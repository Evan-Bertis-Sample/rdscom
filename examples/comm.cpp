#include <stdio.h>

#include <string>
#include <vector>
#include <chrono>

#include "rdscom.hpp"

using namespace rdscom;

#define MESSAGE_TYPE_PERSON 0
#define MESSAGE_TYPE_CAR 1

#define NUM_RETRIES 3
#define RETRY_DELAY 2000

DummyChannel g_channel;
CommunicationInterfaceOptions options {
    NUM_RETRIES, RETRY_DELAY,
    []() -> std::uint32_t { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count(); }
};

CommunicationInterface g_com(g_channel, options);

void onPersonMessage(const rdscom::Message &message) {
    std::cout << "Received person message\n";
    message.printClean(std::cout);

    Message response(MessageType::REQUEST, g_com.getPrototype(MESSAGE_TYPE_CAR).value());
    bool check = rdscom::check(
        rdscom::defaultErrorCallback(std::cerr),
        response.setField<std::uint8_t>("make", 1),
        response.setField<std::uint8_t>("model", 2),
        response.setField<std::uint16_t>("year", 2020)
    );

    if (check) {
        std::cerr << "Error setting fields\n";
        return;
    }

    g_com.sendMessage(response);
}

void onCarMessage(const rdscom::Message &message) {
    std::cout << "Received car message\n";
    message.printClean(std::cout);

    // send a response, of type person
    Message response = Message::createResponse(message, g_com.getPrototype(MESSAGE_TYPE_PERSON).value());

    bool check = rdscom::check(
        rdscom::defaultErrorCallback(std::cerr),
        response.setField<std::int8_t>("id", 1),
        response.setField<std::uint8_t>("age", 30)
    );

    if (check) {
        std::cerr << "Error setting fields\n";
        return;
    }

    g_com.sendMessage(response);
}



int main() {
    g_com.addPrototype(
           DataPrototype(MESSAGE_TYPE_PERSON)
               .addField("id", DataFieldType::INT8)
               .addField("age", DataFieldType::UINT8))
        .addPrototype(
            DataPrototype(MESSAGE_TYPE_CAR)
                .addField("make", DataFieldType::BYTE)
                .addField("model", DataFieldType::BYTE)
                .addField("year", DataFieldType::UINT16));

    g_com.addCallback(
        MESSAGE_TYPE_PERSON,
        MessageType::REQUEST,
        onPersonMessage
    );

    g_com.addCallback(
        MESSAGE_TYPE_CAR,
        MessageType::RESPONSE,
        onCarMessage
    );

    Message msg(MessageType::REQUEST, g_com.getPrototype(MESSAGE_TYPE_PERSON).value());

    bool error = rdscom::check(
        rdscom::defaultErrorCallback(std::cerr),
        msg.setField<std::int8_t>("id", 1),
        msg.setField<std::uint8_t>("age", 30)
    );

    if (error) {
        std::cerr << "Error setting fields\n";
        return 1;
    }

    g_com.sendMessage(msg);

    while (true) {
        g_com.tick();
        // sleep for a bit
        auto start = std::chrono::high_resolution_clock::now();

        while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count() < 1) {
        }

        if (g_com.timeSinceLastRecieved() > 2000) {
            std::cerr << "No messages received in 2 seconds -- this shouldn't happen in this program\n";
            return 1;
        }

    }
}