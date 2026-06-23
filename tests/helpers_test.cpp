#include "helpers_generated.h"
#include <cstdio>

static int g_failures = 0;
#define CHECK(cond) do { if(!(cond)) { printf("FAIL line %d: %s\n", __LINE__, #cond); ++g_failures; } } while(0)

int main() {
    uint32_t c = 0;
    // parseHexColor
    CHECK(parseHexColor(L"#ff0000", c) && c == 0xFFFF0000u);
    CHECK(parseHexColor(L"ff0000",  c) && c == 0xFFFF0000u);   // '#' optional
    CHECK(parseHexColor(L"#f00",    c) && c == 0xFFFF0000u);   // 3-digit expands
    CHECK(parseHexColor(L"#80ff0000", c) && c == 0x80FF0000u); // explicit alpha
    CHECK(!parseHexColor(L"", c));                              // empty -> false
    CHECK(!parseHexColor(L"nothex", c));                        // invalid -> false
    CHECK(!parseHexColor(L"#12345", c));                        // bad length -> false

    // parseLayout
    CHECK(parseLayout(L"pill") == ToastLayout::Pill);
    CHECK(parseLayout(L"tile") == ToastLayout::Tile);
    CHECK(parseLayout(L"minimal") == ToastLayout::Minimal);
    CHECK(parseLayout(L"") == ToastLayout::Pill);        // blank -> default
    CHECK(parseLayout(L"bogus") == ToastLayout::Pill);   // unknown -> default

    // computeToastRect
    RECT wa{0, 0, 1000, 1000};
    SIZE s{100, 40};
    RECT r = computeToastRect(Anchor::TopLeft, s, 0, 0, wa);
    CHECK(r.left == 0 && r.top == 0 && r.right == 100 && r.bottom == 40);
    r = computeToastRect(Anchor::BottomRight, s, 0, 0, wa);
    CHECK(r.left == 900 && r.top == 960 && r.right == 1000 && r.bottom == 1000);
    r = computeToastRect(Anchor::Center, s, 0, 0, wa);
    CHECK(r.left == 450 && r.top == 480);
    r = computeToastRect(Anchor::BottomCenter, s, 0, 10, wa); // offset pushes up from bottom
    CHECK(r.left == 450 && r.top == 950);
    r = computeToastRect(Anchor::TopLeft, s, 5, 7, wa);       // offset pushes in from top-left
    CHECK(r.left == 5 && r.top == 7);

    // shadowLayerAlpha: 0-100 opacity -> per-layer alpha; default 40 -> 10
    CHECK(shadowLayerAlpha(40) == 10);
    CHECK(shadowLayerAlpha(0) == 0);
    CHECK(shadowLayerAlpha(100) == 25);
    CHECK(shadowLayerAlpha(-5) == 0);     // clamps low
    CHECK(shadowLayerAlpha(150) == 25);   // clamps high

    // shadowMargin: enabled -> size + |offsetY| + 1; defaults 13,4 -> 18 (==old constant)
    CHECK(shadowMargin(true, 13, 4) == 18);
    CHECK(shadowMargin(true, 13, -4) == 18);  // negative offset, same magnitude
    CHECK(shadowMargin(true, 0, 0) == 1);
    CHECK(shadowMargin(false, 13, 4) == 2);   // disabled collapses to AA pad

    if (g_failures == 0) { printf("All tests passed\n"); return 0; }
    printf("%d failure(s)\n", g_failures);
    return 1;
}
