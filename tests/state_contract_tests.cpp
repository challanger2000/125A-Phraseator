#include "parameters.h"
#include "test_common.h"
#include "project_state.h"

#include <type_traits>

using namespace phraseator;

int main() {
    static_assert(static_cast<std::uint32_t>(ParameterId::Density) == 1000u);
    static_assert(static_cast<std::uint32_t>(ParameterId::GenerateTrigger) == 1300u);
    static_assert(static_cast<std::uint32_t>(ParameterId::RestartMode) == 1303u);
    static_assert(ProjectState::kCurrentVersion == 5u);

    ProjectState state;
    CHECK(state.version == 5u);
    CHECK(state.randomSeed == 0x125A0001u);
    CHECK(state.density == ParameterDefaults::density);
    CHECK(state.variation == ParameterDefaults::variation);
    CHECK(state.repeat == 0.0f);
    CHECK(state.delayAmount == 0.0f);
    CHECK(!state.pitchToKey);
    CHECK(!state.lockPattern);
    CHECK(state.restartOnNote);

    return 0;
}
