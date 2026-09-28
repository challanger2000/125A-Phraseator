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
        // Exact tempo-sync regression at 120 BPM / 48 kHz.
        // Quarter note = 24000 samples.
        const int expected[7] {24000, 12000, 18000, 8000, 6000, 9000, 4000};

        for (int division = 1; division <= 7; ++division) {
            PhraseFx measured;
            measured.prepare(48000.0);
            measured.setDelayAmount(1.0f);
            measured.setDelayDivision(division);

            std::vector<float> settleL(48000u, 0.0f);
            std::vector<float> settleR(48000u, 0.0f);
            measured.processBlock(
                settleL.data(), settleR.data(), settleL.size(), 120.0);

            const std::size_t count =
                static_cast<std::size_t>(expected[division - 1] + 2);
            std::vector<float> l(count, 0.0f);
            std::vector<float> r(count, 0.0f);
            l[0] = 1.0f;

            CHECK(measured.processBlock(
                l.data(), r.data(), l.size(), 120.0));

            const int target = expected[division - 1];
            float peak = 0.0f;
            for (int offset = -2; offset <= 2; ++offset) {
                const auto index = static_cast<std::size_t>(target + offset);
                peak = std::max(peak, std::fabs(l[index]));
                peak = std::max(peak, std::fabs(r[index]));
            }
            CHECK(peak > 0.001f);
        }
    }

    {
        PhraseFx bypassed;
        bypassed.prepare(48000.0);
        bypassed.setDelayAmount(1.0f);
        bypassed.setDelayDivision(0);

        float l[4] {1.0f, 0.5f, -0.25f, 0.0f};
        float r[4] {-1.0f, 0.25f, 0.5f, 0.0f};
        const float refL[4] {1.0f, 0.5f, -0.25f, 0.0f};
        const float refR[4] {-1.0f, 0.25f, 0.5f, 0.0f};

        CHECK(bypassed.processBlock(l, r, 4u, 120.0));
        for (int i = 0; i < 4; ++i) {
            CHECK(l[i] == refL[i]);
            CHECK(r[i] == refR[i]);
        }
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
        const auto measureGain = [&](float amount, double frequency, int mode) {
            PhraseFx measured;
            measured.prepare(48000.0);
            measured.setFilterMode(mode);
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

        const double lpSubtle1k = measureGain(0.25f, 1000.0, 0);
        const double lpStrong1k = measureGain(1.0f, 1000.0, 0);
        const double lpSubtle10k = measureGain(0.25f, 10000.0, 0);
        const double lpStrong10k = measureGain(1.0f, 10000.0, 0);

        CHECK(lpSubtle1k > 0.90);
        CHECK(lpStrong1k > 0.60);
        CHECK(lpSubtle10k > lpStrong10k);
        CHECK(lpStrong10k < 0.20);

        const double hpStrong100 = measureGain(1.0f, 100.0, 1);
        const double hpStrong1k = measureGain(1.0f, 1000.0, 1);
        const double hpStrong10k = measureGain(1.0f, 10000.0, 1);

        CHECK(hpStrong100 < 0.10);
        CHECK(hpStrong1k < hpStrong10k);
        CHECK(hpStrong10k > 0.65);
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


    {
        // OFF must not secretly keep recording history that reappears when
        // the delay is enabled later.
        PhraseFx measured;
        measured.prepare(48000.0);
        measured.setDelayAmount(1.0f);
        measured.setDelayDivision(0);

        std::vector<float> hiddenL(13000u, 0.0f);
        std::vector<float> hiddenR(13000u, 0.0f);
        hiddenL[0] = 1.0f;
        CHECK(measured.processBlock(
            hiddenL.data(), hiddenR.data(), hiddenL.size(), 120.0));

        measured.setDelayDivision(2); // 1/8 = 12000 samples

        std::vector<float> afterL(13000u, 0.0f);
        std::vector<float> afterR(13000u, 0.0f);
        measured.processBlock(
            afterL.data(), afterR.data(), afterL.size(), 120.0);

        for (float x : afterL)
            CHECK(std::fabs(x) < 1.0e-6f);
        for (float x : afterR)
            CHECK(std::fabs(x) < 1.0e-6f);
    }

    {
        // Amount 0 likewise must not accumulate hidden history.
        PhraseFx measured;
        measured.prepare(48000.0);
        measured.setDelayDivision(2);
        measured.setDelayAmount(0.0f);

        std::vector<float> hiddenL(13000u, 0.0f);
        std::vector<float> hiddenR(13000u, 0.0f);
        hiddenL[0] = 1.0f;
        CHECK(measured.processBlock(
            hiddenL.data(), hiddenR.data(), hiddenL.size(), 120.0));

        measured.setDelayAmount(1.0f);

        std::vector<float> afterL(13000u, 0.0f);
        std::vector<float> afterR(13000u, 0.0f);
        measured.processBlock(
            afterL.data(), afterR.data(), afterL.size(), 120.0);

        for (float x : afterL)
            CHECK(std::fabs(x) < 1.0e-6f);
        for (float x : afterR)
            CHECK(std::fabs(x) < 1.0e-6f);
    }


    {
        // Bypass transition: turning division OFF from a settled 100% wet
        // state must return to full dry immediately, not attenuate the input.
        PhraseFx measured;
        measured.prepare(48000.0);
        measured.setDelayAmount(1.0f);
        measured.setDelayDivision(2);

        std::vector<float> settleL(48000u, 0.0f);
        std::vector<float> settleR(48000u, 0.0f);
        measured.processBlock(
            settleL.data(), settleR.data(), settleL.size(), 120.0);

        measured.setDelayDivision(0);
        float offL[1] {1.0f};
        float offR[1] {1.0f};
        CHECK(measured.processBlock(offL, offR, 1u, 120.0));
        CHECK(std::fabs(offL[0] - 1.0f) < 1.0e-6f);
        CHECK(std::fabs(offR[0] - 1.0f) < 1.0e-6f);

        // Re-enable with an empty history buffer. The first sample must still
        // be essentially dry; wet mix ramps in from zero instead of creating
        // a silence hole before the first new echo exists.
        measured.setDelayDivision(2);
        float onL[1] {1.0f};
        float onR[1] {1.0f};
        CHECK(measured.processBlock(onL, onR, 1u, 120.0));
        CHECK(onL[0] > 0.99f);
        CHECK(onR[0] > 0.99f);
    }

    {
        // Amount 0 follows the same dry/re-enable contract.
        PhraseFx measured;
        measured.prepare(48000.0);
        measured.setDelayAmount(1.0f);
        measured.setDelayDivision(2);

        std::vector<float> settleL(48000u, 0.0f);
        std::vector<float> settleR(48000u, 0.0f);
        measured.processBlock(
            settleL.data(), settleR.data(), settleL.size(), 120.0);

        measured.setDelayAmount(0.0f);
        float dryL[1] {0.75f};
        float dryR[1] {-0.5f};
        CHECK(measured.processBlock(dryL, dryR, 1u, 120.0));
        CHECK(std::fabs(dryL[0] - 0.75f) < 1.0e-6f);
        CHECK(std::fabs(dryR[0] + 0.5f) < 1.0e-6f);

        measured.setDelayAmount(1.0f);
        float restartL[1] {0.75f};
        float restartR[1] {-0.5f};
        CHECK(measured.processBlock(restartL, restartR, 1u, 120.0));
        CHECK(std::fabs(restartL[0]) > 0.74f);
        CHECK(std::fabs(restartR[0]) > 0.49f);
    }


    {
        // Long realtime tail must settle cleanly without NaN/Inf/denormal
        // residue after delay feedback and filtering decay into silence.
        PhraseFx measured;
        measured.prepare(48000.0);
        measured.setDelayAmount(0.7f);
        measured.setDelayDivision(5);
        measured.setFilterMode(0);
        measured.setFilterAmount(0.5f);

        std::vector<float> exciteL(1u, 1.0f);
        std::vector<float> exciteR(1u, 1.0f);
        CHECK(measured.processBlock(
            exciteL.data(), exciteR.data(), exciteL.size(), 120.0));

        std::vector<float> tailL(48000u * 8u, 0.0f);
        std::vector<float> tailR(48000u * 8u, 0.0f);
        measured.processBlock(
            tailL.data(), tailR.data(), tailL.size(), 120.0);

        for (float x : tailL) CHECK(std::isfinite(x));
        for (float x : tailR) CHECK(std::isfinite(x));

        CHECK(std::fabs(tailL.back()) < 1.0e-18f);
        CHECK(std::fabs(tailR.back()) < 1.0e-18f);
    }


    {
        // Slow-tempo boundary: at 20 BPM a quarter note is exactly 3 seconds.
        // The internal delay buffer must not clamp it shorter.
        PhraseFx slow;
        slow.prepare(48000.0);
        slow.setDelayAmount(1.0f);
        slow.setDelayDivision(1); // 1/4

        std::vector<float> settleL(48000u, 0.0f);
        std::vector<float> settleR(48000u, 0.0f);
        slow.processBlock(
            settleL.data(), settleR.data(), settleL.size(), 20.0);

        constexpr std::size_t expected = 144000u;
        std::vector<float> l(expected + 2u, 0.0f);
        std::vector<float> r(expected + 2u, 0.0f);
        l[0] = 1.0f;
        CHECK(slow.processBlock(l.data(), r.data(), l.size(), 20.0));

        float peak = 0.0f;
        for (int offset = -1; offset <= 1; ++offset) {
            const auto index = static_cast<std::size_t>(
                static_cast<long long>(expected) + offset);
            peak = std::max(peak, std::fabs(l[index]));
            peak = std::max(peak, std::fabs(r[index]));
        }
        CHECK(peak > 0.001f);
    }

    {
        // Boundary contract: exactly 1000 Hz is accepted by the processor.
        // A quarter note at 60 BPM must therefore be exactly 1000 samples,
        // not 48000 samples from an internal fallback rate.
        PhraseFx boundary;
        boundary.prepare(1000.0);
        boundary.setDelayAmount(1.0f);
        boundary.setDelayDivision(1);

        std::vector<float> settleL(5000u, 0.0f);
        std::vector<float> settleR(5000u, 0.0f);
        boundary.processBlock(
            settleL.data(), settleR.data(), settleL.size(), 60.0);

        std::vector<float> l(1002u, 0.0f);
        std::vector<float> r(1002u, 0.0f);
        l[0] = 1.0f;
        CHECK(boundary.processBlock(l.data(), r.data(), l.size(), 60.0));
        CHECK(std::fabs(l[1000]) > 0.001f || std::fabs(r[1000]) > 0.001f);
    }

    {
        // Low-rate boundary: every CUT target must remain Nyquist-safe and
        // monotonic at the exact minimum supported host rate.
        const auto measureLowRate = [](float amount, double frequency, int mode) {
            PhraseFx measured;
            measured.prepare(1000.0);
            measured.setFilterMode(mode);
            measured.setFilterAmount(amount);

            constexpr std::size_t count = 4000u;
            std::vector<float> l(count);
            std::vector<float> r(count);
            constexpr double twoPi = 6.28318530717958647692;

            for (std::size_t i = 0; i < count; ++i) {
                const float x = static_cast<float>(
                    std::sin(twoPi * frequency * static_cast<double>(i) / 1000.0));
                l[i] = x;
                r[i] = x;
            }

            measured.processBlock(l.data(), r.data(), count, 120.0);

            double inSq = 0.0;
            double outSq = 0.0;
            for (std::size_t i = count / 2u; i < count; ++i) {
                const double x = std::sin(
                    twoPi * frequency * static_cast<double>(i) / 1000.0);
                inSq += x * x;
                outSq += static_cast<double>(l[i]) * static_cast<double>(l[i]);
            }
            return std::sqrt(outSq / inSq);
        };

        const double lp25 = measureLowRate(0.25f, 400.0, 0);
        const double lp100 = measureLowRate(1.0f, 400.0, 0);
        CHECK(std::isfinite(lp25));
        CHECK(std::isfinite(lp100));
        CHECK(lp100 <= lp25 + 1.0e-9);

        const double hp25 = measureLowRate(0.25f, 50.0, 1);
        const double hp100 = measureLowRate(1.0f, 50.0, 1);
        CHECK(std::isfinite(hp25));
        CHECK(std::isfinite(hp100));
        CHECK(hp100 <= hp25 + 1.0e-9);
    }

    {
        // Lifecycle reset must discard an existing delay tail without
        // requiring a physical full-buffer clear.
        PhraseFx measured;
        measured.prepare(48000.0);
        measured.setDelayAmount(1.0f);
        measured.setDelayDivision(2);

        std::vector<float> exciteL(1000u, 0.0f);
        std::vector<float> exciteR(1000u, 0.0f);
        exciteL[0] = 1.0f;
        CHECK(measured.processBlock(
            exciteL.data(), exciteR.data(), exciteL.size(), 120.0));

        measured.reset();

        std::vector<float> afterL(13000u, 0.0f);
        std::vector<float> afterR(13000u, 0.0f);
        measured.processBlock(
            afterL.data(), afterR.data(), afterL.size(), 120.0);

        for (float x : afterL)
            CHECK(std::fabs(x) < 1.0e-6f);
        for (float x : afterR)
            CHECK(std::fabs(x) < 1.0e-6f);
    }

    return 0;
}
