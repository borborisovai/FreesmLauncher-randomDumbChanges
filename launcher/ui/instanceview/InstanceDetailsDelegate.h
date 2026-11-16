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
 *  Delegate for the details (table) view of the instance list: paints the icon,
 *  the (possibly elided) name and the status badges in the name column and keeps
 *  the other columns default. Provides inline renaming of the instance like in
 *  the grid view.
 */

#pragma once

#include <QStyledItemDelegate>

class InstanceDetailsDelegate : public QStyledItemDelegate {
    Q_OBJECT

   public:
    explicit InstanceDetailsDelegate(QObject* parent = nullptr);
    ~InstanceDetailsDelegate() override = default;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;

   signals:
    /// emitted before the new name is committed, like ListViewDelegate::textChanged
    void textChanged(QString before, QString after) const;
};
