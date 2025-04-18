/*
 *   SPDX-FileCopyrightText: 2016 Nicolas Carion
 *   SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QFontInfo>

template <typename Curve_t>
AbstractCurveWidget<Curve_t>::AbstractCurveWidget(QWidget *parent)
    : __dummy_AbstractCurveWidget(parent)
    , m_pixmapCache(nullptr)

{
    setMouseTracking(true);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(QFontInfo(font()).pixelSize() * 10, QFontInfo(font()).pixelSize() * 10);
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sp.setHeightForWidth(true); // force widget to have a height dependent on width;
    setSizePolicy(sp);
    setFocusPolicy(Qt::StrongFocus);
}

template <typename Curve_t> void AbstractCurveWidget<Curve_t>::paintBackground(QPainter *p)
{
    /*
     * Zoom
     */
    m_wWidth = width() - 1;
    m_wHeight = height() - 1;
    int offsetX = int(1 / 8. * m_zoomLevel * m_wWidth);
    int offsetY = int(1 / 8. * m_zoomLevel * m_wHeight);
    m_wWidth -= 2 * offsetX;
    m_wHeight -= 2 * offsetY;

    p->translate(offsetX, offsetY);

    /*
     * Background
     */
    p->fillRect(rect().translated(-offsetX, -offsetY), palette().window());
    if (!m_pixmap.isNull()) {
        if (m_pixmapIsDirty || !m_pixmapCache) {
            m_pixmapCache = std::make_shared<QPixmap>(m_wWidth + 1, m_wHeight + 1);
            QPainter cachePainter(m_pixmapCache.get());

            cachePainter.scale(1.0 * (m_wWidth + 1) / m_pixmap.width(), 1.0 * (m_wHeight + 1) / m_pixmap.height());
            cachePainter.drawPixmap(0, 0, m_pixmap);
            m_pixmapIsDirty = false;
        }
        p->drawPixmap(0, 0, *m_pixmapCache);
    }

    // select color of the grid, depending on whether we have a palette or not
    if (!m_pixmap.isNull()) {
        p->setPen(QPen(palette().mid().color(), 1, Qt::SolidLine));
    } else {
        int h, s, l, a;
        auto bg = palette().color(QPalette::Window);
        bg.getHsl(&h, &s, &l, &a);
        l = (l > 128) ? l - 30 : l + 30;
        bg.setHsl(h, s, l, a);
        p->setPen(QPen(bg, 1, Qt::SolidLine));
    }

    /*
     * Borders
     */
    p->drawRect(0, 0, m_wWidth, m_wHeight);

    /*
     * Grid
     */
    if (m_gridLines != 0) {
        double stepH = m_wWidth / double(m_gridLines + 1);
        double stepV = m_wHeight / double(m_gridLines + 1);
        for (int i = 1; i <= m_gridLines; ++i) {
            p->drawLine(QLineF(i * stepH, 0, i * stepH, m_wHeight));
            p->drawLine(QLineF(0, i * stepV, m_wWidth, i * stepV));
        }
    }

    p->setRenderHint(QPainter::Antialiasing);

    /*
     * Standard line
     */
    p->drawLine(QLineF(0, m_wHeight, m_wWidth, 0));
}

template <typename Curve_t> void AbstractCurveWidget<Curve_t>::setMaxPoints(int max)
{
    Q_ASSERT(max >= 2);
    m_maxPoints = max;
}

template <typename Curve_t> void AbstractCurveWidget<Curve_t>::setPixmap(const QPixmap &pix)
{
    m_pixmap = pix;
    m_pixmapIsDirty = true;
    update();
}

template <typename Curve_t> int AbstractCurveWidget<Curve_t>::gridLines() const
{
    return m_gridLines;
}

template <typename Curve_t> void AbstractCurveWidget<Curve_t>::setGridLines(int lines)
{
    m_gridLines = qBound(0, lines, 8);
    update();
}

template <typename Curve_t> void AbstractCurveWidget<Curve_t>::slotZoomIn()
{
    m_zoomLevel = qMax(m_zoomLevel - 1, 0);
    m_pixmapIsDirty = true;
    update();
}

template <typename Curve_t> void AbstractCurveWidget<Curve_t>::slotZoomOut()
{
    m_zoomLevel = qMin(m_zoomLevel + 1, 3);
    m_pixmapIsDirty = true;
    update();
}

template <typename Curve_t> void AbstractCurveWidget<Curve_t>::setFromString(const QString &str)
{
    m_curve.fromString(str);
    m_currentPointIndex = -1;
    Q_EMIT currentPoint(Point_t(), true);
    update();
}

template <typename Curve_t> void AbstractCurveWidget<Curve_t>::reset()
{
    setFromString(Curve_t().toString());
    m_currentPointIndex = -1;
    Q_EMIT currentPoint(Point_t(), true);
    Q_EMIT modified();
    update();
}

template <typename Curve_t> QString AbstractCurveWidget<Curve_t>::toString()
{
    return m_curve.toString();
}

template <typename Curve_t> void AbstractCurveWidget<Curve_t>::updateCurrentPoint(const Point_t &p, bool final)
{
    if (m_currentPointIndex >= 0) {
        m_curve.setPoint(m_currentPointIndex, p);
        // during validation the point might have changed
        Q_EMIT currentPoint(m_curve.getPoint(m_currentPointIndex), isCurrentPointExtremal());
        if (final) {
            Q_EMIT modified();
        }
        update();
    }
}

template <typename Curve_t> typename AbstractCurveWidget<Curve_t>::Point_t AbstractCurveWidget<Curve_t>::getCurrentPoint()
{
    if (m_currentPointIndex >= 0) {
        return m_curve.getPoint(m_currentPointIndex);
    } else {
        return Point_t();
    }
}

template <typename Curve_t> bool AbstractCurveWidget<Curve_t>::isCurrentPointExtremal()
{
    return m_currentPointIndex == 0 || m_currentPointIndex == m_curve.points().size() - 1;
}

template <typename Curve_t> void AbstractCurveWidget<Curve_t>::slotDeleteCurrentPoint()
{
    if (m_currentPointIndex > 0 && m_currentPointIndex < m_curve.points().size() - 1) {
        m_curve.removePoint(m_currentPointIndex);
        m_currentPointIndex--;
        Q_EMIT currentPoint(m_curve.getPoint(m_currentPointIndex), isCurrentPointExtremal());
        update();
        Q_EMIT modified();
        setCursor(Qt::ArrowCursor);
        m_state = State_t::NORMAL;
    }
}

template <typename Curve_t> void AbstractCurveWidget<Curve_t>::resizeEvent(QResizeEvent *e)
{
    m_pixmapIsDirty = true;
    QWidget::resizeEvent(e);
}

template <typename Curve_t> void AbstractCurveWidget<Curve_t>::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
}

template <typename Curve_t> void AbstractCurveWidget<Curve_t>::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }

    setCursor(Qt::ArrowCursor);
    m_state = State_t::NORMAL;

    Q_EMIT modified();
}

template <typename Curve_t> void AbstractCurveWidget<Curve_t>::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
        slotDeleteCurrentPoint();
    } else {
        QWidget::keyPressEvent(e);
    }
}

template <typename Curve_t> int AbstractCurveWidget<Curve_t>::heightForWidth(int w) const
{
    return w;
}
