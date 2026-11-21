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
 *  Table (details) view of the instance list, shown as an alternative to the
 *  grid view. Modeled after Dolphin's details view: a QTreeView with sortable
 *  columns, single-row selection and the same drag and drop semantics as the
 *  grid view (dropping URLs adds instances, dropping an instance onto another
 *  instance moves it into the same group).
 */

#pragma once

#include <QTreeView>

class CatPainter;

class InstanceDetailsView : public QTreeView {
    Q_OBJECT

   public:
    explicit InstanceDetailsView(QWidget* parent = nullptr);

    void setModel(QAbstractItemModel* model) override;
    void setPaintCat(bool enabled);

   signals:
    void droppedURLs(QList<QUrl> urls);
    void zoomRequested(int delta);

   protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void startDrag(Qt::DropActions supportedActions) override;
    void wheelEvent(QWheelEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

   private:
    CatPainter* m_cat = nullptr;
};
