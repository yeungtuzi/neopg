#include <sstream>

#include "gtest/gtest.h"

#include <neopg/openpgp/compressed_data_packet.h>

#include <memory>

using namespace NeoPG;

TEST(NeoPGTest, openpg_compressed_data_packet_test) {
  {
    std::stringstream out;
    OpenPGP::UncompressedDataPacket packet;
    packet.write(out);
    ASSERT_EQ(out.str(), std::string("\xC8\x01"
                                     "\x00",
                                     3));
  }

  {
    std::stringstream out;
    OpenPGP::DeflateCompressedDataPacket packet;
    packet.write(out);
    ASSERT_EQ(out.str(), std::string("\xC8\x01"
                                     "\x01",
                                     3));
  }

  {
    std::stringstream out;
    OpenPGP::ZlibCompressedDataPacket packet;
    packet.write(out);
    ASSERT_EQ(out.str(), std::string("\xC8\x01"
                                     "\x02",
                                     3));
  }

  {
    std::stringstream out;
    OpenPGP::Bzip2CompressedDataPacket packet;
    packet.write(out);
    ASSERT_EQ(out.str(), std::string("\xC8\x01"
                                     "\x03",
                                     3));
  }
}
