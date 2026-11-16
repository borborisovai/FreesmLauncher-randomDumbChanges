// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2023 Trial97 <alexandru.tripon97@gmail.com>
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

#include "ExternalResourcesPage.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui_ExternalResourcesPage.h"

#include "DesktopServices.h"
#include "Version.h"
#include "minecraft/mod/ResourceFolderModel.h"
#include "ui/GuiUtil.h"

#include "ui/instanceview/InstanceViewMode.h"
#include "ui/widgets/ResourceIconDelegate.h"

#include <QActionGroup>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QListView>
#include <QMenu>
#include <QStackedWidget>
#include <QStatusBar>

#include "ui/widgets/ZoomSlider.h"
#include <QToolButton>
#include <algorithm>

ExternalResourcesPage::ExternalResourcesPage(BaseInstance* instance, ResourceFolderModel* model, QWidget* parent)
    : QMainWindow(parent), m_instance(instance), ui(new Ui::ExternalResourcesPage), m_model(model)
{
    ui->setupUi(this);

    ui->actionsToolbar->insertSpacer(ui->actionViewFolder);

    m_filterModel = model->createFilterProxyModel(this);
    m_filterModel->setDynamicSortFilter(true);
    m_filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_filterModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_filterModel->setSourceModel(m_model);
    m_filterModel->setFilterKeyColumn(-1);
    ui->treeView->setModel(m_filterModel);
    // must come after setModel
    ui->treeView->setResizeModes(m_model->columnResizeModes());

    ui->treeView->installEventFilter(this);
    ui->treeView->sortByColumn(1, Qt::AscendingOrder);
    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);

    // Icon grid view, stacked with the table (modeled after Dolphin's view mode switching).
    // Both views share the same filter model and keep their selections in sync.
    m_iconView = new QListView(this);
    m_iconView->setViewMode(QListView::IconMode);
    m_iconView->setIconSize(QSize(56, 56));
    m_iconView->setSpacing(6);
    m_iconView->setResizeMode(QListView::Adjust);
    m_iconView->setMovement(QListView::Static);
    m_iconView->setUniformItemSizes(true);
    m_iconView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_iconView->setFrameShape(QFrame::NoFrame);
    m_iconView->setAttribute(Qt::WA_MacShowFocusRect, false);
    // dropping files onto the grid installs them, like dropping onto the table
    m_iconView->setDragDropMode(QAbstractItemView::DropOnly);
    m_iconView->setDropIndicatorShown(true);
    m_iconView->viewport()->setAcceptDrops(true);
    m_iconView->setItemDelegate(new ResourceIconDelegate(m_model->listViewImageColumn(), m_model->listViewNameColumn(), m_iconView, 0, m_model->listViewNameColumn()));
    m_iconView->setModel(m_filterModel);
    m_iconView->installEventFilter(this);
    m_iconView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_iconView, &QWidget::customContextMenuRequested, this, &ExternalResourcesPage::ShowContextMenu);
    connect(m_iconView, &QListView::activated, this, &ExternalResourcesPage::itemActivated);
    connect(m_iconView->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current, const QModelIndex& previous) {
                if (!current.isValid()) {
                    ui->frame->clear();
                    return;
                }
                updateFrame(current, previous);
            });
    connect(m_iconView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] { updateActions(); });

    // stack the table and the grid; the grid layout cell is the one the treeView occupied
    auto* layout = ui->gridLayout;
    layout->removeWidget(ui->treeView);
    m_viewStack = new QStackedWidget(this);
    m_viewStack->addWidget(ui->treeView);
    m_viewStack->addWidget(m_iconView);
    layout->addWidget(m_viewStack, 1, 1, 1, 2);

    // keep the selections of the two views in sync (same filter model, full rows in both)
    connect(ui->treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { syncListSelection(ui->treeView->selectionModel(), m_iconView->selectionModel()); });
    connect(m_iconView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { syncListSelection(m_iconView->selectionModel(), ui->treeView->selectionModel()); });

    // view mode switcher, modeled after Dolphin's view_mode action; the mode is
    // persisted globally in the ResourceListViewMode setting
    m_actionListViewIcons = new QAction(QIcon::fromTheme(QStringLiteral("view-list-icons")), tr("Icons"), this);
    m_actionListViewIcons->setCheckable(true);
    m_actionListViewIcons->setToolTip(tr("Show items as a grid of icons"));
    m_actionListViewDetails = new QAction(QIcon::fromTheme(QStringLiteral("view-list-tree")), tr("Details"), this);
    m_actionListViewDetails->setCheckable(true);
    m_actionListViewDetails->setToolTip(tr("Show items as a table"));
    auto* viewModeGroup = new QActionGroup(this);
    viewModeGroup->addAction(m_actionListViewIcons);
    viewModeGroup->addAction(m_actionListViewDetails);
    connect(m_actionListViewIcons, &QAction::triggered, this, [this] { setResourceViewMode(InstanceViewMode::Icons); });
    connect(m_actionListViewDetails, &QAction::triggered, this, [this] { setResourceViewMode(InstanceViewMode::Details); });

    auto* viewModeMenu = new QMenu(this);
    viewModeMenu->addAction(m_actionListViewIcons);
    viewModeMenu->addAction(m_actionListViewDetails);
    m_viewModeAction = new QAction(QIcon::fromTheme(QStringLiteral("view-list-tree")), tr("View Mode"), this);
    m_viewModeAction->setMenu(viewModeMenu);
    m_viewModeAction->setToolTip(tr("Change view mode"));
    ui->actionsToolbar->insertActionBefore(ui->actionAddItem, m_viewModeAction);
    if (auto* button = qobject_cast<QToolButton*>(ui->actionsToolbar->widgetForAction(m_viewModeAction))) {
        button->setPopupMode(QToolButton::InstantPopup);
    }

    // zoom actions, modeled after Dolphin's zoom_in/zoom_out actions (Ctrl++/Ctrl+-/Ctrl+0);
    // the slider itself lives in the status bar, like Dolphin's zoom slider
    const auto zoomActions = createZoomToolbarActions(this);
    m_actionZoomIn = zoomActions.zoomIn;
    m_actionZoomOut = zoomActions.zoomOut;
    m_actionZoomReset = zoomActions.zoomReset;
    connect(m_actionZoomIn, &QAction::triggered, this, [this] { setZoomLevel(m_zoomLevel + 1); });
    connect(m_actionZoomOut, &QAction::triggered, this, [this] { setZoomLevel(m_zoomLevel - 1); });
    connect(m_actionZoomReset, &QAction::triggered, this, [this] { setZoomLevel(4); });
    ui->actionsToolbar->insertActionBefore(m_viewModeAction, m_actionZoomOut);
    ui->actionsToolbar->insertActionBefore(m_viewModeAction, m_actionZoomIn);
    this->addAction(m_actionZoomReset);  // widget-level, shortcut-only action (no toolbar button)

    auto* zoomSlider = new ZoomStatusSlider(this);
    m_zoomSlider = zoomSlider;
    connect(zoomSlider, &ZoomStatusSlider::zoomLevelChanged, this, &ExternalResourcesPage::setZoomLevel);
    setStatusBar(new QStatusBar(this));
    statusBar()->addPermanentWidget(zoomSlider);

    // The default function names by Qt are pretty ugly, so let's just connect the actions manually,
    // to make it easier to read :)
    connect(ui->actionAddItem, &QAction::triggered, this, &ExternalResourcesPage::addItem);
    connect(ui->actionRemoveItem, &QAction::triggered, this, &ExternalResourcesPage::removeItem);
    connect(ui->actionEnableItem, &QAction::triggered, this, &ExternalResourcesPage::enableItem);
    connect(ui->actionDisableItem, &QAction::triggered, this, &ExternalResourcesPage::disableItem);
    connect(ui->actionViewHomepage, &QAction::triggered, this, &ExternalResourcesPage::viewHomepage);
    connect(ui->actionViewConfigs, &QAction::triggered, this, &ExternalResourcesPage::viewConfigs);
    connect(ui->actionViewFolder, &QAction::triggered, this, &ExternalResourcesPage::viewFolder);

    connect(ui->treeView, &ModListView::customContextMenuRequested, this, &ExternalResourcesPage::ShowContextMenu);
    connect(ui->treeView, &ModListView::activated, this, &ExternalResourcesPage::itemActivated);

    auto selection_model = ui->treeView->selectionModel();

    connect(selection_model, &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current, const QModelIndex& previous) {
        if (!current.isValid()) {
            ui->frame->clear();
            return;
        }

        updateFrame(current, previous);
    });

    auto updateExtra = [this] { refreshExtraHeader(); };

    connect(selection_model, &QItemSelectionModel::selectionChanged, this, updateExtra);
    connect(m_iconView->selectionModel(), &QItemSelectionModel::selectionChanged, this, updateExtra);
    connect(model, &ResourceFolderModel::updateFinished, this, updateExtra);
    connect(model, &ResourceFolderModel::parseFinished, this, updateExtra);

    connect(selection_model, &QItemSelectionModel::selectionChanged, this, [this] { updateActions(); });
    connect(m_model, &ResourceFolderModel::rowsInserted, this, [this] { updateActions(); });
    connect(m_model, &ResourceFolderModel::rowsRemoved, this, [this] { updateActions(); });

    auto viewHeader = ui->treeView->header();
    viewHeader->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(viewHeader, &QHeaderView::customContextMenuRequested, this, &ExternalResourcesPage::ShowHeaderContextMenu);

    m_model->loadColumns(ui->treeView);
    connect(ui->treeView->header(), &QHeaderView::sectionResized, this, [this] { m_model->saveColumns(ui->treeView); });
    connect(ui->filterEdit, &QLineEdit::textChanged, this, &ExternalResourcesPage::filterTextChanged);
    updateActions();

    // apply the saved view mode (table or icon grid)
    setResourceViewMode(instanceViewModeFromString(APPLICATION->settings()->get("ResourceListViewMode").toString()));

    // apply the saved zoom level of the icon grid (persisted globally in the ResourceIconZoomLevel setting)
    setZoomLevel(APPLICATION->settings()->get("ResourceIconZoomLevel").toInt(), /*save=*/false);
}

