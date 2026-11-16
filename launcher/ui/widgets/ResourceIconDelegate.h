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
 */

#pragma once

#include <QStyledItemDelegate>

/**
 * Delegate for the icon grid view of the external resources pages (mods,
 * resource packs, shader packs, texture packs and data packs) and for the
 * icon grid views of the worlds, servers and components pages.
 *
 * The grid shows each resource as a large icon with its name below it,
 * and a small badge in the corner for resources that have reported issues
 * (the same information the table shows in the name column). Disabled
 * resources are painted semi-transparent, like in the table.
 *
 * Most ResourceFolderModel-derived models use the same column layout:
 *   column 0 - the enable/disable checkbox,
 *   column 1 - the image (DecorationRole with a QIcon),
 *   column 2 - the name.
 * Shader packs have no embedded image and use the base layout (name in column 1);
 * pass -1 as \a imageColumn for those, the delegate will paint a generic placeholder.
 *
 * Views whose models have no enable/disable checkbox or issue badge can opt out
 * (see \a checkColumn and \a badgeColumn).
 */
class ResourceIconDelegate : public QStyledItemDelegate {
    Q_OBJECT

   public:
    /// \a imageColumn and \a nameColumn default to the shared layout (1 and 2).
    /// Pass -1 for \a checkColumn to skip the disabled dimming (views without a checkbox),
    /// and -1 for \a badgeColumn to skip the issue badge.
    explicit ResourceIconDelegate(int imageColumn = 1,
                                  int nameColumn = 2,
                                  QObject* parent = nullptr,
                                  int checkColumn = 0,
                                  int badgeColumn = 2);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    /// Change the icon size (in pixels); the cell size adapts to fit it.
    void setIconSize(int iconSize);
    int iconSize() const { return m_iconSize; }

   private:
    int m_imageColumn;
    int m_nameColumn;
    int m_checkColumn;
    /// -1 = no badge, >=0 = the column whose DecorationRole holds the badge icon
    int m_badgeColumn;
    int m_iconSize = 56;
};
