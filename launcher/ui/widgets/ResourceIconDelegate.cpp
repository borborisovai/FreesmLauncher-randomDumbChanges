// SPDX-License-Identifier: GPL-3.0-only
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
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "ResourceIconDelegate.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QStyle>

constexpr int ICON_SIZE = 56;
constexpr int ITEM_WIDTH = 104;
constexpr int ITEM_HEIGHT = 100;
constexpr int CELL_PADDING = 4;
constexpr int BADGE_SIZE = 16;

ResourceIconDelegate::ResourceIconDelegate(int imageColumn, int nameColumn, QObject* parent)
    : QStyledItemDelegate(parent), m_imageColumn(imageColumn), m_nameColumn(nameColumn)
{
}

void ResourceIconDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const bool selected = opt.state & QStyle::State_Selected;
    const bool enabled = index.sibling(index.row(), 0).data(Qt::CheckStateRole).toInt() == Qt::Checked;

    painter->save();
    painter->setClipRect(opt.rect);

    // selection background; left transparent otherwise so the alternating row colors show
    if (selected) {
        painter->fillRect(opt.rect, opt.palette.brush(QPalette::Highlight));
    }

    if (!enabled) {
        // dim disabled resources, like disabled rows in the table
        painter->setOpacity(0.4);
    }

    const QPalette::ColorGroup cg = opt.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
    if (selected) {
        painter->setPen(opt.palette.color(cg, QPalette::HighlightedText));
    } else {
        painter->setPen(opt.palette.color(cg, QPalette::Text));
    }

    // the icon, vertically centered in the upper part of the cell
    const QIcon icon = m_imageColumn >= 0 ? index.sibling(index.row(), m_imageColumn).data(Qt::DecorationRole).value<QIcon>()
                                          : QIcon();
    QRect iconRect(opt.rect.x() + (opt.rect.width() - ICON_SIZE) / 2, opt.rect.y() + CELL_PADDING, ICON_SIZE, ICON_SIZE);
    if (icon.isNull()) {
        // generic placeholder for resources without an embedded image
        auto* style = opt.widget ? opt.widget->style() : QApplication::style();
        QIcon fallback = style->standardIcon(QStyle::SP_FileIcon, nullptr, opt.widget);
        fallback.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal);
    } else {
        icon.paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal);
    }

    // the name, elided to fit below the icon
    const QString text = index.sibling(index.row(), m_nameColumn).data(Qt::DisplayRole).toString();
    const QRect textRect(opt.rect.x() + CELL_PADDING, iconRect.bottom() + CELL_PADDING / 2, opt.rect.width() - 2 * CELL_PADDING,
                         opt.rect.bottom() - iconRect.bottom() - CELL_PADDING);
    const QFontMetrics fm(opt.font);
    const QString elided = fm.elidedText(text, Qt::ElideRight, qMax(0, textRect.width()));
    painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextSingleLine, elided);

    // small issue badge in the corner, if the model reports one for the name column
    // (this is what the table view paints in the name column)
    const QIcon badge = index.sibling(index.row(), m_nameColumn).data(Qt::DecorationRole).value<QIcon>();
    if (!badge.isNull()) {
        const QRect badgeRect(iconRect.right() - BADGE_SIZE + CELL_PADDING / 2, iconRect.top(), BADGE_SIZE, BADGE_SIZE);
        badge.paint(painter, badgeRect);
    }

    painter->restore();
}

QSize ResourceIconDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(ITEM_WIDTH, ITEM_HEIGHT);
}
