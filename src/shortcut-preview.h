#pragma once
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QWidget>
#include <array>
#include <functional>

// Hotspots use the same 750 x 500 tablet layout as the vendor preview.
class ShortcutPreview : public QWidget {
    QPixmap tablet{QStringLiteral(":/assets/shortcut-preview/tablet.png")};
    QPixmap pen{QStringLiteral(":/assets/shortcut-preview/pen.png")};
    std::array<QPushButton *, 30> keys{};
    QRectF tabletRect, penRect;
    bool softVisible = false;

    QRect scaled(const QRectF &area, const QRectF &key, double width, double height) const {
        return QRectF(area.x() + key.x() * area.width() / width,
                      area.y() + key.y() * area.height() / height,
                      key.width() * area.width() / width,
                      key.height() * area.height() / height).toAlignedRect();
    }
    void arrange() {
        const double scale = qMin((width() - 110.0) / 750.0, (height() - 36.0) / 500.0);
        tabletRect = QRectF(8, (height() - 500 * scale - 28) / 2, 750 * scale, 500 * scale);
        penRect = QRectF(tabletRect.right() + 38, tabletRect.top(), 43 * scale, 500 * scale);
        for (int i = 0; i < 12; i++) {
            const int row = i % 6;
            const int y = 88 + row * 50 + (row >= 3 ? 48 : 0);
            keys[i]->setGeometry(scaled(tabletRect, QRectF(30 + (i / 6) * 37, y, 36, 27), 750, 500));
        }
        for (int i = 0; i < 16; i++) {
            keys[12 + i]->setGeometry(scaled(tabletRect, QRectF(148 + i * 33.4, 77, 33.4, 29), 750, 500));
            keys[12 + i]->setVisible(softVisible);
        }
        keys[28]->setGeometry(scaled(penRect, QRectF(5, 355, 33, 41), 43, 500));
        keys[29]->setGeometry(scaled(penRect, QRectF(5, 314, 33, 41), 43, 500));
        update();
    }
protected:
    void resizeEvent(QResizeEvent *) override { arrange(); }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawPixmap(tabletRect, tablet, tablet.rect());
        p.drawPixmap(penRect, pen, pen.rect());
        p.drawText(QRectF(tabletRect.left(), tabletRect.bottom() + 4, tabletRect.width(), 24),
                   Qt::AlignCenter, QStringLiteral("1060PRO · 点击按键查看快捷键"));
    }
public:
    explicit ShortcutPreview(std::function<void(int, QWidget *)> clicked, QWidget *parent = nullptr)
        : QWidget(parent) {
        setMinimumSize(710, 430);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        for (int i = 0; i < 30; i++) {
            auto *key = new QPushButton(this);
            keys[i] = key;
            key->setObjectName(QStringLiteral("shortcutKey%1").arg(i));
            key->setText(QString::number(i < 12 ? i % 6 + 1 : i < 28 ? i - 11 : i - 27));
            key->setCursor(Qt::PointingHandCursor);
            key->setStyleSheet(QStringLiteral(
                "QPushButton { color: white; background: rgba(40, 45, 52, 180);"
                " border: 1px solid #8c969f; border-radius: 3px; padding: 0px; }"
                "QPushButton:hover, QPushButton:focus { background: #286eaa; border-color: #95d0ff; }"));
            connect(key, &QPushButton::clicked, this, [clicked, key, i] { clicked(i, key); });
        }
        arrange();
    }
    void setSoftVisible(bool visible) { softVisible = visible; arrange(); }
    void setBindingDescription(int i, const QString &label, const QString &description) {
        keys[i]->setAccessibleName(label);
        keys[i]->setToolTip(label + QStringLiteral("\n当前配置：") + description);
    }
};
