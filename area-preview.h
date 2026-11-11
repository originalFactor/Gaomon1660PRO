#pragma once
#include "soft-area.h"
#include <QMouseEvent>
#include <QPainter>
#include <QWidget>
#include <algorithm>

// Selection coordinates are normalized to the full input or output area.
class AreaPreview : public QWidget {
    Q_OBJECT
    QRectF selection{0, 0, 1, 1}, startSelection;
    QSizeF reference{254, 158.8};
    QList<QRectF> monitors;
    bool tablet;
    bool reserveSoft = false;
    int edges = 0; // 1 left, 2 right, 4 top, 8 bottom, 16 move
    QPointF origin;
    static constexpr double minSize = 0.01;
    QRectF canvas() const {
        QRectF available =
            rect().adjusted(tablet ? 36 : 12, tablet ? 46 : 14, -12, tablet ? -26 : -14);
        if (available.width() < 1 || available.height() < 1)
            return {};
        const double ratio = reference.width() / reference.height();
        QSizeF size = available.size();
        if (size.width() / size.height() > ratio)
            size.setWidth(size.height() * ratio);
        else
            size.setHeight(size.width() / ratio);
        return QRectF(QPointF(available.center().x() - size.width() / 2,
                              available.center().y() - size.height() / 2),
                      size);
    }
    QRectF pixels() const {
        auto c = canvas();
        return {c.left() + selection.x() * c.width(), c.top() + selection.y() * c.height(),
                selection.width() * c.width(), selection.height() * c.height()};
    }
    int hit(const QPointF &pos) const {
        const QRectF r = pixels();
        const double tolerance = 9;
        const bool alongX = pos.x() >= r.left() - tolerance && pos.x() <= r.right() + tolerance;
        const bool alongY = pos.y() >= r.top() - tolerance && pos.y() <= r.bottom() + tolerance;
        int h = 0;
        if (alongY && qAbs(pos.x() - r.left()) <= tolerance)
            h |= 1;
        else if (alongY && qAbs(pos.x() - r.right()) <= tolerance)
            h |= 2;
        if (alongX && qAbs(pos.y() - r.top()) <= tolerance)
            h |= 4;
        else if (alongX && qAbs(pos.y() - r.bottom()) <= tolerance)
            h |= 8;
        if (!h && r.contains(pos))
            h = 16;
        return h;
    }
    void updateCursor(int h) {
        if (!isEnabled()) {
            unsetCursor();
            return;
        }
        if (h == 16)
            setCursor(Qt::SizeAllCursor);
        else if (h == 1 || h == 2)
            setCursor(Qt::SizeHorCursor);
        else if (h == 4 || h == 8)
            setCursor(Qt::SizeVerCursor);
        else if (h == (1 | 4) || h == (2 | 8))
            setCursor(Qt::SizeFDiagCursor);
        else if (h == (2 | 4) || h == (1 | 8))
            setCursor(Qt::SizeBDiagCursor);
        else
            unsetCursor();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto c = canvas();
        if (c.isEmpty())
            return;
        const QColor base = palette().color(QPalette::Base),
                     accent = palette().color(QPalette::Highlight);
        QColor outline = palette().color(QPalette::Text);
        outline.setAlpha(145);
        QColor neutral = palette().color(QPalette::Text);
        neutral.setAlpha(24);
        QColor buttonFill = palette().color(QPalette::Text);
        buttonFill.setAlpha(45);
        if (tablet) {
            QRectF body = c.adjusted(-30, -7, 7, 7);
            p.setPen(outline);
            p.setBrush(palette().color(QPalette::AlternateBase));
            p.drawRoundedRect(body, 7, 7);
            p.setBrush(buttonFill);
            for (int i = 0; i < 6; i++)
                for (int j = 0; j < 2; j++)
                    p.drawRoundedRect(QRectF(c.left() - 25 + j * 10,
                                             c.top() + 8 + i * (c.height() - 16) / 6, 7, 7),
                                      1, 1);
        }
        p.fillRect(c, base);
        p.fillRect(c, neutral);
        if (tablet) {
            const QColor softColor(210, 145, 40, 150);
            const QRectF soft(c.left(), c.top(), c.width(), c.height() * softAreaHeight);
            p.fillRect(soft, softColor);
            p.setPen(QPen(outline, 0.8));
            for (int i = 1; i < 16; i++) {
                double x = c.left() + c.width() * i / 16;
                p.drawLine(QPointF(x, soft.top()), QPointF(x, soft.bottom()));
            }
            // The inset magnifies the physically narrow strip; the strip on the
            // tablet itself retains its true proportion for mapping.
            const QRectF inset(c.left(), c.top() - 24, c.width(), 14);
            p.setPen(palette().color(QPalette::Text));
            p.setBrush(Qt::NoBrush);
            p.drawText(QRectF(c.left(), c.top() - 42, c.width(), 15),
                       Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("软快捷键区（放大）"));
            p.fillRect(inset, softColor);
            p.setPen(QPen(outline, 1));
            p.drawRect(inset);
            QFont small = p.font();
            small.setPixelSize(9);
            p.setFont(small);
            for (int i = 0; i < 16; i++) {
                QRectF key(inset.left() + i * inset.width() / 16, inset.top(), inset.width() / 16,
                           inset.height());
                if (i)
                    p.drawLine(key.topLeft(), key.bottomLeft());
                p.setPen(palette().color(QPalette::Text));
                p.drawText(key, Qt::AlignCenter, QString::number(i + 1));
                p.setPen(QPen(outline, 1));
            }
        } else {
            p.setBrush(Qt::NoBrush);
            for (const auto &m : monitors) {
                QRectF r(c.left() + m.x() * c.width(), c.top() + m.y() * c.height(),
                         m.width() * c.width(), m.height() * c.height());
                p.setPen(QPen(outline, 1));
                p.drawRect(r);
            }
        }
        const QRectF r = pixels();
        QColor fill = accent;
        fill.setAlpha(isEnabled() ? 55 : 25);
        p.fillRect(r, fill);
        // Keep the full-area frame visible even when the selected area is full size.
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(outline, 1.5));
        p.drawRect(c.adjusted(-3, -3, 3, 3));
        p.setPen(QPen(accent, isEnabled() ? 2 : 1, Qt::SolidLine));
        p.drawRect(r);
        p.setBrush(base);
        if (isEnabled())
            for (const auto &pt : {r.topLeft(), QPointF(r.center().x(), r.top()), r.topRight(),
                                   QPointF(r.right(), r.center().y()), r.bottomRight(),
                                   QPointF(r.center().x(), r.bottom()), r.bottomLeft(),
                                   QPointF(r.left(), r.center().y())})
                p.drawRect(QRectF(pt.x() - 3.5, pt.y() - 3.5, 7, 7));
    }
    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() != Qt::LeftButton)
            return;
        edges = hit(e->position());
        if (!edges)
            return;
        origin = e->position();
        startSelection = selection;
        updateCursor(edges);
        e->accept();
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if (!edges) {
            updateCursor(hit(e->position()));
            return;
        }
        const auto c = canvas();
        if (c.isEmpty())
            return;
        const double dx = (e->position().x() - origin.x()) / c.width(),
                     dy = (e->position().y() - origin.y()) / c.height();
        double l = startSelection.left(), t = startSelection.top(), r = startSelection.right(),
               b = startSelection.bottom();
        const double minY = tablet && reserveSoft ? softAreaHeight : 0.0;
        if (edges == 16) {
            l = std::clamp(l + dx, 0.0, 1.0 - startSelection.width());
            t = std::clamp(t + dy, minY, std::max(minY, 1.0 - startSelection.height()));
            r = l + startSelection.width();
            b = t + startSelection.height();
        } else {
            if (edges & 1)
                l = std::clamp(l + dx, 0.0, std::max(0.0, r - minSize));
            if (edges & 2)
                r = std::clamp(r + dx, std::min(1.0, l + minSize), 1.0);
            if (edges & 4)
                t = std::clamp(t + dy, minY, std::max(minY, b - minSize));
            if (edges & 8)
                b = std::clamp(b + dy, std::min(1.0, t + minSize), 1.0);
        }
        selection = QRectF(l, t, r - l, b - t);
        update();
        emit areaChanged(selection);
        e->accept();
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton && edges) {
            edges = 0;
            updateCursor(hit(e->position()));
            e->accept();
        }
    }
    void leaveEvent(QEvent *) override {
        if (!edges)
            unsetCursor();
    }

public:
    explicit AreaPreview(bool isTablet, QWidget *parent = nullptr)
        : QWidget(parent), tablet(isTablet) {
        setMinimumSize(210, isTablet ? 190 : 150);
        setMouseTracking(true);
        setAccessibleName(isTablet ? QStringLiteral("数位板区域预览")
                                   : QStringLiteral("屏幕区域预览"));
    }
    void setReserveSoft(bool enabled) {
        reserveSoft = enabled;
        update();
    }
    void setArea(const QRectF &r) {
        selection = r;
        update();
    }
    QRectF area() const {
        return selection;
    }
    void setReference(const QSizeF &size, const QList<QRectF> &layouts = {}) {
        if (size.width() > 0 && size.height() > 0)
            reference = size;
        monitors = layouts;
        update();
    }
    QRectF canvasRect() const {
        return canvas();
    }
signals:
    void areaChanged(const QRectF &area);
};
