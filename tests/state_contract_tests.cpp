#include "parameters.h"
#include "test_common.h"
#include "project_state.h"

#include <type_traits>

using namespace phraseator;

int main() {
    static_assert(static_cast<std::uint32_t>(ParameterId::Density) == 1000u);
    static_assert(static_cast<std::uint32_t>(ParameterId::GenerateTrigger) == 1300u);
    static_assert(static_cast<std::uint32_t>(ParameterId::RestartMode) == 1303u);
    static_assert(static_cast<std::uint32_t>(ParameterId::DelayDivision) == 1201u);
    static_assert(static_cast<std::uint32_t>(ParameterId::FilterMode) == 1205u);
    static_assert(ProjectState::kCurrentVersion == 9u);

    ProjectState state;
    CHECK(state.version == 9u);
    CHECK(state.randomSeed == 0x125A0001u);
    CHECK(state.density == ParameterDefaults::density);
    CHECK(state.variation == ParameterDefaults::variation);
    CHECK(state.repeat == 0.0f);
    CHECK(state.velocity == ParameterDefaults::velocity);
    CHECK(state.octaveMode == 0);
    CHECK(state.delayAmount == 0.0f);
    CHECK(!state.pitchToKey);
    CHECK(!state.lockPattern);
    CHECK(state.restartOnNote);
    CHECK(state.delayDivision == 2);
    CHECK(state.filterMode == 1);
    for (const auto& source : state.sources)
        CHECK(!source.muted);

    return 0;
}