ExternalResourcesPage::~ExternalResourcesPage()
{
    delete ui;
}

QMenu* ExternalResourcesPage::createPopupMenu()
{
    QMenu* filteredMenu = QMainWindow::createPopupMenu();
    filteredMenu->removeAction(ui->actionsToolbar->toggleViewAction());
    return filteredMenu;
}

void ExternalResourcesPage::ShowContextMenu(const QPoint& pos)
{
    // the context menu is shared between the table view and the icon grid view
    auto* senderWidget = qobject_cast<QWidget*>(sender());
    if (!senderWidget) {
        senderWidget = ui->treeView;
    }
    // right-clicking an item selects it first, so the menu actions apply to the clicked item
    if (auto* view = qobject_cast<QAbstractItemView*>(senderWidget)) {
        const QPoint viewportPos = view->viewport()->mapFrom(view, pos);
        if (const QModelIndex index = view->indexAt(viewportPos); index.isValid() && m_filterModel) {
            const int colCount = m_filterModel->columnCount();
            const QItemSelection rowSelection(m_filterModel->index(index.row(), 0), m_filterModel->index(index.row(), colCount - 1));
            view->selectionModel()->select(rowSelection, QItemSelectionModel::ClearAndSelect);
            view->setCurrentIndex(index);
        }
    }
    auto menu = ui->actionsToolbar->createContextMenu(this, tr("Context menu"));
    menu->exec(senderWidget->mapToGlobal(pos));
    delete menu;
}

