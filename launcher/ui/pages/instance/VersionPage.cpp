// SPDX-FileCopyrightText: 2022-2023 Sefa Eyeoglu <contact@scrumplex.net>
//
// SPDX-License-Identifier: GPL-3.0-only AND Apache-2.0

/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *  Copyright (C) 2022-2023 Sefa Eyeoglu <contact@scrumplex.net>
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
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

#include "Application.h"

#include <QAbstractItemModel>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QHeaderView>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QString>
#include <QUrl>
#include <QActionGroup>
#include <QItemSelectionModel>
#include <algorithm>

#include "QObjectPtr.h"
#include "VersionPage.h"
#include "meta/JsonFormat.h"
#include "tasks/SequentialTask.h"
#include "ui/dialogs/InstallLoaderDialog.h"
#include "ui_VersionPage.h"

#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/NewComponentDialog.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/VersionSelectDialog.h"

#include "ui/GuiUtil.h"
#include "ui/widgets/ResourceIconDelegate.h"
#include "ui/widgets/ZoomSlider.h"
#include <QToolButton>

#include "DesktopServices.h"
#include "Exception.h"
#include "icons/IconList.h"
#include "minecraft/PackProfile.h"
#include "minecraft/auth/AccountList.h"

#include "meta/Index.h"
#include "meta/VersionList.h"

class IconProxy : public QIdentityProxyModel {
    Q_OBJECT
   public:
    IconProxy(QWidget* parentWidget) : QIdentityProxyModel(parentWidget)
    {
        connect(parentWidget, &QObject::destroyed, this, &IconProxy::widgetGone);
        m_parentWidget = parentWidget;
    }

    virtual QVariant data(const QModelIndex& proxyIndex, int role = Qt::DisplayRole) const override
    {
        QVariant var = QIdentityProxyModel::data(proxyIndex, role);
        int column = proxyIndex.column();
        if (column == 0 && role == Qt::DecorationRole && m_parentWidget) {
            if (!var.isNull()) {
                auto string = var.toString();
                if (string == "warning") {
                    return QIcon::fromTheme("status-yellow");
                } else if (string == "error") {
                    return QIcon::fromTheme("status-bad");
                }
            }
            return QIcon::fromTheme("status-good");
        }
        return var;
    }
   private slots:
    void widgetGone() { m_parentWidget = nullptr; }

   private:
    QWidget* m_parentWidget = nullptr;
};

QIcon VersionPage::icon() const
{
    return APPLICATION->icons()->getIcon(m_inst->iconKey());
}
bool VersionPage::shouldDisplay() const
{
    return true;
}

void VersionPage::retranslate()
{
    ui->retranslateUi(this);
}

void VersionPage::openedImpl()
{
    auto const setting_name = QString("WideBarVisibility_%1").arg(id());
    m_wide_bar_setting = APPLICATION->settings()->getOrRegisterSetting(setting_name);

    ui->toolBar->setVisibilityState(QByteArray::fromBase64(m_wide_bar_setting->get().toString().toUtf8()));
}
void VersionPage::closedImpl()
{
    m_wide_bar_setting->set(QString::fromUtf8(ui->toolBar->getVisibilityState().toBase64()));
}

QMenu* VersionPage::createPopupMenu()
{
    QMenu* filteredMenu = QMainWindow::createPopupMenu();
    filteredMenu->removeAction(ui->toolBar->toggleViewAction());
    return filteredMenu;
}

