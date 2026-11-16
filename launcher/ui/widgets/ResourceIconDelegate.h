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
 * resource packs, shader packs, texture packs and data packs).
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
 */
class ResourceIconDelegate : public QStyledItemDelegate {
    Q_OBJECT

   public:
    /// \a imageColumn and \a nameColumn default to the shared layout (1 and 2).
    explicit ResourceIconDelegate(int imageColumn = 1, int nameColumn = 2, QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

   private:
    int m_imageColumn;
    int m_nameColumn;
};
