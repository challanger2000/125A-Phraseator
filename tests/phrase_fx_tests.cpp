#include "phrase_fx.h"
#include "test_common.h"

#include <cmath>
#include <vector>

using namespace phraseator;

int main() {
    PhraseFx fx;
    fx.prepare(48000.0);

    {
        float l[4] {1.0f, 0.5f, -0.25f, 0.0f};
        float r[4] {-1.0f, 0.25f, 0.5f, 0.0f};
        const float refL[4] {1.0f, 0.5f, -0.25f, 0.0f};
        const float refR[4] {-1.0f, 0.25f, 0.5f, 0.0f};

        CHECK(fx.processBlock(l, r, 4u, 120.0));

        for (int i = 0; i < 4; ++i) {
            CHECK(l[i] == refL[i]);
            CHECK(r[i] == refR[i]);
        }
    }

    {
        fx.reset();
        fx.setDelayAmount(1.0f);

        // Allow the click-free 20 ms mix smoother to settle before measuring
        // the steady-state meaning of "100% Wet".
        std::vector<float> settleL(4800u, 0.0f);
        std::vector<float> settleR(4800u, 0.0f);
        fx.processBlock(settleL.data(), settleR.data(), settleL.size(), 120.0);

        std::vector<float> l(12001u, 0.0f);
        std::vector<float> r(12001u, 0.0f);
        l[0] = 1.0f;

        CHECK(fx.processBlock(l.data(), r.data(), l.size(), 120.0));

        // At settled 100% delay mix the dry impulse must be effectively gone.
        CHECK(std::fabs(l[0]) < 0.01f);
        CHECK(std::fabs(r[0]) < 0.01f);

        // The delayed signal must appear at one eighth-note (12000 samples
        // at 120 BPM / 48 kHz) and remain finite.
        CHECK(std::fabs(l[12000]) > 0.001f ||
              std::fabs(r[12000]) > 0.001f);
        CHECK(std::isfinite(l[12000]));
        CHECK(std::isfinite(r[12000]));
    }

    {
        fx.reset();
        fx.setDelayAmount(0.0f);
        fx.setFilterAmount(1.0f);

        float l[64] {};
        float r[64] {};
        l[0] = 1.0f;

        CHECK(fx.processBlock(l, r, 64u, 120.0));
        CHECK(l[0] < 1.0f);
        CHECK(l[0] > 0.0f);

        for (float x : l)
            CHECK(std::isfinite(x));
    }

    {
        // Measure the filter macro at multiple frequencies. 25% should leave
        // the mid band largely intact; 100% should clearly attenuate highs.
        const auto measureGain = [&](float amount, double frequency) {
            PhraseFx measured;
            measured.prepare(48000.0);
            measured.setFilterAmount(amount);

            constexpr std::size_t count = 48000u;
            std::vector<float> l(count);
            std::vector<float> r(count);
            constexpr double twoPi = 6.28318530717958647692;

            for (std::size_t i = 0; i < count; ++i) {
                const float x = static_cast<float>(
                    std::sin(twoPi * frequency *
                             static_cast<double>(i) / 48000.0));
                l[i] = x;
                r[i] = x;
            }

            measured.processBlock(l.data(), r.data(), count, 120.0);

            double inSq = 0.0;
            double outSq = 0.0;
            for (std::size_t i = count / 2u; i < count; ++i) {
                const double x = std::sin(
                    twoPi * frequency * static_cast<double>(i) / 48000.0);
                inSq += x * x;
                outSq += static_cast<double>(l[i]) *
                         static_cast<double>(l[i]);
            }

            return std::sqrt(outSq / inSq);
        };

        const double subtle1k = measureGain(0.25f, 1000.0);
        const double strong1k = measureGain(1.0f, 1000.0);
        const double subtle10k = measureGain(0.25f, 10000.0);
        const double strong10k = measureGain(1.0f, 10000.0);

        CHECK(subtle1k > 0.90);
        CHECK(strong1k > 0.60);
        CHECK(subtle10k > strong10k);
        CHECK(strong10k < 0.20);
    }


    {
        // Regression: disabling delay must logically discard the old tail
        // without requiring a physical full-buffer clear in setDelayAmount().
        fx.reset();
        fx.setDelayAmount(1.0f);

        std::vector<float> l(1000u, 0.0f);
        std::vector<float> r(1000u, 0.0f);
        l[0] = 1.0f;
        CHECK(fx.processBlock(l.data(), r.data(), l.size(), 120.0));

        fx.setDelayAmount(0.0f);

        std::vector<float> silenceL(13000u, 0.0f);
        std::vector<float> silenceR(13000u, 0.0f);
        fx.processBlock(silenceL.data(), silenceR.data(), silenceL.size(), 120.0);

        for (float x : silenceL)
            CHECK(std::isfinite(x));
        for (float x : silenceR)
            CHECK(std::isfinite(x));
    }

    return 0;
}