VersionPage::VersionPage(MinecraftInstance* inst, QWidget* parent) : QMainWindow(parent), ui(new Ui::VersionPage), m_inst(inst)
{
    ui->setupUi(this);

    ui->toolBar->insertSpacer(ui->actionReload);

    m_profile = m_inst->getPackProfile();

    reloadPackProfile();

    auto proxy = new IconProxy(ui->packageView);
    proxy->setSourceModel(m_profile);

    m_filterModel = new QSortFilterProxyModel(this);
    m_filterModel->setDynamicSortFilter(true);
    m_filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_filterModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_filterModel->setSourceModel(proxy);
    m_filterModel->setFilterKeyColumn(-1);

    ui->packageView->setModel(m_filterModel);
    ui->packageView->installEventFilter(this);
    ui->packageView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->packageView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->packageView->setContextMenuPolicy(Qt::CustomContextMenu);

    // Icon grid view, stacked with the table (modeled after Dolphin's view mode switching).
    // Both views share the same proxy model and keep their selections in sync.
    m_iconView = new QListView(this);
    m_iconView->setViewMode(QListView::IconMode);
    m_iconView->setIconSize(QSize(64, 64));
    m_iconView->setGridSize(QSize(112, 108));
    m_iconView->setSpacing(6);
    m_iconView->setResizeMode(QListView::Adjust);
    m_iconView->setMovement(QListView::Static);
    m_iconView->setUniformItemSizes(true);
    m_iconView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_iconView->setFrameShape(QFrame::NoFrame);
    m_iconView->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_iconView->setItemDelegate(new ResourceIconDelegate(0, 0, m_iconView, /*checkColumn=*/0, /*badgeColumn=*/-1));
    m_iconView->setModel(m_filterModel);
    m_iconView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_iconView, &QWidget::customContextMenuRequested, this, &VersionPage::showContextMenu);
    connect(m_iconView, &QAbstractItemView::activated, this, [this](const QModelIndex& index) {
        auto sourceIndex = m_filterModel->mapToSource(index);
        auto component = m_profile->getComponent(sourceIndex.row());
        component->setEnabled(!component->isEnabled());
    });

    // replace packageView's cell with a stack of the table and the grid
    auto* verticalLayout = ui->verticalLayout;
    verticalLayout->removeWidget(ui->packageView);
    m_viewStack = new QStackedWidget(this);
    m_viewStack->addWidget(ui->packageView);
    m_viewStack->addWidget(m_iconView);
    verticalLayout->insertWidget(0, m_viewStack);

    // keep the selections of the two views in sync (same proxy model)
    connect(ui->packageView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { syncSelection(ui->packageView->selectionModel(), m_iconView->selectionModel()); });
    connect(m_iconView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this] { syncSelection(m_iconView->selectionModel(), ui->packageView->selectionModel()); });

    auto smodel = ui->packageView->selectionModel();
    connect(smodel, &QItemSelectionModel::currentChanged, this, &VersionPage::versionCurrent);
    connect(smodel, &QItemSelectionModel::currentChanged, this, &VersionPage::packageCurrent);
    connect(m_iconView->selectionModel(), &QItemSelectionModel::currentChanged, this, &VersionPage::versionCurrent);
    connect(m_iconView->selectionModel(), &QItemSelectionModel::currentChanged, this, &VersionPage::packageCurrent);
    connect(m_profile, &PackProfile::minecraftChanged, this, &VersionPage::updateVersionControls);
    updateVersionControls();
    preselect(0);
    connect(ui->packageView, &ModListView::customContextMenuRequested, this, &VersionPage::showContextMenu);
    connect(ui->packageView, &QAbstractItemView::activated, this, [this](const QModelIndex& index) {
        auto sourceIndex = m_filterModel->mapToSource(index);
        auto component = m_profile->getComponent(sourceIndex.row());
        component->setEnabled(!component->isEnabled());
    });
    connect(ui->filterEdit, &QLineEdit::textChanged, this, &VersionPage::onFilterTextChanged);

    // view mode switcher, modeled after Dolphin's view_mode action; the mode is
    // persisted globally in the ComponentListViewMode setting
    m_actionListViewIcons = new QAction(tr("Icons"), this);
    m_actionListViewIcons->setCheckable(true);
    m_actionListViewIcons->setToolTip(tr("Show items as a grid of icons"));
    m_actionListViewDetails = new QAction(tr("Details"), this);
    m_actionListViewDetails->setCheckable(true);
    m_actionListViewDetails->setToolTip(tr("Show items as a table"));
    auto* viewModeGroup = new QActionGroup(this);
    viewModeGroup->addAction(m_actionListViewIcons);
    viewModeGroup->addAction(m_actionListViewDetails);
    connect(m_actionListViewIcons, &QAction::triggered, this, [this] { setViewMode(InstanceViewMode::Icons); });
    connect(m_actionListViewDetails, &QAction::triggered, this, [this] { setViewMode(InstanceViewMode::Details); });

    auto* viewModeMenu = new QMenu(this);
    viewModeMenu->addAction(m_actionListViewIcons);
    viewModeMenu->addAction(m_actionListViewDetails);
    m_viewModeAction = new QAction(tr("View Mode"), this);
    m_viewModeAction->setMenu(viewModeMenu);
    m_viewModeAction->setToolTip(tr("Change view mode"));
    ui->toolBar->insertActionBefore(ui->actionChange_version, m_viewModeAction);
    if (auto* button = qobject_cast<QToolButton*>(ui->toolBar->widgetForAction(m_viewModeAction))) {
        button->setPopupMode(QToolButton::InstantPopup);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setStyleSheet(QStringLiteral("QToolButton { text-align: center; }"));
    }

    // zoom actions, modeled after Dolphin's zoom_in/zoom_out actions;
    // the slider itself lives in the status bar, like Dolphin's zoom slider
    const auto zoomActions = createZoomToolbarActions(this);
    m_actionZoomIn = zoomActions.zoomIn;
    m_actionZoomOut = zoomActions.zoomOut;
    m_actionZoomReset = zoomActions.zoomReset;
    connect(m_actionZoomIn, &QAction::triggered, this, [this] { setZoomLevel(m_zoomLevel + 1); });
    connect(m_actionZoomOut, &QAction::triggered, this, [this] { setZoomLevel(m_zoomLevel - 1); });
    connect(m_actionZoomReset, &QAction::triggered, this, [this] { setZoomLevel(4); });
    m_actionZoomOut->setIcon(QIcon());
    m_actionZoomIn->setIcon(QIcon());
    ui->toolBar->insertActionBefore(m_viewModeAction, m_actionZoomOut);
    ui->toolBar->insertActionBefore(m_viewModeAction, m_actionZoomIn);
    for (QAction* action : {m_actionZoomOut, m_actionZoomIn}) {
        if (auto* button = qobject_cast<QToolButton*>(ui->toolBar->widgetForAction(action))) {
            button->setToolButtonStyle(Qt::ToolButtonTextOnly);
            button->setStyleSheet(QStringLiteral("QToolButton { text-align: center; }"));
        }
    }
    this->addAction(m_actionZoomReset);  // widget-level, shortcut-only action (no toolbar button)

    auto* zoomSlider = new ZoomStatusSlider(this);
    m_zoomSlider = zoomSlider;
    connect(zoomSlider, &ZoomStatusSlider::zoomLevelChanged, this, [this](int level) { setZoomLevel(level); });
    setStatusBar(new QStatusBar(this));
    statusBar()->addPermanentWidget(zoomSlider);

    // apply the saved view mode (table or icon grid) and zoom level of the grid
    setViewMode(instanceViewModeFromString(APPLICATION->settings()->get("ComponentListViewMode").toString()));
    setZoomLevel(APPLICATION->settings()->get("ComponentIconZoomLevel").toInt(), /*save=*/false);
}

