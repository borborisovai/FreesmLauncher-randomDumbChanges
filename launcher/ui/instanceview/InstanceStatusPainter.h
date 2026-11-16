// SPDX-License-Identifier: GPL-3.0-or-later
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2026 FreesmLauncher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 *  Shared helpers for painting instance status badges (running / broken / update available)
 *  and progress overlays. Extracted from InstanceDelegate.cpp so both the grid delegate
 *  and the details (table) delegate can use them.
 */

#pragma once

#include <BaseInstance.h>

#include <QIcon>
#include <QList>
#include <QListIterator>
#include <QPainter>
#include <QPalette>
#include <QRect>
#include <QStyleOptionViewItem>
#include <QtMath>

/// Collects the status badge icon names for the given instance, in paint order.
inline QList<QString> instanceStatusBadgeNames(BaseInstance* instance)
{
    QList<QString> pixmaps;
    if (!instance) {
        return pixmaps;
    }
    if (instance->isRunning()) {
        pixmaps.append(QStringLiteral("status-running"));
    } else if (instance->hasCrashed() || instance->hasVersionBroken()) {
        pixmaps.append(QStringLiteral("status-bad"));
    }
    if (instance->hasUpdateAvailable()) {
        pixmaps.append(QStringLiteral("checkupdate"));
    }
    return pixmaps;
}

/// Returns how many badges would be painted for the given instance.
/// Used by delegates to leave room for the badges when eliding text.
inline int statusBadgeCount(BaseInstance* instance)
{
    return static_cast<int>(instanceStatusBadgeNames(instance).size());
}

/// Paints the status badges for the given instance, anchored to the top-right corner of \a cellRect.
inline void drawStatusBadges(QPainter* painter, const QRect& cellRect, BaseInstance* instance, QIcon::Mode mode, QIcon::State state)
{
    const QList<QString> pixmaps = instanceStatusBadgeNames(instance);
    if (pixmaps.isEmpty()) {
        return;
    }

    static const int itemSide = 24;
    static const int spacing = 1;
    const int itemsPerRow = qMax(1, qFloor(double(cellRect.width() + spacing) / double(itemSide + spacing)));
    const int rows = qCeil((double)pixmaps.size() / (double)itemsPerRow);
    QListIterator<QString> it(pixmaps);

    painter->save();
    painter->translate(cellRect.topLeft());
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < itemsPerRow; ++x) {
            if (!it.hasNext()) {
                painter->restore();
                return;
            }
            // FIXME: inject this.
            auto icon = QIcon::fromTheme(it.next());
            QRect badgeRect(cellRect.width() - x * itemSide + qMax(x - 1, 0) * spacing - itemSide,
                            y * itemSide + qMax(y - 1, 0) * spacing, itemSide, itemSide);
            icon.paint(painter, badgeRect, Qt::AlignCenter, mode, state);
        }
    }
    painter->restore();
}

/// Draws a pie-shaped progress overlay on top of \a rect. Does nothing when maximum is 0 or value == maximum.
inline void drawProgressOverlay(QPainter* painter, const QRect& rect, const QPalette& palette, const int value, const int maximum)
{
    if (maximum == 0 || value == maximum) {
        return;
    }

    painter->save();

    qreal percent = (qreal)value / (qreal)maximum;
    QColor color = palette.color(QPalette::Dark);
    color.setAlphaF(0.70f);
    painter->setBrush(color);
    painter->setPen(QPen(QBrush(), 0));
    painter->drawPie(rect, 90 * 16, -qRound(percent * 360.0 * 16));

    painter->restore();
}