void ExternalResourcesPage::ShowHeaderContextMenu(const QPoint& pos)
{
    auto menu = m_model->createHeaderContextMenu(ui->treeView);
    menu->exec(ui->treeView->mapToGlobal(pos));
    menu->deleteLater();
}

void ExternalResourcesPage::openedImpl()
{
    m_model->startWatching();

    auto const setting_name = QString("WideBarVisibility_%1").arg(id());
    m_wide_bar_setting = APPLICATION->settings()->getOrRegisterSetting(setting_name);

    ui->actionsToolbar->setVisibilityState(QByteArray::fromBase64(m_wide_bar_setting->get().toString().toUtf8()));
}

void ExternalResourcesPage::closedImpl()
{
    m_model->stopWatching();

    m_wide_bar_setting->set(QString::fromUtf8(ui->actionsToolbar->getVisibilityState().toBase64()));
}

void ExternalResourcesPage::retranslate()
{
    ui->retranslateUi(this);

    if (m_actionListViewIcons && m_actionListViewDetails && m_viewModeAction) {
        m_actionListViewIcons->setText(tr("Icons"));
        m_actionListViewIcons->setToolTip(tr("Show items as a grid of icons"));
        m_actionListViewDetails->setText(tr("Details"));
        m_actionListViewDetails->setToolTip(tr("Show items as a table"));
        m_viewModeAction->setText(tr("View Mode"));
        m_viewModeAction->setToolTip(tr("Change view mode"));
    }
}

void ExternalResourcesPage::itemActivated(const QModelIndex&)
{
    auto selection = m_filterModel->mapSelectionToSource(listSelectionModel()->selection());
    m_model->setResourceEnabled(selection.indexes(), EnableAction::TOGGLE);
}

void ExternalResourcesPage::filterTextChanged(const QString& newContents)
{
    m_viewFilter = newContents;
    m_filterModel->setFilterRegularExpression(m_viewFilter);
}

