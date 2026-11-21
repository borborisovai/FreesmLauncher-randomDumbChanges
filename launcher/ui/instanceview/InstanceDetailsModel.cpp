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

#include "InstanceDetailsModel.h"

#include <Application.h>
#include <InstanceList.h>
#include <MMCTime.h>
#include "BaseInstance.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/Component.h"
#include "minecraft/PackProfile.h"
#include "InstanceView.h"

#include <QDateTime>
#include <QLocale>
#include <QStringList>

InstanceDetailsModel::InstanceDetailsModel(QObject* parent) : InstanceProxyModel(parent) {}

int InstanceDetailsModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColumnCount;
}

void InstanceDetailsModel::setSourceModel(QAbstractItemModel* sourceModel)
{
    if (this->sourceModel()) {
        disconnect(this->sourceModel(), &QAbstractItemModel::dataChanged, this, &InstanceDetailsModel::sourceDataChanged);
    }
    InstanceProxyModel::setSourceModel(sourceModel);
    if (sourceModel) {
        connect(sourceModel, &QAbstractItemModel::dataChanged, this, &InstanceDetailsModel::sourceDataChanged);
    }
}

static BaseInstance* instanceForIndex(const QAbstractProxyModel* proxy, const QModelIndex& index)
{
    const QModelIndex src = proxy->mapToSource(index.sibling(index.row(), InstanceDetailsModel::ColumnName));
    return src.isValid() ? static_cast<BaseInstance*>(src.internalPointer()) : nullptr;
}

static QString columnText(int column, BaseInstance* instance)
{
    if (!instance) {
        return {};
    }
    switch (column) {
        case InstanceDetailsModel::ColumnName:
            return instance->name();
        case InstanceDetailsModel::ColumnVersion: {
            const QString version = instance->getVersionString();
            return version.isEmpty() ? QStringLiteral("—") : version;
        }
        case InstanceDetailsModel::ColumnLoader: {
            auto* minecraft = dynamic_cast<MinecraftInstance*>(instance);
            if (!minecraft) {
                return InstanceDetailsModel::tr("—");
            }
            QStringList loaders;
            const auto* profile = minecraft->getPackProfile();
            if (!profile) {
                return InstanceDetailsModel::tr("Vanilla");
            }
            for (auto it = Component::KNOWN_MODLOADERS.cbegin(); it != Component::KNOWN_MODLOADERS.cend(); ++it) {
                const QString version = profile->getComponentVersion(it.key());
                if (!version.isEmpty()) {
                    QString name;
                    if (it.key() == QStringLiteral("net.neoforged")) name = QStringLiteral("NeoForge");
                    else if (it.key() == QStringLiteral("net.minecraftforge")) name = QStringLiteral("Forge");
                    else if (it.key() == QStringLiteral("net.fabricmc.fabric-loader")) name = QStringLiteral("Fabric");
                    else if (it.key() == QStringLiteral("org.quiltmc.quilt-loader")) name = QStringLiteral("Quilt");
                    else name = QStringLiteral("LiteLoader");
                    loaders.append(QStringLiteral("%1 %2").arg(name, version));
                }
            }
            return loaders.isEmpty() ? InstanceDetailsModel::tr("Vanilla") : loaders.join(QStringLiteral(", "));
        }
        case InstanceDetailsModel::ColumnGroup: {
            const QString group = APPLICATION->instances()->getInstanceGroup(instance->id());
            return group.isEmpty() ? InstanceDetailsModel::tr("Ungrouped") : group;
        }
        case InstanceDetailsModel::ColumnLastLaunched: {
            const qint64 lastLaunch = instance->lastLaunch();
            if (lastLaunch <= 0) {
                return InstanceDetailsModel::tr("Never");
            }
            return QLocale().toString(QDateTime::fromMSecsSinceEpoch(lastLaunch), QLocale::ShortFormat);
        }
        case InstanceDetailsModel::ColumnPlaytime: {
            const int64_t played = instance->totalTimePlayed();
            if (played <= 0) {
                return QStringLiteral("-");
            }
            return Time::prettifyDuration(played, APPLICATION->settings()->get("ShowGameTimeWithoutDays").toBool());
        }
        default:
            return {};
    }
}

