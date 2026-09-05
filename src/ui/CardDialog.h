#pragma once

#include <QDialog>
#include <QPoint>
#include <QWidget>

// Frameless, translucent dialog painted as a floating card (same look as the main window
// panels). Draggable from any non-interactive area; Esc rejects.
// Plain container painted as a card (used inside dialogs that sit on a backdrop).
class CardWidget : public QWidget {
    Q_OBJECT
public:
    using QWidget::QWidget;
protected:
    void paintEvent(QPaintEvent *event) override;
};

class CardDialog : public QDialog {
    Q_OBJECT
public:
    explicit CardDialog(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QPoint m_dragOffset;
    bool m_dragging = false;
};