VersionPage::~VersionPage()
{
    delete ui;
}

void VersionPage::showContextMenu(const QPoint& pos)
{
    auto menu = ui->toolBar->createContextMenu(this, tr("Context menu"));
    auto* view = qobject_cast<QWidget*>(sender());
    menu->exec(view ? view->mapToGlobal(pos) : mapToGlobal(pos));
    delete menu;
}

void VersionPage::packageCurrent(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    if (!current.isValid()) {
        ui->frame->clear();
        return;
    }
    int row = m_filterModel->mapToSource(current).row();
    auto patch = m_profile->getComponent(row);
    auto severity = patch->getProblemSeverity();
    switch (severity) {
        case ProblemSeverity::Warning:
            ui->frame->setName(tr("%1 possibly has issues.").arg(patch->getName()));
            break;
        case ProblemSeverity::Error:
            ui->frame->setName(tr("%1 has issues!").arg(patch->getName()));
            break;
        default:
        case ProblemSeverity::None:
            ui->frame->clear();
            return;
    }

    auto& problems = patch->getProblems();
    QString problemOut;
    for (auto& problem : problems) {
        if (problem.m_severity == ProblemSeverity::Error) {
            problemOut += tr("Error: ");
        } else if (problem.m_severity == ProblemSeverity::Warning) {
            problemOut += tr("Warning: ");
        }
        problemOut += problem.m_description;
        problemOut += "\n";
    }
    ui->frame->setDescription(problemOut);
}

void VersionPage::updateVersionControls()
{
    updateButtons();
}

void VersionPage::updateButtons(int row)
{
    if (row == -1)
        row = currentRow();
    auto patch = m_profile->getComponent(row);
    ui->actionRemove->setEnabled(patch && patch->isRemovable());
    ui->actionMove_down->setEnabled(patch && patch->isMoveable());
    ui->actionMove_up->setEnabled(patch && patch->isMoveable());
    ui->actionChange_version->setEnabled(patch && patch->isVersionChangeable(false));
    ui->actionEdit->setEnabled(patch && patch->isCustom());
    ui->actionCustomize->setEnabled(patch && patch->isCustomizable());
    ui->actionRevert->setEnabled(patch && patch->isRevertible());
}

