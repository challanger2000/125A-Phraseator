#include "step_clock.h"
#include "test_common.h"

#include <cmath>

using namespace phraseator;

int main() {
    StepClock clock;
    clock.configure(48000.0, 120.0);

    CHECK(std::fabs(clock.samplesPerStep() - 6000.0) < 1.0e-9);
    CHECK(clock.absoluteStepAt(0.0) == 0u);
    CHECK(clock.absoluteStepAt(5999.0) == 0u);
    CHECK(clock.absoluteStepAt(6000.0) == 1u);
    CHECK(clock.stepIndexAt(0.0) == 0u);
    CHECK(clock.stepIndexAt(6000.0 * 15.0) == 15u);
    CHECK(clock.stepIndexAt(6000.0 * 16.0) == 0u);

    clock.configure(48000.0, 60.0);
    CHECK(std::fabs(clock.samplesPerStep() - 12000.0) < 1.0e-9);

    return 0;
}
