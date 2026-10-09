#pragma once
#include <cstdint>
#include <vector>

namespace NXR::Render {
    class FramebufferReadback {
    public:
        void prepare(int width, int height);
        bool capture(std::vector<uint8_t>& destination);
        bool drain(std::vector<uint8_t>& destination);
        void release();

    private:
        bool allocatePixelBuffers();
        bool copyFromPixelBuffer(unsigned slot, std::vector<uint8_t>& destination);

        int m_width = 0;
        int m_height = 0;
        bool m_usesPixelBuffers = false;
        unsigned m_pixelBuffers[2] = {0, 0};
        unsigned m_writeSlot = 0;
        int m_pendingSlot = -1;
    };
}