bool VersionPage::reloadPackProfile()
{
    try {
        auto result = m_profile->reload(Net::Mode::Online);
        if (!result) {
            QMessageBox::critical(this, tr("Error"), result.error);
        }
        return result;
    } catch (const Exception& e) {
        QMessageBox::critical(this, tr("Error"), e.cause());
        return false;
    } catch (...) {
        QMessageBox::critical(this, tr("Error"), tr("Couldn't load the instance profile."));
        return false;
    }
}

void VersionPage::on_actionReload_triggered()
{
    reloadPackProfile();
    m_container->refreshContainer();
}

void VersionPage::on_actionRemove_triggered()
{
    if (!currentView()->currentIndex().isValid()) {
        return;
    }
    int index = m_filterModel->mapToSource(currentView()->currentIndex()).row();
    auto component = m_profile->getComponent(index);
    if (component->isCustom()) {
        auto response = CustomMessageBox::selectable(this, tr("Confirm Removal"),
                                                     tr("You are about to remove \"%1\".\n"
                                                        "This is permanent and will completely remove the custom component.\n\n"
                                                        "Are you sure?")
                                                         .arg(component->getName()),
                                                     QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes)
            return;
    }
    // FIXME: use actual model, not reloading.
    if (!m_profile->remove(index)) {
        QMessageBox::critical(this, tr("Error"), tr("Couldn't remove file"));
    }
    updateButtons();
    reloadPackProfile();
    m_container->refreshContainer();
}

void VersionPage::on_actionAdd_to_Minecraft_jar_triggered()
{
    auto list = GuiUtil::BrowseForFiles("jarmod", tr("Select jar mods"), tr("Minecraft.jar mods") + " (*.zip *.jar)",
                                        APPLICATION->settings()->get("CentralModsDir").toString(), this->parentWidget());
    if (!list.empty()) {
        m_profile->installJarMods(list);
    }
    updateButtons();
}

void VersionPage::on_actionReplace_Minecraft_jar_triggered()
{
    auto jarPath = GuiUtil::BrowseForFile("jar", tr("Select jar"), tr("Minecraft.jar replacement") + " (*.jar)",
                                          APPLICATION->settings()->get("CentralModsDir").toString(), this->parentWidget());
    if (!jarPath.isEmpty()) {
        m_profile->installCustomJar(jarPath);
    }
    updateButtons();
}

void VersionPage::on_actionImport_Components_triggered()
{
    QStringList list = GuiUtil::BrowseForFiles("component", tr("Select components"), tr("Components") + " (*.json)",
                                               APPLICATION->settings()->get("CentralModsDir").toString(), this->parentWidget());

    if (!list.isEmpty()) {
        if (!m_profile->installComponents(list)) {
            QMessageBox::warning(this, tr("Failed to import components"),
                                 tr("Some components could not be imported. Check logs for details"));
        }
    }

    updateButtons();
}

void VersionPage::on_actionAdd_Agents_triggered()
{
    QStringList list = GuiUtil::BrowseForFiles("agent", tr("Select agents"), tr("Java agents") + " (*.jar)",
                                               APPLICATION->settings()->get("CentralModsDir").toString(), this->parentWidget());

    if (!list.isEmpty())
        m_profile->installAgents(list);

    updateButtons();
}

void VersionPage::on_actionMove_up_triggered()
{
    try {
        m_profile->move(currentRow(), PackProfile::MoveUp);
    } catch (const Exception& e) {
        QMessageBox::critical(this, tr("Error"), e.cause());
    }
    updateButtons();
}

void VersionPage::on_actionMove_down_triggered()
{
    try {
        m_profile->move(currentRow(), PackProfile::MoveDown);
    } catch (const Exception& e) {
        QMessageBox::critical(this, tr("Error"), e.cause());
    }
    updateButtons();
}

