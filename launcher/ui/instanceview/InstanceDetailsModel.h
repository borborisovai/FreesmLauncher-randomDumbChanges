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
 *  Multi-column proxy model backing the details (table) view of the instance list.
 *  The source model is a plain list model (InstanceList) with a single column;
 *  this proxy exposes the instance metadata as sortable columns, in the spirit of
 *  Dolphin's details view.
 */

#pragma once

#include "InstanceProxyModel.h"

class InstanceDetailsModel : public InstanceProxyModel {
    Q_OBJECT

   public:
    enum Column {
        ColumnName = 0,
        ColumnVersion,
        ColumnGroup,
        ColumnLastLaunched,
        ColumnPlaytime,
        ColumnCount
    };

    explicit InstanceDetailsModel(QObject* parent = nullptr);

    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    void setSourceModel(QAbstractItemModel* sourceModel) override;

   protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

   private slots:
    /// re-emits source dataChanged() stretched over all columns, so the whole row repaints
    void sourceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles);
};
