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

#include "InstanceDetailsDelegate.h"

#include <QApplication>
#include <QFontMetrics>
#include <QLineEdit>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionFocusRect>

#include "BaseInstance.h"
#include "InstanceDetailsModel.h"
#include "InstanceList.h"
#include "InstanceStatusPainter.h"

constexpr int ICON_SIZE = 24;
constexpr int CELL_HEIGHT = ICON_SIZE + 8;  // 32 px rows
constexpr int CELL_MARGIN = 4;

InstanceDetailsDelegate::InstanceDetailsDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void InstanceDetailsDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (index.column() != InstanceDetailsModel::ColumnName) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    painter->save();
    painter->setClipRect(opt.rect);

    const bool selected = opt.state & QStyle::State_Selected;

    // selection background; left transparent otherwise so the alternating row colors show
    if (selected) {
        painter->fillRect(opt.rect, opt.palette.brush(QPalette::Highlight));
    }

    QIcon::Mode mode = QIcon::Normal;
    if (!(opt.state & QStyle::State_Enabled)) {
        mode = QIcon::Disabled;
    } else if (selected) {
        mode = QIcon::Selected;
    }
    QIcon::State state = opt.state & QStyle::State_Open ? QIcon::On : QIcon::Off;

    // icon, vertically centered at the left edge
    QRect iconRect = opt.rect;
    iconRect.setX(opt.rect.x() + CELL_MARGIN);
    iconRect.setWidth(ICON_SIZE);
    iconRect.setY(opt.rect.y() + (opt.rect.height() - ICON_SIZE) / 2);
    iconRect.setHeight(ICON_SIZE);
    opt.icon.paint(painter, iconRect, Qt::AlignCenter, mode, state);

    // text color
    QPalette::ColorGroup cg = opt.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
    if (cg == QPalette::Normal && !(opt.state & QStyle::State_Active)) {
        cg = QPalette::Inactive;
    }
    painter->setPen(selected ? opt.palette.color(cg, QPalette::HighlightedText) : opt.palette.color(cg, QPalette::Text));

    // leave room for the status badges (24 px each + spacing)
    auto* instance = static_cast<BaseInstance*>(index.data(InstanceList::InstancePointerRole).value<void*>());
    const int badgeRoom = instance ? statusBadgeCount(instance) * 25 : 0;

    const QString text = index.data(Qt::DisplayRole).toString();
    QRect textRect = opt.rect;
    textRect.setLeft(iconRect.right() + CELL_MARGIN + 2);
    textRect.setRight(opt.rect.right() - CELL_MARGIN - badgeRoom);

    const QFontMetrics fm(opt.font);
    const QString elided = fm.elidedText(text, Qt::ElideRight, qMax(0, textRect.width()));
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, elided);

    // status badges, anchored to the right edge of the name column
    drawStatusBadges(painter, opt.rect, instance, mode, state);

    // focus rectangle
    if ((opt.state & QStyle::State_HasFocus) && selected) {
        QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
        QStyleOptionFocusRect focusOpt;
        focusOpt.direction = opt.direction;
        focusOpt.fontMetrics = opt.fontMetrics;
        focusOpt.palette = opt.palette;
        focusOpt.rect = opt.rect;
        focusOpt.backgroundColor = opt.palette.color(QPalette::Highlight);
        style->drawPrimitive(QStyle::PE_FrameFocusRect, &focusOpt, painter, opt.widget);
    }

    painter->restore();
}

QSize InstanceDetailsDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (index.column() == InstanceDetailsModel::ColumnName) {
        return QSize(200, CELL_HEIGHT);
    }
    QSize base = QStyledItemDelegate::sizeHint(option, index);
    base.setHeight(CELL_HEIGHT);
    return base;
}

QWidget* InstanceDetailsDelegate::createEditor(QWidget* parent,
                                               const QStyleOptionViewItem& option,
                                               const QModelIndex& index) const
{
    Q_UNUSED(option);
    if (index.column() != InstanceDetailsModel::ColumnName) {
        return nullptr;
    }
    auto* editor = new QLineEdit(parent);
    editor->setMaxLength(128);
    return editor;
}

void InstanceDetailsDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    auto* lineEdit = qobject_cast<QLineEdit*>(editor);
    if (!lineEdit) {
        return;
    }
    lineEdit->setText(index.data(Qt::EditRole).toString());
    lineEdit->selectAll();
}

void InstanceDetailsDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    auto* lineEdit = qobject_cast<QLineEdit*>(editor);
    if (!lineEdit) {
        return;
    }
    QString text = lineEdit->text().trimmed();
    text.replace(QChar('\n'), QChar(' '));
    text = text.trimmed();
    // Prevent instance names longer than 128 chars
    text.truncate(128);
    if (text.size() != 0) {
        emit textChanged(model->data(index).toString(), text);
        model->setData(index, text, Qt::EditRole);
    }
}
