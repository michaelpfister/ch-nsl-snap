#pragma once

// How a pixelated or blurred area looks (F-26).
enum class RedactMode { Pixelate, Blur };
enum class RedactLevel { Light, Medium, Strong };

struct RedactStyle
{
    RedactMode mode = RedactMode::Pixelate;
    RedactLevel level = RedactLevel::Medium;

    // Pixel block size (pixelate) or blur radius, in logical pixels (D-015).
    int size() const
    {
        static constexpr int kPixelate[] = {6, 10, 16};
        static constexpr int kBlur[] = {4, 8, 14};
        const int i = int(level);
        return mode == RedactMode::Pixelate ? kPixelate[i] : kBlur[i];
    }

    bool operator==(const RedactStyle &) const = default;
};
