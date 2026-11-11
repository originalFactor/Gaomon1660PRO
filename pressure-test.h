#pragma once
#include <QPainter>
#include <QTabletEvent>
#include <QWidget>
#include <vector>
#include <algorithm>

class TestPad : public QWidget {
    struct Segment {
        QPointF a, b;
        qreal pressure;
    };
    std::vector<Segment> strokes;
    QPointF previous;
    qreal pressure = 0;
    bool drawing = false;

public:
    explicit TestPad(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumHeight(90);
        setAutoFillBackground(true);
        setAccessibleName(QStringLiteral("压感测试区"));
    }
    void clear() {
        strokes.clear();
        pressure = 0;
        drawing = false;
        update();
    }

protected:
    void tabletEvent(QTabletEvent *e) override {
        pressure = std::clamp(e->pressure(), qreal(0), qreal(1));
        if (e->type() == QEvent::TabletPress) {
            drawing = true;
            previous = e->position();
            strokes.push_back({previous, previous, pressure});
        } else if (e->type() == QEvent::TabletMove && drawing) {
            strokes.push_back({previous, e->position(), pressure});
            previous = e->position();
        } else if (e->type() == QEvent::TabletRelease)
            drawing = false;
        if (strokes.size() > 30000)
            strokes.erase(strokes.begin(), strokes.begin() + 10000);
        update();
        e->accept();
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), palette().base());
        for (const auto &s : strokes) {
            p.setPen(QPen(palette().highlight().color(), 1 + 12 * s.pressure, Qt::SolidLine,
                          Qt::RoundCap, Qt::RoundJoin));
            p.drawLine(s.a, s.b);
        }
        p.setPen(palette().text().color());
        p.drawText(
            10, 20,
            QStringLiteral("在此用笔书写 · 应用收到的压力：%1%").arg(qRound(pressure * 100)));
        p.setPen(palette().mid().color());
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }
};
