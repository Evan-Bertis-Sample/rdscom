#include <stdio.h>

#include <string>
#include <vector>

#include "rdscom.hpp"

using namespace rdscom;

#define MESSAGE_TYPE_PERSON 0
#define MESSAGE_TYPE_CAR 1

class DummyChannel : public CommunicationChannel {
   public:
    std::vector<std::uint8_t> receive() override { return _data; }
    void send(const rdscom::Message &message) override {
        std::vector<std::uint8_t> serialized = message.serialize();
        _data.insert(_data.end(), serialized.begin(), serialized.end());
    }

   private:
    std::vector<std::uint8_t> _data;
};

void onPersonMessage(const rdscom::Message &message) {
    std::cout << "Received person message\n";
    message.printClean(std::cout);
}

void onCarMessage(const rdscom::Message &message) {
    std::cout << "Received car message\n";
    message.printClean(std::cout);
}

DummyChannel g_channel;
CommunicationInterface g_com(g_channel);

int main() {
    g_com.addPrototype(
           DataPrototype(MESSAGE_TYPE_PERSON)
               .addField("id", DataFieldType::INT8)
               .addField("name", DataFieldType::BYTE)
               .addField("age", DataFieldType::UINT8))
        .addPrototype(
            DataPrototype(MESSAGE_TYPE_CAR)
                .addField("make", DataFieldType::BYTE)
                .addField("model", DataFieldType::BYTE)
                .addField("year", DataFieldType::UINT16));

    


}