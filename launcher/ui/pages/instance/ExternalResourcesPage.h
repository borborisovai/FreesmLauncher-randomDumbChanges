#pragma once

#include <QMainWindow>
#include <QSortFilterProxyModel>

#include "Application.h"
#include "minecraft/MinecraftInstance.h"
#include "settings/Setting.h"
#include "ui/instanceview/InstanceViewMode.h"
#include "ui/pages/BasePage.h"

class ResourceFolderModel;
class QStackedWidget;
class QListView;
class QAction;
class ZoomStatusSlider;

namespace Ui {
class ExternalResourcesPage;
}

/* This page is used as a base for pages in which the user can manage external resources
 * related to the game, such as mods, shaders or resource packs. */
class ExternalResourcesPage : public QMainWindow, public BasePage {
    Q_OBJECT

   public:
    explicit ExternalResourcesPage(BaseInstance* instance, ResourceFolderModel* model, QWidget* parent = nullptr);
    virtual ~ExternalResourcesPage();

    virtual QString displayName() const override = 0;
    virtual QIcon icon() const override = 0;
    virtual QString id() const override = 0;
    virtual QString helpPage() const override = 0;

    virtual bool shouldDisplay() const override = 0;
    QString extraHeaderInfoString();

    void openedImpl() override;
    void closedImpl() override;

    void retranslate() override;

   protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;
    bool listFilter(QKeyEvent* ev);
    QMenu* createPopupMenu() override;

   public slots:
    virtual void updateActions();
    virtual void updateFrame(const QModelIndex& current, const QModelIndex& previous);

    /// switch between the table (details) and the icon grid (modeled after Dolphin's view mode switching);
    /// the mode is persisted globally in the ResourceListViewMode setting
    void setResourceViewMode(InstanceViewMode mode);

    /// set the icon size level (0-16) of the icon grid (modeled after Dolphin's zoom);
    /// persisted globally in the ResourceIconZoomLevel setting unless \a save is false
    void setZoomLevel(int level, bool save = true);

   protected:
    /// the list view currently shown (table or icon grid)
    QAbstractItemView* listView() const;
    QItemSelectionModel* listSelectionModel() const;

   protected slots:
    void itemActivated(const QModelIndex& index);
    void filterTextChanged(const QString& newContents);

    virtual void addItem();
    void removeItem();
    virtual void removeItems(const QItemSelection& selection);

    virtual void enableItem();
    virtual void disableItem();

    virtual void viewHomepage();

    virtual void viewFolder();
    virtual void viewConfigs();

    void ShowContextMenu(const QPoint& pos);
    void ShowHeaderContextMenu(const QPoint& pos);

   protected:
    BaseInstance* m_instance = nullptr;

    Ui::ExternalResourcesPage* ui = nullptr;
    ResourceFolderModel* m_model;
    QSortFilterProxyModel* m_filterModel = nullptr;

    QString m_fileSelectionFilter;
    QString m_viewFilter;

    std::shared_ptr<Setting> m_wide_bar_setting = nullptr;

    QStackedWidget* m_viewStack = nullptr;
    QListView* m_iconView = nullptr;
    InstanceViewMode m_resourceViewMode = InstanceViewMode::Details;
    QAction* m_actionListViewIcons = nullptr;
    QAction* m_actionListViewDetails = nullptr;
    QAction* m_viewModeAction = nullptr;
    QAction* m_actionZoomIn = nullptr;
    QAction* m_actionZoomOut = nullptr;
    QAction* m_actionZoomReset = nullptr;
    ZoomStatusSlider* m_zoomSlider = nullptr;
    int m_zoomLevel = 4;
    /// re-entrancy guard for keeping the two views' selections in sync
    bool m_syncing_selection = false;

   private:
    void syncListSelection(QItemSelectionModel* from, QItemSelectionModel* to);
    void refreshExtraHeader();
};
