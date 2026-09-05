#include "CardDialog.h"
#include "ThemeManager.h"
#include "AppSettings.h"
#include <QPainter>
#include <QMouseEvent>

CardDialog::CardDialog(QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setModal(true);
}

void CardDialog::paintEvent(QPaintEvent *) {
    QPainter p(this);
    ThemeManager::paintCard(p, rect(), QString(), AppSettings::instance().dialogOpacity());
}

void CardDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && !childAt(event->pos())) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void CardDialog::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void CardDialog::mouseReleaseEvent(QMouseEvent *event) {
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}

void CardWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    ThemeManager::paintCard(p, rect());
}