void VersionPage::on_actionChange_version_triggered()
{
    auto versionRow = currentRow();
    if (versionRow == -1) {
        return;
    }
    auto patch = m_profile->getComponent(versionRow);
    auto name = patch->getName();
    auto list = patch->getVersionList();
    list->clearExternalRecommends();
    if (!list) {
        return;
    }
    auto uid = list->uid();

    // recommend the correct lwjgl version for the current minecraft version
    if (uid == "org.lwjgl" || uid == "org.lwjgl3") {
        auto minecraft = m_profile->getComponent("net.minecraft");
        auto lwjglReq = std::find_if(minecraft->m_cachedRequires.cbegin(), minecraft->m_cachedRequires.cend(),
                                     [uid](const Meta::Require& req) -> bool { return req.uid == uid; });
        if (lwjglReq != minecraft->m_cachedRequires.cend()) {
            auto lwjglVersion = !lwjglReq->equalsVersion.isEmpty() ? lwjglReq->equalsVersion : lwjglReq->suggests;
            if (!lwjglVersion.isEmpty()) {
                list->addExternalRecommends({ lwjglVersion });
            }
        }
    }

    VersionSelectDialog vselect(list.get(), tr("Change %1 version").arg(name), this);
    if (uid == "net.fabricmc.intermediary" || uid == "org.quiltmc.hashed") {
        vselect.setEmptyString(tr("No intermediary mappings versions are currently available."));
        vselect.setEmptyErrorString(tr("Couldn't load or download the intermediary mappings version lists!"));
    }
    vselect.setExactIfPresentFilter(BaseVersionList::ParentVersionRole, m_profile->getComponentVersion("net.minecraft"));

    auto currentVersion = patch->getVersion();
    if (!currentVersion.isEmpty()) {
        vselect.setCurrentVersion(currentVersion);
    }
    if (!vselect.exec() || !vselect.selectedVersion())
        return;

    qDebug() << "Change" << uid << "to" << vselect.selectedVersion()->descriptor();
    bool important = false;
    if (uid == "net.minecraft") {
        important = true;
        if (APPLICATION->settings()->get("AutomaticJavaSwitch").toBool() && m_inst->settings()->get("AutomaticJava").toBool() &&
            m_inst->settings()->get("OverrideJavaLocation").toBool()) {
            m_inst->settings()->set("OverrideJavaLocation", false);
            m_inst->settings()->set("JavaPath", "");
        }
    }
    m_profile->setComponentVersion(uid, vselect.selectedVersion()->descriptor(), important);
    m_profile->resolve(Net::Mode::Online);
    m_container->refreshContainer();
}

void VersionPage::on_actionDownload_All_triggered()
{
    auto updateTasks = m_inst->createUpdateTask();
    if (updateTasks.isEmpty()) {
        return;
    }
    auto task = makeShared<SequentialTask>();
    for (auto t : updateTasks) {
        task->addTask(t);
    }
    ProgressDialog tDialog(this);
    connect(task.get(), &Task::failed, this, &VersionPage::onGameUpdateError);
    // FIXME: unused return value
    tDialog.execWithTask(task.get());
    updateButtons();
    m_container->refreshContainer();
}

void VersionPage::on_actionInstall_Loader_triggered()
{
    InstallLoaderDialog dialog(m_inst->getPackProfile(), QString(), this);
    dialog.exec();
    m_container->refreshContainer();
}

void VersionPage::on_actionAdd_Empty_triggered()
{
    NewComponentDialog compdialog(QString(), QString(), this);
    QStringList blacklist;
    for (int i = 0; i < m_profile->rowCount(); i++) {
        auto comp = m_profile->getComponent(i);
        blacklist.push_back(comp->getID());
    }
    compdialog.setBlacklist(blacklist);
    if (compdialog.exec()) {
        qDebug() << "name:" << compdialog.name();
        qDebug() << "uid:" << compdialog.uid();
        m_profile->installEmpty(compdialog.uid(), compdialog.name());
    }
}

void VersionPage::on_actionLibrariesFolder_triggered()
{
    DesktopServices::openPath(m_inst->getLocalLibraryPath(), true);
}

void VersionPage::on_actionMinecraftFolder_triggered()
{
    DesktopServices::openPath(m_inst->gameRoot(), true);
}

void VersionPage::versionCurrent(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    currentIdx = current.isValid() ? m_filterModel->mapToSource(current).row() : -1;
    updateButtons(currentIdx);
}