bool ExternalResourcesPage::shouldDisplay() const
{
    return true;
}

bool ExternalResourcesPage::listFilter(QKeyEvent* keyEvent)
{
    switch (keyEvent->key()) {
        case Qt::Key_Delete:
            removeItem();
            return true;
        case Qt::Key_Plus:
            addItem();
            return true;
        default:
            break;
    }
    return false;  // let the key press be handled normally
}

bool ExternalResourcesPage::eventFilter(QObject* obj, QEvent* ev)
{
    if (ev->type() != QEvent::KeyPress)
        return QWidget::eventFilter(obj, ev);

    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(ev);
    if (obj == ui->treeView || obj == m_iconView)
        return listFilter(keyEvent);

    return QWidget::eventFilter(obj, ev);
}

void ExternalResourcesPage::addItem()
{
    auto list = GuiUtil::BrowseForFiles(
        helpPage(), tr("Select %1", "Select whatever type of files the page contains. Example: 'Loader Mods'").arg(displayName()),
        m_fileSelectionFilter.arg(displayName()), APPLICATION->settings()->get("CentralModsDir").toString(), this->parentWidget());

    if (!list.isEmpty()) {
        for (auto filename : list) {
            m_model->installResource(filename);
        }
    }
}

void ExternalResourcesPage::removeItem()
{
    auto selection = m_filterModel->mapSelectionToSource(listSelectionModel()->selection());

    int count = 0;
    bool folder = false;
    for (auto& i : selection.indexes()) {
        if (i.column() == 0) {
            count++;

            // if a folder is selected, show the confirmation dialog
            if (m_model->at(i.row()).fileinfo().isDir())
                folder = true;
        }
    }

    QString text;
    bool multiple = count > 1;

    if (multiple) {
        text = tr("You are about to remove %1 items.\n"
                  "This may be permanent and they will be gone from the folder.\n\n"
                  "Are you sure?")
                   .arg(count);
    } else if (folder) {
        text = tr("You are about to remove the folder \"%1\".\n"
                  "This may be permanent and it will be gone from the parent folder.\n\n"
                  "Are you sure?")
                   .arg(m_model->at(selection.indexes().at(0).row()).fileinfo().fileName());
    }

    if (!text.isEmpty()) {
        auto response = CustomMessageBox::selectable(this, tr("Confirm Removal"), text, QMessageBox::Warning,
                                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes)
            return;
    }

    removeItems(selection);
}

void ExternalResourcesPage::removeItems(const QItemSelection& selection)
{
    if (m_instance != nullptr && m_instance->isRunning()) {
        auto response = CustomMessageBox::selectable(this, tr("Confirm Delete"),
                                                     tr("If you remove this resource while the game is running it may crash your game.\n"
                                                        "Are you sure you want to do this?"),
                                                     QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes)
            return;
    }
    m_model->deleteResources(selection.indexes());
}

void ExternalResourcesPage::enableItem()
{
    auto selection = m_filterModel->mapSelectionToSource(listSelectionModel()->selection());
    m_model->setResourceEnabled(selection.indexes(), EnableAction::ENABLE);
}

void ExternalResourcesPage::disableItem()
{
    auto selection = m_filterModel->mapSelectionToSource(listSelectionModel()->selection());
    m_model->setResourceEnabled(selection.indexes(), EnableAction::DISABLE);
}

void ExternalResourcesPage::viewHomepage()
{
    auto selection = m_filterModel->mapSelectionToSource(listSelectionModel()->selection()).indexes();
    for (auto resource : m_model->selectedResources(selection)) {
        auto url = resource->homepage();
        if (!url.isEmpty())
            DesktopServices::openUrl(url);
    }
}

void ExternalResourcesPage::viewConfigs()
{
    DesktopServices::openPath(m_instance->instanceConfigFolder(), true);
}

void ExternalResourcesPage::viewFolder()
{
    DesktopServices::openPath(m_model->dir().absolutePath(), true);
}

void ExternalResourcesPage::updateActions()
{
    const bool hasSelection = listSelectionModel() && listSelectionModel()->hasSelection();
    const QModelIndexList selection = m_filterModel->mapSelectionToSource(listSelectionModel()->selection()).indexes();
    const QList<Resource*> selectedResources = m_model->selectedResources(selection);

    ui->actionUpdateItem->setEnabled(!m_model->empty());
    ui->actionResetItemMetadata->setEnabled(hasSelection);

    ui->actionChangeVersion->setEnabled(selectedResources.size() == 1 && selectedResources[0]->metadata() != nullptr);

    ui->actionRemoveItem->setEnabled(hasSelection);
    ui->actionEnableItem->setEnabled(hasSelection);
    ui->actionDisableItem->setEnabled(hasSelection);

    ui->actionViewHomepage->setEnabled(hasSelection && std::any_of(selectedResources.begin(), selectedResources.end(),
                                                                   [](Resource* resource) { return !resource->homepage().isEmpty(); }));
    ui->actionExportMetadata->setEnabled(!m_model->empty());
}

