#if defined(GEODE_IS_ANDROID64)
#include <Geode/Geode.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>
#include "../../core/nxr_gui.hpp"
#include "../../core/nxr_safe_hook.hpp"

using namespace geode::prelude;

NXR_HACK_CREATE("Creator", "Editor Extension", "Increases the editor length by a factor of 128", false);

namespace {
    struct PatchSpec {
        uintptr_t offset;
        std::array<uint8_t, 4> original;
        std::array<uint8_t, 4> patched;
    };

    constexpr std::array<uint8_t, 4> kLengthOriginal = { 0x00, 0x60, 0x6A, 0x48 };
    constexpr std::array<uint8_t, 4> kLengthPatched = { 0x00, 0x60, 0xEA, 0x4B };
    constexpr std::array<uint8_t, 4> kScrollOriginalNegative = { 0xF0, 0x23, 0x74, 0xC9 };
    constexpr std::array<uint8_t, 4> kScrollPatchedNegative = { 0xFF, 0xFF, 0x7F, 0xFF };
    constexpr std::array<uint8_t, 4> kScrollOriginalPositive = { 0xF0, 0x23, 0x74, 0x49 };
    constexpr std::array<uint8_t, 4> kScrollPatchedPositive = { 0xFF, 0xFF, 0x7F, 0x7F };
    constexpr std::array<uint8_t, 4> kAltOriginal = { 0x80, 0x67, 0x6A, 0x48 };

    const std::vector<PatchSpec> kSpecs = {
        { 0x667224, kLengthOriginal, kLengthPatched },
        { 0x68EF18, kLengthOriginal, kLengthPatched },
        { 0x6C04BC, kLengthOriginal, kLengthPatched },
        { 0x68F460, kAltOriginal, kLengthPatched },
        { 0x68FB8C, kScrollOriginalNegative, kScrollPatchedNegative },
        { 0x68FB88, kScrollOriginalPositive, kScrollPatchedPositive }
    };

    std::vector<Patch*> g_patches;
    bool g_built = false;
    bool g_supported = false;

    bool buildPatches() {
        if (g_built) return g_supported;
        g_built = true;

        const uintptr_t base = geode::base::get();

        for (const auto& spec : kSpecs) {
            if (std::memcmp(reinterpret_cast<const void*>(base + spec.offset), spec.original.data(), spec.original.size()) != 0) {
                log::warn("NXR: Editor Extension is not supported on this GD version (bytes at 0x{:X} do not match)", spec.offset);
                return false;
            }
        }

        for (const auto& spec : kSpecs) {
            ByteVector bytes(spec.patched.begin(), spec.patched.end());
            auto res = Mod::get()->patch(reinterpret_cast<void*>(base + spec.offset), bytes);
            if (!res) {
                log::warn("NXR: Editor Extension failed to patch 0x{:X}", spec.offset);
                for (auto* patch : g_patches) (void) patch->disable();
                g_patches.clear();
                return false;
            }
            g_patches.push_back(res.unwrap());
        }

        g_supported = true;
        return true;
    }
}

$execute {
    auto& hack = NXR::Gui::get().getWindow("Creator").findHackByName("Editor Extension");

    hack.setHandler([](bool enabled) {
        if (!buildPatches()) return;

        for (auto* patch : g_patches) {
            if (patch) (void) patch->toggle(enabled);
        }
    });
}
#endif
