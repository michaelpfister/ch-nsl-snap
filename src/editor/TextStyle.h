#pragma once

#include <QColor>
#include <QString>

// Optional box behind a text, for legibility on a busy background (F-25).
enum class TextBox {
    None,   // only the letters
    Light,  // white box, text in the chosen colour
    Filled, // box in the chosen colour, white text
};

// Content and look of a free text (F-25). Sizes are in logical pixels (D-015).
struct TextStyle
{
    QString text;
    QColor color = QColor(0xE5484D); // like the frames
    int fontSize = 16;               // pixel size, logical px
    bool bold = true;
    TextBox box = TextBox::None;

    // Everything except the text: what the panel changes.
    bool sameLook(const TextStyle &other) const
    {
        return color == other.color && fontSize == other.fontSize && bold == other.bold && box == other.box;
    }
    bool operator==(const TextStyle &) const = default;
};
