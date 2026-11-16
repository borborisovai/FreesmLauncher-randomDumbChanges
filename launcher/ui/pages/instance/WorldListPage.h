// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
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

#include <LoggedProcess.h>
#include "minecraft/MinecraftInstance.h"
#include "ui/pages/BasePage.h"

#include "settings/Setting.h"
#include "ui/instanceview/InstanceViewMode.h"

class WorldList;
class QAbstractItemView;
class QItemSelectionModel;
class QListView;
class QStackedWidget;
class QAction;
class ZoomStatusSlider;
namespace Ui {
class WorldListPage;
}

class WorldListPage : public QMainWindow, public BasePage {
    Q_OBJECT

   public:
    explicit WorldListPage(MinecraftInstance* inst, WorldList* worlds, QWidget* parent = 0);
    virtual ~WorldListPage();

    virtual QString displayName() const override { return tr("Worlds"); }
    virtual QIcon icon() const override { return QIcon::fromTheme("worlds"); }
    virtual QString id() const override { return "worlds"; }
    virtual QString helpPage() const override { return "Worlds"; }
    virtual bool shouldDisplay() const override;
    void retranslate() override;

    virtual void openedImpl() override;
    virtual void closedImpl() override;

   public slots:
    /// switch between the table (details) and the icon grid (modeled after Dolphin's view mode switching);
    /// the mode is persisted globally in the WorldListViewMode setting
    void setViewMode(InstanceViewMode mode);
    /// set the zoom level (0-16, modeled after Dolphin) of the icon grid;
    /// persisted globally in the WorldsIconZoomLevel setting unless \a save is false
    void setZoomLevel(int level, bool save = true);

   protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;
    bool worldListFilter(QKeyEvent* ev);
    QMenu* createPopupMenu() override;

   protected:
    MinecraftInstance* m_inst;

   private:
    QModelIndex getSelectedWorld();
    bool isWorldSafe(QModelIndex index);
    bool worldSafetyNagQuestion(const QString& actionType);
    void mceditError();
    /// the currently visible view (table or icon grid)
    QAbstractItemView* listView() const;
    void syncSelection(QItemSelectionModel* from, QItemSelectionModel* to);

   private:
    Ui::WorldListPage* ui;
    WorldList* m_worlds;
    unique_qobject_ptr<LoggedProcess> m_mceditProcess;
    bool m_mceditStarting = false;

    std::shared_ptr<Setting> m_wide_bar_setting = nullptr;
    std::unique_ptr<DataPackFolderModel> m_datapackModel;

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

   private slots:
    void on_actionCopy_Seed_triggered();
    void on_actionMCEdit_triggered();
    void on_actionRemove_triggered();
    void on_actionAdd_triggered();
    void on_actionCopy_triggered();
    void on_actionRename_triggered();
    void on_actionRefresh_triggered();
    void on_actionView_Folder_triggered();
    void on_actionData_Packs_triggered();
    void on_actionReset_Icon_triggered();
    void worldChanged(const QModelIndex& current, const QModelIndex& previous);
    void mceditState(LoggedProcess::State state);
    void on_actionJoin_triggered();

    void ShowContextMenu(const QPoint& pos);
};
