// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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

#pragma once

#include <QMainWindow>
#include <QString>

#include "BaseInstance.h"
#include "ui/pages/BasePage.h"

#include "settings/Setting.h"
#include "ui/instanceview/InstanceViewMode.h"

namespace Ui {
class ServersPage;
}
class QAbstractItemView;
class QItemSelectionModel;
class QListView;
class QStackedWidget;
class QAction;
class ZoomStatusSlider;

struct Server;
class ServersModel;
class MinecraftInstance;

class ServersPage : public QMainWindow, public BasePage {
    Q_OBJECT

   public:
    explicit ServersPage(BaseInstance* inst, QWidget* parent = 0);
    virtual ~ServersPage();

    void openedImpl() override;
    void closedImpl() override;

    virtual QString displayName() const override { return tr("Servers"); }
    virtual QIcon icon() const override { return QIcon::fromTheme("server"); }
    virtual QString id() const override { return "servers"; }
    virtual QString helpPage() const override { return "Servers-management"; }
    void retranslate() override;

   public slots:
    /// switch between the table (details) and the icon grid (modeled after Dolphin's view mode switching);
    /// the mode is persisted globally in the ServersListViewMode setting
    void setViewMode(InstanceViewMode mode);
    /// set the zoom level (0-16, modeled after Dolphin) of the icon grid;
    /// persisted globally in the ServersIconZoomLevel setting unless \a save is false
    void setZoomLevel(int level, bool save = true);

   protected:
    QMenu* createPopupMenu() override;

   private:
    void updateState();
    void scheduleSave();
    bool saveIsScheduled() const;
    /// the currently visible view (table or icon grid)
    QAbstractItemView* listView() const;
    void syncSelection(QItemSelectionModel* from, QItemSelectionModel* to);

   private slots:
    void currentChanged(const QModelIndex& current, const QModelIndex& previous);
    void rowsRemoved(const QModelIndex& parent, int first, int last);

    void on_actionAdd_triggered();
    void on_actionRemove_triggered();
    void on_actionMove_Up_triggered();
    void on_actionMove_Down_triggered();
    void on_actionJoin_triggered();
    void on_actionRefresh_triggered();

    void runningStateChanged(bool running);

    void nameEdited(const QString& name);
    void addressEdited(const QString& address);
    void resourceIndexChanged(int index);

    void ShowContextMenu(const QPoint& pos);

   private:  // data
    int currentServer = -1;
    bool m_locked = true;
    Ui::ServersPage* ui = nullptr;
    ServersModel* m_model = nullptr;
    BaseInstance* m_inst = nullptr;

    std::shared_ptr<Setting> m_wide_bar_setting = nullptr;

    QStackedWidget* m_viewStack = nullptr;
    QListView* m_iconView = nullptr;
    InstanceViewMode m_viewMode = InstanceViewMode::Details;
    QAction* m_actionListViewIcons = nullptr;
    QAction* m_actionListViewDetails = nullptr;
    QAction* m_viewModeAction = nullptr;
    QAction* m_actionZoomIn = nullptr;
    QAction* m_actionZoomOut = nullptr;
    QAction* m_actionZoomReset = nullptr;
    ZoomStatusSlider* m_zoomSlider = nullptr;
    int m_zoomLevel = 4;
    bool m_syncing_selection = false;
};
