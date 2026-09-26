#include "step_clock.h"

#include <cassert>
#include <cmath>

using namespace phraseator;

int main() {
    StepClock clock;
    clock.configure(48000.0, 120.0);

    assert(std::fabs(clock.samplesPerStep() - 6000.0) < 1.0e-9);
    assert(clock.absoluteStepAt(0.0) == 0u);
    assert(clock.absoluteStepAt(5999.0) == 0u);
    assert(clock.absoluteStepAt(6000.0) == 1u);
    assert(clock.stepIndexAt(0.0) == 0u);
    assert(clock.stepIndexAt(6000.0 * 15.0) == 15u);
    assert(clock.stepIndexAt(6000.0 * 16.0) == 0u);

    clock.configure(48000.0, 60.0);
    assert(std::fabs(clock.samplesPerStep() - 12000.0) < 1.0e-9);

    return 0;
}