void ExternalResourcesPage::updateFrame(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    auto sourceCurrent = m_filterModel->mapToSource(current);
    int row = sourceCurrent.row();
    Resource const& resource = m_model->at(row);
    ui->frame->updateWithResource(resource);
}

QString ExternalResourcesPage::extraHeaderInfoString()
{
    if (listView() && listView()->selectionModel()) {
        auto selection = m_filterModel->mapSelectionToSource(listSelectionModel()->selection()).indexes();
        if (auto count = std::count_if(selection.cbegin(), selection.cend(), [](auto v) { return v.column() == 0; }); count != 0)
            return tr(" (%1 installed, %2 selected)").arg(m_model->size()).arg(count);
    }
    return tr(" (%1 installed)").arg(m_model->size());
}

QAbstractItemView* ExternalResourcesPage::listView() const
{
    if (m_resourceViewMode == InstanceViewMode::Icons && m_iconView) {
        return m_iconView;
    }
    return ui->treeView;
}

QItemSelectionModel* ExternalResourcesPage::listSelectionModel() const
{
    auto* view = listView();
    return view ? view->selectionModel() : nullptr;
}

void ExternalResourcesPage::setZoomLevel(int level, bool save)
{
    m_zoomLevel = qBound(ZoomLevelInfo::minimumLevel(), level, ZoomLevelInfo::maximumLevel());
    const int iconSize = ZoomLevelInfo::iconSizeForZoomLevel(m_zoomLevel);

    if (auto* delegate = qobject_cast<ResourceIconDelegate*>(m_iconView ? m_iconView->itemDelegate() : nullptr)) {
        delegate->setIconSize(iconSize);
    }
    if (m_iconView) {
        m_iconView->setIconSize(QSize(iconSize, iconSize));
        m_iconView->setGridSize(QSize(iconSize + 48, iconSize + 44));
        m_iconView->viewport()->update();
    }
    if (m_zoomSlider) {
        m_zoomSlider->setZoomLevel(m_zoomLevel);
    }

    if (save) {
        APPLICATION->settings()->set("ResourceIconZoomLevel", m_zoomLevel);
    }
}

void ExternalResourcesPage::setResourceViewMode(InstanceViewMode mode)
{
    m_resourceViewMode = mode;
    APPLICATION->settings()->set("ResourceListViewMode", instanceViewModeToString(mode));

    if (m_viewStack) {
        m_viewStack->setCurrentWidget(mode == InstanceViewMode::Icons ? static_cast<QWidget*>(m_iconView)
                                                                       : static_cast<QWidget*>(ui->treeView));
    }
    if (m_actionListViewIcons && m_actionListViewDetails) {
        m_actionListViewIcons->setChecked(mode == InstanceViewMode::Icons);
        m_actionListViewDetails->setChecked(mode == InstanceViewMode::Details);
    }
    if (m_viewModeAction) {
        m_viewModeAction->setIcon(QIcon::fromTheme(mode == InstanceViewMode::Icons ? QStringLiteral("view-list-icons")
                                                                                   : QStringLiteral("view-list-tree")));
    }
    if (auto* view = listView()) {
        view->setFocus();
    }
}

void ExternalResourcesPage::syncListSelection(QItemSelectionModel* from, QItemSelectionModel* to)
{
    if (m_syncing_selection || !from || !to || !m_filterModel) {
        return;
    }
    m_syncing_selection = true;

    // collect the unique rows of the current selection and re-select them as full rows
    QList<int> rows;
    const int colCount = m_filterModel->columnCount();
    for (const QModelIndex& index : from->selection().indexes()) {
        if (!rows.contains(index.row())) {
            rows.append(index.row());
        }
    }
    QItemSelection selection;
    for (int row : rows) {
        selection.select(m_filterModel->index(row, 0), m_filterModel->index(row, colCount - 1));
    }
    to->select(selection, QItemSelectionModel::ClearAndSelect);

    m_syncing_selection = false;
}

void ExternalResourcesPage::refreshExtraHeader()
{
    if (updateExtraInfo) {
        updateExtraInfo(id(), extraHeaderInfoString());
    }
}
