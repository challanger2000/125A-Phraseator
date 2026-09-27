#include "step_clock.h"
#include "test_common.h"

#include <cmath>
#include <limits>

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

    clock.configure(44100.0, 120.0);
    CHECK(std::fabs(clock.samplesPerStep() - 5512.5) < 1.0e-9);
    CHECK(clock.absoluteStepAt(5512.49) == 0u);
    CHECK(clock.absoluteStepAt(5512.5) == 1u);

    clock.configure(96000.0, 120.0);
    CHECK(std::fabs(clock.samplesPerStep() - 12000.0) < 1.0e-9);

    clock.configure(std::numeric_limits<double>::quiet_NaN(),
                    std::numeric_limits<double>::quiet_NaN());
    CHECK(std::fabs(clock.samplesPerStep() - 6000.0) < 1.0e-9);

    clock.configure(std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::infinity());
    CHECK(std::fabs(clock.samplesPerStep() - 6000.0) < 1.0e-9);

    // Tempo limits are part of the scheduler safety contract.
    clock.configure(48000.0, 1.0);
    CHECK(std::fabs(clock.samplesPerStep() - 36000.0) < 1.0e-9); // 20 BPM clamp
    clock.configure(48000.0, 1000.0);
    CHECK(std::fabs(clock.samplesPerStep() - 1800.0) < 1.0e-9); // 400 BPM clamp

    return 0;
}
