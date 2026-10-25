// Tests for zooming in the editor view (D-017).

#include "editor/EditorScene.h"
#include "editor/EditorView.h"
#include "editor/FrameItem.h"

#include <QTest>
#include <QUndoStack>

namespace {

void drag(QWidget *w, QPoint from, QPoint to)
{
    QTest::mousePress(w, Qt::LeftButton, {}, from);
    for (int i = 1; i <= 6; ++i)
        QTest::mouseMove(w, from + (to - from) * i / 6);
    QTest::mouseRelease(w, Qt::LeftButton, {}, to);
}

} // namespace

class TestView : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_stack = new QUndoStack(this);
        m_view = new EditorView(m_stack);
        m_view->setFrameShape(QFrame::NoFrame);
        m_view->resize(800, 600);
        m_view->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_view));
        // A scale-2 capture: 1600 × 1200 image pixels = 800 × 600 logical.
        QImage image(1600, 1200, QImage::Format_ARGB32);
        image.fill(Qt::white);
        m_view->setImage(image, 2.0);
    }

    void cleanup()
    {
        delete m_stack;
        delete m_view;
    }

    void newImageFitsTheWindow()
    {
        QVERIFY(m_view->isFitMode());
        // 800 × 600 logical into 800 × 600 minus margins: a bit below 100 %.
        QVERIFY(m_view->zoom() > 0.85 && m_view->zoom() < 1.0);
    }

    void fitGrowsWithTheWindow()
    {
        m_view->resize(1700, 1300);
        QTest::qWait(20);
        QVERIFY(m_view->isFitMode());
        QVERIFY(m_view->zoom() > 2.0); // larger than 100 %: details get readable
        m_view->resize(400, 300);
        QTest::qWait(20);
        QVERIFY(m_view->zoom() < 0.5);
    }

    void manualZoomLeavesFitMode()
    {
        m_view->zoomIn();
        QVERIFY(!m_view->isFitMode());
        QCOMPARE(m_view->zoom(), 1.0); // next step above ~0.92
        m_view->zoomIn();
        QCOMPARE(m_view->zoom(), 1.25);
        m_view->resize(1200, 900);
        QTest::qWait(20);
        QCOMPARE(m_view->zoom(), 1.25); // not following the window any more
        m_view->fitToWindow();
        QVERIFY(m_view->isFitMode());
        QVERIFY(m_view->zoom() > 1.25);
    }

    void zoomIsClamped()
    {
        m_view->setZoom(100.0);
        QCOMPARE(m_view->zoom(), EditorView::kMaxZoom);
        m_view->setZoom(0.0001);
        QCOMPARE(m_view->zoom(), EditorView::kMinZoom);
    }

    void highZoomShowsCrispPixels()
    {
        m_view->setZoom(4.0);
        QVERIFY(!m_view->editorScene()->smoothBase());
        m_view->setZoom(0.5);
        QVERIFY(m_view->editorScene()->smoothBase());
    }

    void framesStayInsideTheImage()
    {
        m_view->setZoom(0.5); // image is 400 × 300 on screen, centred with space around
        m_view->setTool(ToolId::Frame);
        QWidget *vp = m_view->viewport();
        const QPoint topLeft = m_view->mapFromScene(QPointF(0, 0));
        const QPoint inside = m_view->mapFromScene(QPointF(800, 600));

        // A press outside the image draws nothing.
        drag(vp, topLeft - QPoint(30, 30), inside);
        QCOMPARE(m_view->editorScene()->frames().size(), 0);

        // A drag that ends outside is cut at the image edge.
        drag(vp, inside, topLeft - QPoint(60, 60));
        QCOMPARE(m_view->editorScene()->frames().size(), 1);
        FrameItem *frame = m_view->editorScene()->frames().first();
        const QRectF bounds = m_view->editorScene()->sceneRect();
        QVERIFY(bounds.contains(frame->mapRectToScene(frame->rect())));

        // Moving it beyond the edge stops at the edge.
        frame->setPos(-5000, -5000);
        QVERIFY(bounds.contains(frame->mapRectToScene(frame->rect())));
        frame->setPos(5000, 5000);
        QVERIFY(bounds.contains(frame->mapRectToScene(frame->rect())));
    }

    void numberMovesToAVisibleCorner()
    {
        FrameStyle style;
        style.badge = BadgePosition::TopRight;
        // Image is 1600 × 1200; badge diameter 26 × scale 2 = 52.
        auto *frame = new FrameItem(QRectF(400, 400, 300, 200), style, 2.0);
        m_view->editorScene()->addItem(frame);
        QCOMPARE(frame->effectiveBadge(), BadgePosition::TopRight);
        QCOMPARE(frame->fittingBadgePositions().size(), 4);

        // At the top edge: the number moves down to the bottom-right corner.
        frame->setRect(QRectF(400, 0, 300, 200));
        QCOMPARE(frame->effectiveBadge(), BadgePosition::BottomRight);
        QCOMPARE(frame->fittingBadgePositions(),
                 (QList<BadgePosition>{BadgePosition::BottomRight, BadgePosition::BottomLeft}));

        // In the top-right corner of the image: only bottom-left is left.
        frame->setRect(QRectF(1300, 0, 300, 200));
        QCOMPARE(frame->effectiveBadge(), BadgePosition::BottomLeft);
        QCOMPARE(frame->fittingBadgePositions(), QList<BadgePosition>{BadgePosition::BottomLeft});

        // Back in the middle, the chosen corner returns.
        frame->setRect(QRectF(400, 400, 300, 200));
        QCOMPARE(frame->effectiveBadge(), BadgePosition::TopRight);

        // As large as the image: no corner fits; the circle stays on the image.
        frame->setRect(QRectF(0, 0, 1600, 1200));
        QVERIFY(frame->fittingBadgePositions().isEmpty());
        const QRectF badge = frame->mapRectToScene(frame->shape().boundingRect());
        QVERIFY(badge.isValid());
        delete frame;
    }

    void cropToolDrawsAndEnterConfirms()
    {
        m_view->setZoom(0.5); // 1600 × 1200 image px = 400 × 300 on screen
        m_view->setTool(ToolId::Crop);
        QWidget *vp = m_view->viewport();
        QCOMPARE(m_view->cropDraft(), QRect(0, 0, 1600, 1200));

        // A drag from outside the image is cut at the image edge.
        drag(vp, m_view->mapFromScene(QPointF(0, 0)) - QPoint(40, 40), m_view->mapFromScene(QPointF(800, 600)));
        QCOMPARE(m_view->cropDraft(), QRect(0, 0, 800, 600));
        QVERIFY(!m_view->editorScene()->isCropped()); // only a draft so far
        QCOMPARE(m_stack->count(), 0);

        QTest::keyClick(m_view, Qt::Key_Return);
        QCOMPARE(m_view->editorScene()->cropRect(), QRect(0, 0, 800, 600));
        QCOMPARE(m_stack->count(), 1);
        m_stack->undo();
        QCOMPARE(m_view->cropDraft(), QRect(0, 0, 1600, 1200)); // the draft follows undo
    }

    void cropDraftMovesAndResizes()
    {
        m_view->setZoom(0.5);
        m_view->setTool(ToolId::Crop);
        m_view->setCropDraft(QRect(200, 200, 400, 400));
        QWidget *vp = m_view->viewport();

        // Inside: moves; it cannot leave the image.
        drag(vp, m_view->mapFromScene(QPointF(400, 400)), m_view->mapFromScene(QPointF(-1000, 400)));
        QCOMPARE(m_view->cropDraft(), QRect(0, 200, 400, 400));

        // The bottom-right handle resizes, to whole pixels.
        drag(vp, m_view->mapFromScene(QPointF(400, 600)), m_view->mapFromScene(QPointF(1000, 900)));
        QCOMPARE(m_view->cropDraft(), QRect(0, 200, 1000, 700));

        // A click outside keeps the draft.
        const QPoint outside = m_view->mapFromScene(QPointF(1400, 100));
        drag(vp, outside, outside + QPoint(1, 1));
        QCOMPARE(m_view->cropDraft(), QRect(0, 200, 1000, 700));
    }

    void cancelAndLeavingTheCropTool()
    {
        m_view->setTool(ToolId::Crop);
        m_view->setCropDraft(QRect(100, 100, 300, 200));
        m_view->cancelCrop();
        QCOMPARE(m_view->cropDraft(), QRect(0, 0, 1600, 1200));

        // Leaving the tool keeps the draft, and the fit follows the crop.
        m_view->setCropDraft(QRect(100, 100, 400, 300));
        m_view->setTool(ToolId::Select);
        QCOMPARE(m_view->editorScene()->cropRect(), QRect(100, 100, 400, 300));
        QCOMPARE(m_view->sceneRect(), QRectF(100, 100, 400, 300));
        QVERIFY(m_view->isFitMode());
        QVERIFY(m_view->zoom() > 3.0); // 200 × 150 logical px fill 800 × 600

        // Back in the tool, the whole image is shown again.
        m_view->setTool(ToolId::Crop);
        QCOMPARE(m_view->sceneRect(), QRectF(0, 0, 1600, 1200));
        QCOMPARE(m_view->cropDraft(), QRect(100, 100, 400, 300));
    }

private:
    QUndoStack *m_stack = nullptr;
    EditorView *m_view = nullptr;
};

QTEST_MAIN(TestView)
#include "tst_view.moc"
