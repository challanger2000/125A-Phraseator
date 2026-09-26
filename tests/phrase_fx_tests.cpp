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

        std::vector<float> l(12001u, 0.0f);
        std::vector<float> r(12001u, 0.0f);
        l[0] = 1.0f;

        CHECK(fx.processBlock(l.data(), r.data(), l.size(), 120.0));
        CHECK(l[0] == 1.0f);
        CHECK(std::fabs(l[12000]) > 0.001f);
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

    return 0;
}
