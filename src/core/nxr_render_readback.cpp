#include "nxr_render_readback.hpp"
#include <Geode/Geode.hpp>
#include <cstring>
#ifdef GEODE_IS_ANDROID
#include <dlfcn.h>
#endif

namespace {
    constexpr unsigned kPixelPackBuffer = 0x88EB;
    constexpr unsigned kStreamRead = 0x88E1;
    constexpr unsigned kMapReadBit = 0x0001;

#ifdef GEODE_IS_ANDROID
    using MapBufferRangeFn = void* (*)(unsigned, intptr_t, intptr_t, unsigned);
    using UnmapBufferFn = unsigned char (*)(unsigned);

    MapBufferRangeFn mapBufferRange() {
        static auto fn = reinterpret_cast<MapBufferRangeFn>(dlsym(RTLD_DEFAULT, "glMapBufferRange"));
        return fn;
    }

    UnmapBufferFn unmapBuffer() {
        static auto fn = reinterpret_cast<UnmapBufferFn>(dlsym(RTLD_DEFAULT, "glUnmapBuffer"));
        return fn;
    }
#endif

    void clearGlErrors() {
        for (int attempt = 0; attempt < 8 && glGetError() != GL_NO_ERROR; ++attempt) {
        }
    }
}

namespace NXR::Render {
    void FramebufferReadback::prepare(int width, int height) {
        release();
        m_width = width;
        m_height = height;
        m_writeSlot = 0;
        m_pendingSlot = -1;
        m_usesPixelBuffers = allocatePixelBuffers();
    }

    bool FramebufferReadback::allocatePixelBuffers() {
#ifdef GEODE_IS_ANDROID
        if (!mapBufferRange() || !unmapBuffer()) return false;

        clearGlErrors();
        glGenBuffers(2, m_pixelBuffers);
        const intptr_t byteCount = static_cast<intptr_t>(m_width) * m_height * 4;
        for (unsigned slot = 0; slot < 2; ++slot) {
            glBindBuffer(kPixelPackBuffer, m_pixelBuffers[slot]);
            glBufferData(kPixelPackBuffer, byteCount, nullptr, kStreamRead);
        }
        glBindBuffer(kPixelPackBuffer, 0);

        if (glGetError() == GL_NO_ERROR) return true;

        glDeleteBuffers(2, m_pixelBuffers);
        m_pixelBuffers[0] = 0;
        m_pixelBuffers[1] = 0;
        clearGlErrors();
        return false;
#else
        return false;
#endif
    }

    bool FramebufferReadback::copyFromPixelBuffer(unsigned slot, std::vector<uint8_t>& destination) {
#ifdef GEODE_IS_ANDROID
        const intptr_t byteCount = static_cast<intptr_t>(m_width) * m_height * 4;
        glBindBuffer(kPixelPackBuffer, m_pixelBuffers[slot]);
        void* mapped = mapBufferRange()(kPixelPackBuffer, 0, byteCount, kMapReadBit);
        if (!mapped) {
            glBindBuffer(kPixelPackBuffer, 0);
            return false;
        }
        std::memcpy(destination.data(), mapped, static_cast<size_t>(byteCount));
        unmapBuffer()(kPixelPackBuffer);
        glBindBuffer(kPixelPackBuffer, 0);
        return true;
#else
        (void)slot;
        (void)destination;
        return false;
#endif
    }

    bool FramebufferReadback::capture(std::vector<uint8_t>& destination) {
        if (!m_usesPixelBuffers) {
            glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, destination.data());
            return true;
        }

        const unsigned target = m_writeSlot;
        glBindBuffer(kPixelPackBuffer, m_pixelBuffers[target]);
        glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindBuffer(kPixelPackBuffer, 0);

        const bool delivered = m_pendingSlot >= 0 && copyFromPixelBuffer(static_cast<unsigned>(m_pendingSlot), destination);
        m_pendingSlot = static_cast<int>(target);
        m_writeSlot ^= 1u;
        return delivered;
    }

    bool FramebufferReadback::drain(std::vector<uint8_t>& destination) {
        if (!m_usesPixelBuffers || m_pendingSlot < 0) return false;
        const bool delivered = copyFromPixelBuffer(static_cast<unsigned>(m_pendingSlot), destination);
        m_pendingSlot = -1;
        return delivered;
    }

    void FramebufferReadback::release() {
        if (m_usesPixelBuffers) {
            glDeleteBuffers(2, m_pixelBuffers);
        }
        m_pixelBuffers[0] = 0;
        m_pixelBuffers[1] = 0;
        m_usesPixelBuffers = false;
        m_pendingSlot = -1;
    }
}
