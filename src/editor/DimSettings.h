#pragma once

// "Hintergrund abdunkeln" (F-27): everything outside the frames is darkened
// and optionally blurred a little; the frames keep full brightness.
struct DimSettings
{
    bool enabled = false;
    int strength = 45; // darkening in percent (0–80)
    int blur = 0;      // blur radius in logical pixels (0–10)

    // The three blur buttons in the tool rail; other values only come from
    // the sliders in the advanced mode.
    static constexpr int kBlurNone = 0;
    static constexpr int kBlurLight = 1;
    static constexpr int kBlurStrong = 3;

    bool operator==(const DimSettings &) const = default;
};
