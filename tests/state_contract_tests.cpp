#include "parameters.h"
#include "project_state.h"

#include <cassert>
#include <type_traits>

using namespace phraseator;

int main() {
    static_assert(static_cast<std::uint32_t>(ParameterId::Density) == 1000u);
    static_assert(static_cast<std::uint32_t>(ParameterId::GenerateTrigger) == 1300u);
    static_assert(ProjectState::kCurrentVersion == 1u);

    ProjectState state;
    assert(state.version == 1u);
    assert(state.randomSeed == 0x125A0001u);
    assert(state.density == ParameterDefaults::density);
    assert(state.variation == ParameterDefaults::variation);
    assert(state.repeat == 0.0f);
    assert(state.delayAmount == 0.0f);
    assert(!state.pitchToKey);
    assert(!state.lockPattern);

    return 0;
}
