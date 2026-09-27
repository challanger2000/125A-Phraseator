#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace phraseator {

class MidiNoteTracker {
public:
    void clear() noexcept { counts_ = {}; }

    void noteOn(int pitch) noexcept {
        if (pitch < 0 || pitch >= static_cast<int>(counts_.size()))
            return;
        auto& count = counts_[static_cast<std::size_t>(pitch)];
        if (count < std::numeric_limits<std::uint16_t>::max())
            ++count;
    }

    void noteOff(int pitch) noexcept {
        if (pitch < 0 || pitch >= static_cast<int>(counts_.size()))
            return;
        auto& count = counts_[static_cast<std::size_t>(pitch)];
        if (count > 0u)
            --count;
    }

    bool held(int pitch) const noexcept {
        return pitch >= 0 &&
               pitch < static_cast<int>(counts_.size()) &&
               counts_[static_cast<std::size_t>(pitch)] > 0u;
    }

    int highestHeld() const noexcept {
        for (int pitch = static_cast<int>(counts_.size()) - 1; pitch >= 0; --pitch) {
            if (counts_[static_cast<std::size_t>(pitch)] > 0u)
                return pitch;
        }
        return -1;
    }

private:
    std::array<std::uint16_t, 128> counts_ {};
};

} // namespace phraseator
