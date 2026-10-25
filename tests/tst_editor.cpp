// Tests for the editor document (D-012): frames, undo/redo and export.

#include "editor/Commands.h"
#include "editor/EditorScene.h"
#include "editor/FrameItem.h"

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
        m_stack->push(new SetFrameRectCommand(frame, QRectF(20, 20, 50, 30), QRectF(20, 20, 80, 40)));
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

private:
    QUndoStack *m_stack = nullptr;
    EditorScene *m_scene = nullptr;
};

QTEST_MAIN(TestEditor)
#include "tst_editor.moc"
