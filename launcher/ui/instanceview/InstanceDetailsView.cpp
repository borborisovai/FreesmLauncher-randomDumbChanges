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
 */

#include "InstanceDetailsView.h"

#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHeaderView>
#include <QMimeData>
#include <QUrl>

#include <Application.h>
#include "InstanceDetailsModel.h"
#include "InstanceList.h"

InstanceDetailsView::InstanceDetailsView(QWidget* parent) : QTreeView(parent)
{
    setAllColumnsShowFocus(true);
    setRootIsDecorated(false);
    setAlternatingRowColors(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSortingEnabled(true);
    // renaming happens through the F2 shortcut (event filter in the main window), like in the grid view
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setFrameShape(QFrame::NoFrame);
    // do not show ugly blue border on the mac
    setAttribute(Qt::WA_MacShowFocusRect, false);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setIconSize(QSize(24, 24));

    setDragEnabled(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    viewport()->setAcceptDrops(true);
}

void InstanceDetailsView::setModel(QAbstractItemModel* model)
{
    QTreeView::setModel(model);

    auto header = this->header();
    header->setStretchLastSection(false);
    header->setSectionsMovable(false);
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    if (header->count() >= InstanceDetailsModel::ColumnCount) {
        header->setSectionResizeMode(InstanceDetailsModel::ColumnName, QHeaderView::Stretch);
        header->setSectionResizeMode(InstanceDetailsModel::ColumnVersion, QHeaderView::Interactive);
        header->setSectionResizeMode(InstanceDetailsModel::ColumnGroup, QHeaderView::Interactive);
        header->setSectionResizeMode(InstanceDetailsModel::ColumnLastLaunched, QHeaderView::Interactive);
        header->setSectionResizeMode(InstanceDetailsModel::ColumnPlaytime, QHeaderView::Interactive);
        header->setMinimumSectionSize(60);
        setColumnWidth(InstanceDetailsModel::ColumnVersion, 100);
        setColumnWidth(InstanceDetailsModel::ColumnGroup, 120);
        setColumnWidth(InstanceDetailsModel::ColumnLastLaunched, 170);
        setColumnWidth(InstanceDetailsModel::ColumnPlaytime, 110);
    }
}

void InstanceDetailsView::dragEnterEvent(QDragEnterEvent* event)
{
    if (!event->mimeData()) {
        return;
    }
    if (event->mimeData()->hasUrls() || event->mimeData()->hasFormat(QStringLiteral("application/x-instanceid"))) {
        event->acceptProposedAction();
        return;
    }
    QTreeView::dragEnterEvent(event);
}

void InstanceDetailsView::dragMoveEvent(QDragMoveEvent* event)
{
    if (!event->mimeData()) {
        return;
    }
    if (event->mimeData()->hasUrls() || event->mimeData()->hasFormat(QStringLiteral("application/x-instanceid"))) {
        event->acceptProposedAction();
        return;
    }
    QTreeView::dragMoveEvent(event);
}

void InstanceDetailsView::dropEvent(QDropEvent* event)
{
    auto mimeData = event->mimeData();
    if (!mimeData) {
        return;
    }

    if (mimeData->hasUrls() && event->source() != this) {
        // files dropped from outside (modpacks, mrpacks, links, ...)
        const auto urls = mimeData->urls();
        if (!urls.isEmpty()) {
            event->acceptProposedAction();
            emit droppedURLs(urls);
        }
        return;
    }

    if (event->source() == this && (event->possibleActions() & Qt::MoveAction)
        && mimeData->hasFormat(QStringLiteral("application/x-instanceid"))) {
        // dropping an instance onto another one moves it into the same group
        const QString draggedId = QString::fromUtf8(mimeData->data(QStringLiteral("application/x-instanceid")));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPoint dropPos = event->position().toPoint();
#else
        const QPoint dropPos = event->pos();
#endif
        const QModelIndex dropIndex = indexAt(dropPos);

        GroupId targetGroup;  // dropping onto an empty area removes the instance from its group
        if (dropIndex.isValid()) {
            auto* proxy = qobject_cast<InstanceDetailsModel*>(model());
            const QModelIndex sourceIndex = proxy ? proxy->mapToSource(dropIndex.sibling(dropIndex.row(), 0)) : QModelIndex();
            const QString targetId = sourceIndex.data(InstanceList::InstanceIDRole).toString();
            if (targetId == draggedId) {
                return;  // dropped onto itself
            }
            targetGroup = APPLICATION->instances()->getInstanceGroup(targetId);
        }

        APPLICATION->instances()->setInstanceGroup(draggedId, targetGroup);
        event->setDropAction(Qt::MoveAction);
        event->accept();
        return;
    }

    QTreeView::dropEvent(event);
}

void InstanceDetailsView::startDrag(Qt::DropActions supportedActions)
{
    // pass only the first-column indexes to the model, otherwise InstanceList::mimeData()
    // (which expects a single index) produces an empty mime data
    const QModelIndexList selected = selectionModel()->selectedIndexes();
    QModelIndexList firstColumn;
    for (const QModelIndex& index : selected) {
        if (index.column() == 0) {
            firstColumn << index;
        }
    }
    if (firstColumn.isEmpty()) {
        return;
    }
    QMimeData* mimeData = model()->mimeData(firstColumn);
    if (!mimeData) {
        return;
    }
    auto* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->exec(supportedActions, Qt::MoveAction);
}
