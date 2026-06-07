#ifndef LKGETFRAMEPARSER_HH
#define LKGETFRAMEPARSER_HH

#include "LKGETRawFrame.h"

#include <istream>

class LKGETFrameParser
{
  public:
    bool ReadNextFrame(std::istream& input, LKGETRawFrame& frame);

  private:
    bool ParseFrameBytes(const uint8_t* data, size_t size, LKGETRawFrame& frame) const;

    static uint16_t ReadBE16(const uint8_t* data);
    static uint32_t ReadBE24(const uint8_t* data);
    static uint32_t ReadBE32(const uint8_t* data);
    static uint64_t ReadBE48(const uint8_t* data);
};

#endif