QVariant InstanceDetailsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.column() >= ColumnCount) {
        return {};
    }

    BaseInstance* instance = instanceForIndex(this, index);

    switch (role) {
        case InstanceList::InstancePointerRole:
            return QVariant::fromValue(static_cast<void*>(instance));
        case InstanceList::InstanceIDRole:
            return instance ? instance->id() : QVariant();
        case InstanceViewRoles::GroupRole:
            return instance ? APPLICATION->instances()->getInstanceGroup(instance->id()) : QVariant();
        case Qt::DisplayRole:
            return columnText(index.column(), instance);
        case Qt::EditRole:
            return index.column() == ColumnName ? columnText(index.column(), instance) : QVariant();
        case Qt::DecorationRole:
            if (index.column() == ColumnName && instance) {
                return QVariant(APPLICATION->icons()->getIcon(instance->iconKey()));
            }
            return QVariant();
        case Qt::ToolTipRole:
            if (index.column() == ColumnName && instance) {
                return instance->instanceRoot();
            }
            return columnText(index.column(), instance);
        case Qt::AccessibleTextRole:
            if (index.column() == ColumnName && instance) {
                return tr("%1 Instance").arg(instance->name());
            }
            return columnText(index.column(), instance);
        case Qt::TextAlignmentRole:
            switch (index.column()) {
                case ColumnLastLaunched:
                case ColumnPlaytime:
                    return int(Qt::AlignRight | Qt::AlignVCenter);
                default:
                    return int(Qt::AlignLeft | Qt::AlignVCenter);
            }
        default:
            break;
    }
    return {};
}

QVariant InstanceDetailsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < ColumnCount) {
        switch (section) {
            case ColumnName:
                return tr("Name");
            case ColumnVersion:
                return tr("Version");
            case ColumnLoader:
                return tr("Loader");
            case ColumnGroup:
                return tr("Group");
            case ColumnLastLaunched:
                return tr("Last played");
            case ColumnPlaytime:
                return tr("Playtime");
            default:
                break;
        }
    }
    if (orientation == Qt::Vertical && role == Qt::DisplayRole) {
        return section + 1;
    }
    return {};
}

Qt::ItemFlags InstanceDetailsModel::flags(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.column() >= ColumnCount) {
        return Qt::NoItemFlags;
    }
    // only the name column is editable (inline rename, like in the grid view)
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == ColumnName) {
        flags |= Qt::ItemIsEditable;
    }
    return flags;
}

bool InstanceDetailsModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::EditRole || !index.isValid() || index.column() != ColumnName) {
        return false;
    }
    return sourceModel()->setData(mapToSource(index), value, role);
}

bool InstanceDetailsModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    auto* a = static_cast<BaseInstance*>(left.internalPointer());
    auto* b = static_cast<BaseInstance*>(right.internalPointer());
    if (!a || !b) {
        return InstanceProxyModel::lessThan(left, right);
    }

    switch (sortColumn()) {
        case ColumnName:
            return m_naturalSort.compare(a->name(), b->name()) < 0;
        case ColumnVersion:
            // natural sort so that, e.g., 1.9 comes before 1.10
            return m_naturalSort.compare(a->getVersionString(), b->getVersionString()) < 0;
        case ColumnLoader:
            return m_naturalSort.compare(columnText(ColumnLoader, a), columnText(ColumnLoader, b)) < 0;
        case ColumnGroup: {
            const int result = APPLICATION->instances()->getInstanceGroup(a->id()).localeAwareCompare(APPLICATION->instances()->getInstanceGroup(b->id()));
            if (result != 0) {
                return result < 0;
            }
            // stable secondary ordering by name within a group
            return m_naturalSort.compare(a->name(), b->name()) < 0;
        }
        case ColumnLastLaunched:
            return a->lastLaunch() < b->lastLaunch();
        case ColumnPlaytime:
            return a->totalTimePlayed() < b->totalTimePlayed();
        default:
            return InstanceProxyModel::lessThan(left, right);
    }
}

void InstanceDetailsModel::sourceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles)
{
    // The source model is single-column and rows are (potentially) reordered by this proxy,
    // so map every visible row back to its source row and notify per proxy row.
    for (int proxyRow = 0; proxyRow < rowCount(); ++proxyRow) {
        const QModelIndex src = mapToSource(index(proxyRow, 0));
        if (src.isValid() && src.row() >= topLeft.row() && src.row() <= bottomRight.row()) {
            emit dataChanged(index(proxyRow, 0), index(proxyRow, ColumnCount - 1), roles);
        }
    }
}
