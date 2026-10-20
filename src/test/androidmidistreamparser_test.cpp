#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <tuple>
#include <vector>

#include "controllers/midi/androidmidistreamparser.h"

namespace {
using ShortMessage = std::tuple<uint8_t, uint8_t, uint8_t>;

class AndroidMidiStreamParserTest : public ::testing::Test {
  protected:
    void feed(const std::vector<uint8_t>& bytes) {
        parser.feed(bytes.data(),
                bytes.size(),
                [this](uint8_t status, uint8_t data1, uint8_t data2) {
                    shortMessages.emplace_back(status, data1, data2);
                },
                [this](const uint8_t* data, std::size_t size) {
                    sysexMessages.emplace_back(data, data + size);
                });
    }

    AndroidMidiStreamParser parser;
    std::vector<ShortMessage> shortMessages;
    std::vector<std::vector<uint8_t>> sysexMessages;
};

TEST_F(AndroidMidiStreamParserTest, FragmentedMessagesAndRunningStatus) {
    feed({0x90, 0x11});
    EXPECT_TRUE(shortMessages.empty());
    feed({0x7F, 0x12, 0x00, 0xC0, 0x05, 0x06});
    EXPECT_EQ(shortMessages,
            (std::vector<ShortMessage>{{0x90, 0x11, 0x7F},
                    {0x90, 0x12, 0x00},
                    {0xC0, 0x05, 0x00},
                    {0xC0, 0x06, 0x00}}));
}

TEST_F(AndroidMidiStreamParserTest, FragmentedSysexWithRealtimeByte) {
    feed({0xF0, 0x00, 0x40});
    feed({0xF8, 0x05, 0xF7, 0x90, 0x01, 0x7F});
    ASSERT_EQ(sysexMessages.size(), 1U);
    EXPECT_EQ(sysexMessages[0], (std::vector<uint8_t>{0xF0, 0x00, 0x40, 0x05, 0xF7}));
    EXPECT_EQ(shortMessages,
            (std::vector<ShortMessage>{{0xF8, 0, 0}, {0x90, 0x01, 0x7F}}));
}

TEST_F(AndroidMidiStreamParserTest, InterruptedAndOversizeSysexCannotMakeControls) {
    feed({0xF0, 0x01, 0x02, 0x90, 0x10, 0x7F});
    std::vector<uint8_t> tooLong(AndroidMidiStreamParser::kMaxSysexBytes + 1, 0x01);
    tooLong[0] = 0xF0;
    feed(tooLong);
    feed({0x02, 0x03, 0xF7, 0x90, 0x11, 0x7F});
    EXPECT_TRUE(sysexMessages.empty());
    EXPECT_EQ(shortMessages,
            (std::vector<ShortMessage>{{0x90, 0x10, 0x7F}, {0x90, 0x11, 0x7F}}));
}

TEST_F(AndroidMidiStreamParserTest, ResetDiscardsPartialMessagesAfterQueueLoss) {
    feed({0x90, 0x12});
    parser.reset();
    feed({0x7F, 0x13, 0x7F, 0x90, 0x14, 0x7F});
    EXPECT_EQ(shortMessages, (std::vector<ShortMessage>{{0x90, 0x14, 0x7F}}));
}
}
