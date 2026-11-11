#pragma once
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <optional>

struct PressurePoints {
    QPointF first{0, 0}, second{1, 1};
};
inline QString serializePressure(const PressurePoints &p) {
    return QStringLiteral("%1,%2;%3,%4;")
        .arg(QString::number(p.first.x(), 'g', 12), QString::number(p.first.y(), 'g', 12),
             QString::number(p.second.x(), 'g', 12), QString::number(p.second.y(), 'g', 12));
}
inline std::optional<PressurePoints> parsePressure(const QString &text) {
    const auto pairs = text.split(';', Qt::SkipEmptyParts);
    if (pairs.size() != 2)
        return std::nullopt;
    QPointF points[2];
    for (int i = 0; i < 2; i++) {
        const auto values = pairs[i].split(',');
        if (values.size() != 2)
            return std::nullopt;
        bool xok = false, yok = false;
        const double x = values[0].toDouble(&xok), y = values[1].toDouble(&yok);
        if (!xok || !yok || !std::isfinite(x) || !std::isfinite(y) || x < 0 || x > 1 || y < 0 ||
            y > 1)
            return std::nullopt;
        points[i] = QPointF(x, y);
    }
    return PressurePoints{points[0], points[1]};
}

class PressureCurve : public QWidget {
    Q_OBJECT
    PressurePoints points;
    int active = -1, focused = 0;
    QRectF graph() const {
        auto available = rect().adjusted(35, 15, -18, -30);
        double side = std::max(1, std::min(available.width(), available.height()));
        return {available.center().x() - side / 2, available.center().y() - side / 2, side, side};
    }
    QPointF position(const QPointF &v) const {
        const auto r = graph();
        return {r.left() + v.x() * r.width(), r.bottom() - v.y() * r.height()};
    }
    QPointF normalized(const QPointF &v) const {
        const auto r = graph();
        return {std::clamp((v.x() - r.left()) / r.width(), 0.0, 1.0),
                std::clamp((r.bottom() - v.y()) / r.height(), 0.0, 1.0)};
    }
    void changePoint(int index, const QPointF &v) {
        if (index == 0)
            points.first = v;
        else
            points.second = v;
        update();
        emit pointsChanged();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        auto r = graph();
        p.fillRect(r, palette().base());
        QColor grid = palette().text().color();
        grid.setAlpha(35);
        p.setPen(QPen(grid, 1));
        for (int i = 0; i <= 4; i++) {
            double t = i / 4.0;
            p.drawLine(QPointF(r.left() + t * r.width(), r.top()),
                       QPointF(r.left() + t * r.width(), r.bottom()));
            p.drawLine(QPointF(r.left(), r.top() + t * r.height()),
                       QPointF(r.right(), r.top() + t * r.height()));
        }
        p.setPen(palette().text().color());
        p.drawText(QRectF(0, r.top() - 5, 32, 20), Qt::AlignRight, QStringLiteral("100"));
        p.drawText(QRectF(0, r.bottom() - 14, 32, 20), Qt::AlignRight, QStringLiteral("0"));
        p.drawText(QRectF(r.left(), r.bottom() + 6, r.width(), 20), Qt::AlignCenter,
                   QStringLiteral("笔尖力度 →"));
        QPainterPath curve(position({0, 0}));
        curve.cubicTo(position(points.first), position(points.second), position({1, 1}));
        QColor accent = palette().highlight().color();
        p.setPen(QPen(accent, 2.5));
        p.setBrush(Qt::NoBrush);
        p.drawPath(curve);
        QColor guide = palette().text().color();
        guide.setAlpha(120);
        p.setPen(QPen(guide, 1, Qt::DashLine));
        p.drawLine(position({0, 0}), position(points.first));
        p.drawLine(position({1, 1}), position(points.second));
        for (int i = 0; i < 2; i++) {
            auto pos = position(i == 0 ? points.first : points.second);
            p.setPen(QPen(accent, 2));
            p.setBrush(i == focused && hasFocus() ? accent : palette().base().color());
            p.drawEllipse(pos, 6, 6);
        }
    }
    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() != Qt::LeftButton)
            return;
        double d0 = QLineF(e->position(), position(points.first)).length(),
               d1 = QLineF(e->position(), position(points.second)).length();
        if (std::min(d0, d1) > 18)
            return;
        active = qAbs(d0 - d1) < 0.01 ? focused : (d0 < d1 ? 0 : 1);
        focused = active;
        setFocus(Qt::MouseFocusReason);
        e->accept();
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if (active < 0)
            return;
        changePoint(active, normalized(e->position()));
        e->accept();
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton && active >= 0) {
            active = -1;
            e->accept();
        }
    }
    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_1 || e->key() == Qt::Key_2) {
            focused = e->key() == Qt::Key_1 ? 0 : 1;
            update();
            e->accept();
            return;
        }
        QPointF v = focused == 0 ? points.first : points.second;
        const double step = (e->modifiers() & Qt::ShiftModifier) ? 0.1 : 0.01;
        switch (e->key()) {
        case Qt::Key_Left:
            v.rx() -= step;
            break;
        case Qt::Key_Right:
            v.rx() += step;
            break;
        case Qt::Key_Up:
            v.ry() += step;
            break;
        case Qt::Key_Down:
            v.ry() -= step;
            break;
        default:
            QWidget::keyPressEvent(e);
            return;
        }
        changePoint(focused, {std::clamp(v.x(), 0.0, 1.0), std::clamp(v.y(), 0.0, 1.0)});
        e->accept();
    }

public:
    explicit PressureCurve(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumSize(230, 230);
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(QStringLiteral("压感曲线控制点"));
        setToolTip(QStringLiteral("拖动控制点；也可聚焦后按方向键微调，1、2 切换控制点。"));
    }
    PressurePoints value() const {
        return points;
    }
    void setValue(const PressurePoints &p) {
        points = p;
        update();
    }
    QRectF graphRect() const {
        return graph();
    }
signals:
    void pointsChanged();
};