void VersionPage::preselect(int row)
{
    if (row < 0) {
        row = 0;
    }
    if (row >= m_profile->rowCount(QModelIndex())) {
        row = m_profile->rowCount(QModelIndex()) - 1;
    }
    if (row < 0) {
        return;
    }
    auto model_index = m_profile->index(row);
    auto proxy_index = m_filterModel->mapFromSource(model_index);
    ui->packageView->selectionModel()->select(proxy_index, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    updateButtons(row);
}

void VersionPage::onGameUpdateError(QString error)
{
    CustomMessageBox::selectable(this, tr("Error updating instance"), error, QMessageBox::Warning)->show();
}

ComponentPtr VersionPage::current()
{
    auto row = currentRow();
    if (row < 0) {
        return nullptr;
    }
    return m_profile->getComponent(row);
}

int VersionPage::currentRow()
{
    if (currentView()->selectionModel()->selectedRows().isEmpty()) {
        return -1;
    }
    return m_filterModel->mapToSource(currentView()->selectionModel()->selectedRows().first()).row();
}

void VersionPage::on_actionCustomize_triggered()
{
    auto version = currentRow();
    if (version == -1) {
        return;
    }
    auto patch = m_profile->getComponent(version);
    if (!patch->getVersionFile()) {
        // TODO: wait for the update task to finish here...
        return;
    }
    if (!m_profile->customize(version)) {
        // TODO: some error box here
    }
    updateButtons();
    preselect(currentIdx);
}

void VersionPage::on_actionEdit_triggered()
{
    auto version = current();
    if (!version) {
        return;
    }
    auto filename = version->getFilename();
    if (!QFileInfo::exists(filename)) {
        qWarning() << "file" << filename << "can't be opened for editing, doesn't exist!";
        return;
    }
    APPLICATION->openJsonEditor(filename);
}

void VersionPage::on_actionRevert_triggered()
{
    auto version = currentRow();
    if (version == -1) {
        return;
    }
    auto component = m_profile->getComponent(version);

    auto response = CustomMessageBox::selectable(this, tr("Confirm Reversion"),
                                                 tr("You are about to revert \"%1\".\n"
                                                    "This is permanent and will completely revert your customizations.\n\n"
                                                    "Are you sure?")
                                                     .arg(component->getName()),
                                                 QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                        ->exec();

    if (response != QMessageBox::Yes)
        return;

    if (!m_profile->revertToBase(version)) {
        // TODO: some error box here
    }
    updateButtons();
    preselect(currentIdx);
    m_container->refreshContainer();
}

void VersionPage::onFilterTextChanged(const QString& newContents)
{
    m_filterModel->setFilterFixedString(newContents);
}

QAbstractItemView* VersionPage::currentView() const
{
    if (m_viewMode == InstanceViewMode::Icons && m_iconView) {
        return m_iconView;
    }
    return ui->packageView;
}

void VersionPage::syncSelection(QItemSelectionModel* from, QItemSelectionModel* to)
{
    if (m_syncing_selection || !from || !to) {
        return;
    }
    m_syncing_selection = true;

    // collect the unique rows of the current selection and re-select them
    // (column 0 only; SelectRows views still highlight the whole row, and this
    // keeps selectedIndexes().size() equal to the number of selected rows)
    QList<int> rows;
    const auto* model = from->model();
    for (const QModelIndex& index : from->selection().indexes()) {
        if (!rows.contains(index.row())) {
            rows.append(index.row());
        }
    }
    QItemSelection selection;
    for (int row : rows) {
        selection.select(model->index(row, 0), model->index(row, 0));
    }
    to->select(selection, QItemSelectionModel::ClearAndSelect);

    m_syncing_selection = false;
}

void VersionPage::setViewMode(InstanceViewMode mode)
{
    m_viewMode = mode;
    APPLICATION->settings()->set("ComponentListViewMode", instanceViewModeToString(mode));

    if (m_viewStack) {
        m_viewStack->setCurrentWidget(mode == InstanceViewMode::Icons ? static_cast<QWidget*>(m_iconView)
                                                                       : static_cast<QWidget*>(ui->packageView));
    }
    if (m_actionListViewIcons && m_actionListViewDetails) {
        m_actionListViewIcons->setChecked(mode == InstanceViewMode::Icons);
        m_actionListViewDetails->setChecked(mode == InstanceViewMode::Details);
    }
    if (m_viewModeAction) {
        m_viewModeAction->setIcon(QIcon());
    }
    if (auto* view = currentView()) {
        view->setFocus();
    }
}

void VersionPage::setZoomLevel(int level, bool save)
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
        APPLICATION->settings()->set("ComponentIconZoomLevel", m_zoomLevel);
    }
}

#include "VersionPage.moc"
