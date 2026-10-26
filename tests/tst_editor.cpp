// Tests for the editor document (D-012): frames, undo/redo and export.

#include "editor/Commands.h"
#include "editor/DimSettings.h"
#include "editor/EditorScene.h"
#include "editor/FrameItem.h"
#include "editor/RedactItem.h"
#include "editor/ReplaceItem.h"
#include "imaging/TextSample.h"

#include <QPainter>

#include <QBuffer>
#include <QTest>
#include <QUndoStack>

namespace {

const QColor kRed(0xE5484D);
const QColor kBlue(0x0090FF);

QColor pixel(const QImage &image, int x, int y)
{
    return image.pixelColor(x, y);
}

FrameStyle squareStyle(const QColor &color, int width)
{
    FrameStyle style;
    style.color = color;
    style.width = width;
    style.rounded = false;
    return style;
}

} // namespace

class TestEditor : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_stack = new QUndoStack(this);
        m_scene = new EditorScene(m_stack, this);
        QImage base(200, 100, QImage::Format_ARGB32);
        base.fill(Qt::white);
        m_scene->setBaseImage(base, 1.0);
    }

    void cleanup()
    {
        // Commands refer to items, so the stack goes first (as in MainWindow).
        delete m_stack;
        delete m_scene;
    }

    void addFrameRendersAndUndoes()
    {
        auto *frame = new FrameItem(QRectF(20, 20, 100, 50), squareStyle(kRed, 4), 1.0);
        m_stack->push(new AddItemsCommand(m_scene, {frame}, QStringLiteral("add")));

        QCOMPARE(m_scene->frames().size(), 1);
        QVERIFY(frame->isSelected());
        QImage out = m_scene->renderImage();
        QCOMPARE(out.size(), QSize(200, 100));
        QCOMPARE(pixel(out, 21, 40), kRed);          // on the line (inside the rect)
        QCOMPARE(pixel(out, 60, 40), QColor(Qt::white)); // inside the frame
        QCOMPARE(pixel(out, 18, 40), QColor(Qt::white)); // outside: the line does not spill

        m_stack->undo();
        QCOMPARE(m_scene->frames().size(), 0);
        QCOMPARE(pixel(m_scene->renderImage(), 21, 40), QColor(Qt::white));

        m_stack->redo();
        QCOMPARE(m_scene->frames().size(), 1);
    }

    void exportHasNoHandles()
    {
        auto *frame = new FrameItem(QRectF(20, 20, 100, 50), squareStyle(kRed, 2), 1.0);
        m_stack->push(new AddItemsCommand(m_scene, {frame}, QStringLiteral("add")));
        QVERIFY(frame->isSelected());
        // A handle would be drawn white with a blue border around the corner.
        const QImage out = m_scene->renderImage();
        QCOMPARE(pixel(out, 16, 16), QColor(Qt::white));
        QCOMPARE(pixel(out, 23, 23), QColor(Qt::white));
    }

    void moveResizeAndStyleAreUndoable()
    {
        auto *frame = new FrameItem(QRectF(20, 20, 50, 30), squareStyle(kRed, 3), 1.0);
        m_stack->push(new AddItemsCommand(m_scene, {frame}, QStringLiteral("add")));

        frame->setPos(10, 5);
        m_stack->push(new MoveItemsCommand({{frame, QPointF(0, 0), QPointF(10, 5)}}));
        m_stack->push(new SetRectCommand(frame, QRectF(20, 20, 50, 30), QRectF(20, 20, 80, 40)));
        m_stack->push(new SetFrameStyleCommand({frame}, squareStyle(kBlue, 3)));

        QCOMPARE(frame->pos(), QPointF(10, 5));
        QCOMPARE(frame->rect(), QRectF(20, 20, 80, 40));
        QCOMPARE(frame->style().color, kBlue);
        QCOMPARE(pixel(m_scene->renderImage(), 31, 40), kBlue);

        m_stack->undo();
        QCOMPARE(frame->style().color, kRed);
        m_stack->undo();
        QCOMPARE(frame->rect(), QRectF(20, 20, 50, 30));
        m_stack->undo();
        QCOMPARE(frame->pos(), QPointF(0, 0));
    }

    void styleChangesMerge()
    {
        auto *frame = new FrameItem(QRectF(20, 20, 50, 30), squareStyle(kRed, 3), 1.0);
        m_stack->push(new AddItemsCommand(m_scene, {frame}, QStringLiteral("add")));
        const int before = m_stack->count();
        for (int width = 4; width <= 8; ++width)
            m_stack->push(new SetFrameStyleCommand({frame}, squareStyle(kRed, width)));
        QCOMPARE(m_stack->count(), before + 1); // slider drag = one step
        m_stack->undo();
        QCOMPARE(frame->style().width, 3);
    }

    void removeIsUndoable()
    {
        auto *frame = new FrameItem(QRectF(20, 20, 50, 30), squareStyle(kRed, 3), 1.0);
        m_stack->push(new AddItemsCommand(m_scene, {frame}, QStringLiteral("add")));
        m_stack->push(new RemoveItemsCommand(m_scene, {frame}));
        QCOMPARE(m_scene->frames().size(), 0);
        m_stack->undo();
        QCOMPARE(m_scene->frames().size(), 1);
        QVERIFY(frame->isSelected());
    }

    void lineWidthFollowsImageScale()
    {
        // On a scale-2 capture a 3 px frame is 6 image pixels wide.
        auto *frame = new FrameItem(QRectF(20, 20, 100, 50), squareStyle(kRed, 3), 2.0);
        m_stack->push(new AddItemsCommand(m_scene, {frame}, QStringLiteral("add")));
        const QImage out = m_scene->renderImage();
        QCOMPARE(pixel(out, 25, 45), kRed);
        QCOMPARE(pixel(out, 27, 45), QColor(Qt::white));
    }

    void scaleSurvivesSaveAndLoad()
    {
        QImage base(40, 20, QImage::Format_ARGB32);
        base.fill(Qt::white);
        m_scene->setBaseImage(base, 2.0);
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        QVERIFY(m_scene->renderImage().save(&buffer, "PNG"));
        const QImage loaded = QImage::fromData(png, "PNG");
        QCOMPARE(EditorScene::scaleFromMetadata(loaded), 2.0);

        QImage plain(10, 10, QImage::Format_ARGB32); // e.g. a screenshot from elsewhere
        plain.setDotsPerMeterX(2835);                 // 72 dpi
        QCOMPARE(EditorScene::scaleFromMetadata(plain), 1.0);
    }

    void numbersFollowCreationOrderAndRenumber()
    {
        FrameStyle numbered = squareStyle(kRed, 3);
        numbered.badge = BadgePosition::TopRight;
        auto *a = new FrameItem(QRectF(10, 10, 40, 30), numbered, 1.0);
        auto *plain = new FrameItem(QRectF(60, 10, 40, 30), squareStyle(kRed, 3), 1.0);
        auto *b = new FrameItem(QRectF(110, 10, 40, 30), numbered, 1.0);
        auto *c = new FrameItem(QRectF(10, 50, 40, 30), numbered, 1.0);
        for (FrameItem *f : {a, plain, b, c})
            m_stack->push(new AddItemsCommand(m_scene, {f}, QStringLiteral("add")));

        QCOMPARE(m_scene->numberOf(a), 1);
        QCOMPARE(m_scene->numberOf(plain), 0);
        QCOMPARE(m_scene->numberOf(b), 2);
        QCOMPARE(m_scene->numberOf(c), 3);

        // Deleting 2 renumbers 3 to 2; undo brings the old numbers back.
        m_stack->push(new RemoveItemsCommand(m_scene, {b}));
        QCOMPARE(m_scene->numberOf(c), 2);
        m_stack->undo();
        QCOMPARE(m_scene->numberOf(b), 2);
        QCOMPARE(m_scene->numberOf(c), 3);

        // Turning the number off on the first frame renumbers the others.
        FrameStyle off = numbered;
        off.badge = BadgePosition::None;
        m_stack->push(new SetFrameStyleCommand({a}, off));
        QCOMPARE(m_scene->numberOf(b), 1);
        QCOMPARE(m_scene->numberOf(c), 2);
    }

    void badgeIsDrawnOnTheCorner()
    {
        FrameStyle numbered = squareStyle(kRed, 2);
        numbered.badge = BadgePosition::TopRight;
        auto *frame = new FrameItem(QRectF(40, 40, 100, 40), numbered, 1.0);
        m_stack->push(new AddItemsCommand(m_scene, {frame}, QStringLiteral("add")));
        const QImage out = m_scene->renderImage();
        // Badge circle (diameter 26) centred on the top-right corner of the line.
        QCOMPARE(pixel(out, 139 + 9, 41 - 2), kRed); // outside the frame, inside the badge
        QCOMPARE(pixel(out, 40 - 2, 41 - 2), QColor(Qt::white)); // top-left corner: no badge
    }

    void dimDarkensOutsideFramesOnly()
    {
        auto *frame = new FrameItem(QRectF(50, 20, 100, 60), squareStyle(kRed, 2), 1.0);
        m_stack->push(new AddItemsCommand(m_scene, {frame}, QStringLiteral("add")));
        DimSettings dim;
        dim.enabled = true;
        dim.strength = 50;
        dim.blur = 0;
        m_stack->push(new SetDimCommand(m_scene, dim));

        const QImage out = m_scene->renderImage();
        QCOMPARE(pixel(out, 100, 50), QColor(Qt::white)); // inside the frame: untouched
        QCOMPARE(pixel(out, 51, 50), kRed);               // the frame line stays bright
        const QColor outside = pixel(out, 10, 10);
        QVERIFY(outside.value() < 150 && outside.value() > 110); // white under a 50 % veil

        m_stack->undo();
        QCOMPARE(pixel(m_scene->renderImage(), 10, 10), QColor(Qt::white));
    }

    void dimKeepsTransparentCorners()
    {
        QImage base(60, 40, QImage::Format_ARGB32);
        base.fill(Qt::white);
        base.setPixelColor(0, 0, Qt::transparent); // a cleaned window corner
        m_scene->setBaseImage(base, 1.0);
        DimSettings dim;
        dim.enabled = true;
        dim.blur = 3;
        m_stack->push(new SetDimCommand(m_scene, dim));
        QCOMPARE(m_scene->renderImage().pixelColor(0, 0).alpha(), 0);
    }

    void dimSliderMovesMergeButSwitchDoesNot()
    {
        DimSettings dim;
        dim.enabled = true;
        m_stack->push(new SetDimCommand(m_scene, dim));
        for (int s = 50; s <= 70; s += 5) {
            dim.strength = s;
            m_stack->push(new SetDimCommand(m_scene, dim));
        }
        QCOMPARE(m_stack->count(), 2); // switch on + one merged slider step
        m_stack->undo();
        QCOMPARE(m_scene->dimSettings().strength, 45);
        QVERIFY(m_scene->dimSettings().enabled);
    }

    void reorderChangesNumbersAndUndoes()
    {
        FrameStyle numbered = squareStyle(kRed, 3);
        numbered.badge = BadgePosition::TopRight;
        QList<FrameItem *> f;
        for (int i = 0; i < 3; ++i) {
            f << new FrameItem(QRectF(10 + 50 * i, 10, 40, 30), numbered, 1.0);
            m_stack->push(new AddItemsCommand(m_scene, {f.last()}, QStringLiteral("add")));
        }
        // Drag the third entry to the top: it becomes number 1.
        m_stack->push(new ReorderItemsCommand(m_scene, 2, 0));
        QCOMPARE(m_scene->numberOf(f[2]), 1);
        QCOMPARE(m_scene->numberOf(f[0]), 2);
        QCOMPARE(m_scene->numberOf(f[1]), 3);
        QCOMPARE(m_scene->frames(), (QList<FrameItem *>{f[2], f[0], f[1]}));

        m_stack->undo();
        QCOMPARE(m_scene->numberOf(f[0]), 1);
        QCOMPARE(m_scene->numberOf(f[2]), 3);
    }

    // A "secret": black and white stripes, one pixel wide, like text.
    void paintSecret(QImage &image, const QRect &area)
    {
        for (int y = area.top(); y <= area.bottom(); ++y)
            for (int x = area.left(); x <= area.right(); ++x)
                image.setPixelColor(x, y, (x % 2) ? Qt::black : Qt::white);
    }

    void pixelateHidesTheOriginal()
    {
        QImage base(200, 100, QImage::Format_ARGB32);
        base.fill(Qt::white);
        const QRect secret(40, 20, 60, 40);
        paintSecret(base, secret);
        m_scene->setBaseImage(base, 1.0);

        RedactStyle style; // pixelate, medium: 10 px blocks
        m_stack->push(new AddItemsCommand(m_scene, {new RedactItem(QRectF(secret), style, 1.0)}, QStringLiteral("add")));
        const QImage out = m_scene->renderImage();
        // No stripe survives: neighbouring pixels in a block are equal.
        for (int y = secret.top(); y <= secret.bottom(); ++y)
            for (int x = secret.left(); x < secret.right(); ++x)
                if (out.pixelColor(x, y) != out.pixelColor(x + 1, y) && (x - secret.left() + 1) % 10 != 0)
                    QFAIL(qPrintable(QStringLiteral("stripe left at %1,%2").arg(x).arg(y)));
        QCOMPARE(out.pixelColor(10, 10), QColor(Qt::white)); // outside untouched
        m_stack->undo();
        QCOMPARE(m_scene->renderImage().pixelColor(41, 30), QColor(Qt::black));
    }

    void blurHidesTheOriginal()
    {
        QImage base(200, 100, QImage::Format_ARGB32);
        base.fill(Qt::white);
        const QRect secret(40, 20, 60, 40);
        paintSecret(base, secret);
        m_scene->setBaseImage(base, 1.0);
        RedactStyle style;
        style.mode = RedactMode::Blur;
        m_stack->push(new AddItemsCommand(m_scene, {new RedactItem(QRectF(secret), style, 1.0)}, QStringLiteral("add")));
        const QImage out = m_scene->renderImage();
        // Stripes average out to grey: neighbours differ only a little.
        const int a = out.pixelColor(70, 40).red();
        const int b = out.pixelColor(71, 40).red();
        QVERIFY(std::abs(a - b) < 20);
        QVERIFY(a > 60 && a < 200);
    }

    void redactionSnapsToWholePixels()
    {
        auto *item = new RedactItem(QRectF(10.4, 10.6, 30.2, 20.7), RedactStyle(), 1.0);
        m_stack->push(new AddItemsCommand(m_scene, {item}, QStringLiteral("add")));
        QCOMPARE(item->rect(), QRectF(QPointF(10, 11), QPointF(41, 31)));
        item->setPos(3.6, 2.2);
        QCOMPARE(item->pos(), QPointF(4, 2));
    }

    void redactionIsDimmedOutsideFrames()
    {
        auto *item = new RedactItem(QRectF(10, 10, 40, 40), RedactStyle(), 1.0);
        m_stack->push(new AddItemsCommand(m_scene, {item}, QStringLiteral("add")));
        DimSettings dim;
        dim.enabled = true;
        dim.strength = 50;
        m_stack->push(new SetDimCommand(m_scene, dim));
        // White image pixelated is still white; dimmed it turns grey.
        const QColor c = m_scene->renderImage().pixelColor(30, 30);
        QVERIFY(c.value() < 150);
    }

    void redactStyleIsUndoableAndListed()
    {
        auto *frame = new FrameItem(QRectF(100, 20, 40, 30), squareStyle(kRed, 3), 1.0);
        auto *item = new RedactItem(QRectF(10, 10, 40, 40), RedactStyle(), 1.0);
        m_stack->push(new AddItemsCommand(m_scene, {frame}, QStringLiteral("add")));
        m_stack->push(new AddItemsCommand(m_scene, {item}, QStringLiteral("add")));
        QCOMPARE(m_scene->annotations().size(), 2);
        QCOMPARE(m_scene->annotations().last()->label(), QStringLiteral("Verpixelt"));
        QCOMPARE(m_scene->frames().size(), 1);

        RedactStyle blur;
        blur.mode = RedactMode::Blur;
        m_stack->push(new SetRedactStyleCommand({item}, blur));
        QCOMPARE(item->label(), QStringLiteral("Weichgezeichnet"));
        m_stack->undo();
        QCOMPARE(item->style().mode, RedactMode::Pixelate);
    }

    // White image with "Hxg" in dark grey, 40 px font, baseline at y = 60.
    QImage textImage()
    {
        QImage image(300, 100, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter p(&image);
        QFont font = p.font();
        font.setPixelSize(40);
        p.setFont(font);
        p.setPen(QColor(40, 40, 40));
        p.drawText(QPointF(30, 60), QStringLiteral("Hxg Hxg"));
        return image;
    }

    void textSampleMatchesTheOriginal()
    {
        const TextSample sample = TextSample::analyze(textImage(), QRect(10, 10, 280, 80));
        QCOMPARE(sample.background, QColor(Qt::white));
        QVERIFY(sample.text.lightness() < 80);
        QVERIFY2(std::abs(sample.baseline - 60) <= 2, qPrintable(QStringLiteral("baseline %1 capTop %2 size %3 ink %4-%5").arg(sample.baseline).arg(sample.capTop).arg(sample.fontPixelSize).arg(sample.ink.top()).arg(sample.ink.bottom())));
        QVERIFY(sample.fontPixelSize >= 32 && sample.fontPixelSize <= 48); // 40 ± 20 %
        QVERIFY(std::abs(sample.ink.left() - 30) <= 4);
        QVERIFY(!sample.touchesEdge);
        QVERIFY(TextSample::analyze(textImage(), QRect(10, 10, 280, 52)).touchesEdge); // cuts the "g"
    }

    void replacedTextRemovesTheOriginal()
    {
        m_scene->setBaseImage(textImage(), 1.0);
        ReplaceStyle style;
        style.text = QString(); // nothing new: only the background remains
        auto *item = new ReplaceItem(QRectF(10, 10, 280, 80), style, 1.0);
        m_scene->addItem(item);
        QCOMPARE(item->matchOriginal(), ReplaceItem::Match::Ok);
        m_scene->removeItem(item);
        m_stack->push(new AddItemsCommand(m_scene, {item}, QStringLiteral("add")));
        const QImage out = m_scene->renderImage();
        for (int y = 10; y < 90; ++y)
            for (int x = 10; x < 290; ++x)
                if (out.pixelColor(x, y) != QColor(Qt::white))
                    QFAIL(qPrintable(QStringLiteral("original pixel left at %1,%2").arg(x).arg(y)));

        // Typing merges into one undo step; undo restores the old text.
        const int steps = m_stack->count();
        ReplaceStyle typed = item->style();
        for (const QString &text : {QStringLiteral("M"), QStringLiteral("Ma"), QStringLiteral("Max")}) {
            typed.text = text;
            m_stack->push(new SetReplaceStyleCommand(item, typed));
        }
        QCOMPARE(m_stack->count(), steps + 1);
        QCOMPARE(item->label(), QStringLiteral("Text: Max"));
        m_stack->undo();
        QCOMPARE(item->style().text, QString());
    }

    void busyBackgroundIsNotTakenForText()
    {
        // A "photo": random colours everywhere.
        QImage image(300, 120, QImage::Format_ARGB32);
        quint32 seed = 1;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                seed = seed * 1664525u + 1013904223u;
                image.setPixel(x, y, 0xff000000u | (seed >> 8));
            }
        QVERIFY(!TextSample::analyze(image, QRect(20, 20, 200, 60)).reliable);
        QVERIFY(TextSample::analyze(textImage(), QRect(10, 10, 280, 80)).reliable);

        m_scene->setBaseImage(image, 1.0);
        ReplaceStyle style;
        style.text = QStringLiteral("Firstname Lastname");
        auto *item = new ReplaceItem(QRectF(20, 40, 200, 40), style, 1.0);
        m_scene->addItem(item);
        QCOMPARE(item->matchOriginal(), ReplaceItem::Match::NoPlainBackground);
        // Without a usable original the text is centred in the area.
        QCOMPARE(item->style().textBaseline, 0.0);
        m_scene->removeItem(item);
        delete item;
    }

    void textIsClampedIntoTheArea()
    {
        ReplaceStyle style;
        style.text = QStringLiteral("Hxg");
        style.fontSize = 14;
        style.textBaseline = 500; // far below the area
        style.textLeft = -50;
        auto *item = new ReplaceItem(QRectF(20, 20, 200, 40), style, 1.0);
        m_stack->push(new AddItemsCommand(m_scene, {item}, QStringLiteral("add")));
        const QImage out = m_scene->renderImage();
        // Some text pixels inside the area, none below it.
        bool inside = false;
        for (int y = 20; y < 60; ++y)
            for (int x = 20; x < out.width(); ++x) // the area reaches beyond the image
                inside |= out.pixelColor(x, y).lightness() < 128;
        QVERIFY(inside);
        for (int x = 0; x < 200; ++x)
            QCOMPARE(out.pixelColor(x, 75), QColor(Qt::white));
    }

    void cropExportsOnlyThePart()
    {
        // Left half red, right half blue, so the crop position is visible.
        QImage base(200, 100, QImage::Format_ARGB32);
        base.fill(kBlue);
        for (int y = 0; y < 100; ++y)
            for (int x = 0; x < 100; ++x)
                base.setPixelColor(x, y, kRed);
        m_scene->setBaseImage(base, 2.0);
        m_stack->push(new SetCropCommand(m_scene, QRect(90, 10, 40, 30)));

        QVERIFY(m_scene->isCropped());
        QCOMPARE(m_scene->sceneRect(), QRectF(90, 10, 40, 30));
        const QImage out = m_scene->renderImage();
        QCOMPARE(out.size(), QSize(40, 30));
        QCOMPARE(pixel(out, 9, 15), kRed);   // image x 99
        QCOMPARE(pixel(out, 10, 15), kBlue); // image x 100: no blurred edge
        QCOMPARE(pixel(out, 0, 0), kRed);
        QCOMPARE(pixel(out, 39, 29), kBlue);
        QCOMPARE(EditorScene::scaleFromMetadata(out), 2.0);
        QCOMPARE(m_scene->baseImage().size(), QSize(200, 100)); // the original stays

        m_stack->undo();
        QVERIFY(!m_scene->isCropped());
        QCOMPARE(m_scene->renderImage().size(), QSize(200, 100));
    }

    void cropHidesAnnotationsOutsideAndUndoRestoresThem()
    {
        FrameStyle numbered = squareStyle(kRed, 2);
        numbered.badge = BadgePosition::TopRight;
        auto *outside = new FrameItem(QRectF(10, 10, 30, 30), numbered, 1.0);
        auto *inside = new FrameItem(QRectF(120, 40, 40, 30), numbered, 1.0);
        auto *partly = new RedactItem(QRect(90, 10, 30, 20), RedactStyle(), 1.0);
        m_stack->push(new AddItemsCommand(m_scene, {outside, inside, partly}, QStringLiteral("add")));
        QCOMPARE(m_scene->numberOf(inside), 2);

        m_stack->push(new SetCropCommand(m_scene, QRect(100, 0, 100, 100)));
        QVERIFY(!outside->isVisible());
        QVERIFY(inside->isVisible());
        QVERIFY(partly->isVisible()); // reaches into the crop
        QCOMPARE(m_scene->annotations(), (QList<RectItem *>{inside, partly}));
        QCOMPARE(m_scene->numberOf(inside), 1); // the visible numbers start at 1
        // The export does not contain the hidden frame's line.
        const QImage out = m_scene->renderImage();
        QCOMPARE(out.size(), QSize(100, 100));

        m_stack->undo();
        QVERIFY(outside->isVisible());
        QCOMPARE(m_scene->annotations().size(), 3);
        QCOMPARE(m_scene->numberOf(inside), 2);
        QCOMPARE(pixel(m_scene->renderImage(), 11, 25), kRed);
    }

    void annotationsStayInsideTheCrop()
    {
        m_stack->push(new SetCropCommand(m_scene, QRect(50, 20, 100, 60)));
        auto *frame = new FrameItem(QRectF(60, 30, 20, 20), squareStyle(kRed, 2), 1.0);
        m_stack->push(new AddItemsCommand(m_scene, {frame}, QStringLiteral("add")));
        frame->setPos(-500, -500);
        QVERIFY(m_scene->sceneRect().contains(frame->mapRectToScene(frame->rect())));
    }

    void newImageResetsTheCrop()
    {
        m_stack->push(new SetCropCommand(m_scene, QRect(10, 10, 20, 20)));
        m_stack->clear();
        QImage base(300, 50, QImage::Format_ARGB32);
        base.fill(Qt::white);
        m_scene->setBaseImage(base, 1.0);
        QVERIFY(!m_scene->isCropped());
        QCOMPARE(m_scene->cropRect(), QRect(0, 0, 300, 50));
    }

private:
    QUndoStack *m_stack = nullptr;
    EditorScene *m_scene = nullptr;
};

QTEST_MAIN(TestEditor)
#include "tst_editor.moc"
