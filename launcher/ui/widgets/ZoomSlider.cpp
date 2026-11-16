// SPDX-License-Identifier: GPL-3.0-only
/*
 * SPDX-FileCopyrightText: 2026 FreesmLauncher Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file incorporates code adapted from Dolphin, which is
 * licensed under GPL-2.0-or-later:
 *
 *   src/views/zoomlevelinfo.cpp — Dolphin zoom level table
 *   src/statusbar/dolphinstatusbar.cpp — the zoom slider in the status bar
 *   SPDX-FileCopyrightText: 2008 Peter Penz <peter.penz19@gmail.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, version 3.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ZoomSlider.h"

#include <QHBoxLayout>
#include <QAction>
#include <QCoreApplication>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QSlider>

int ZoomLevelInfo::minimumLevel()
{
    return MinimumLevel;
}

int ZoomLevelInfo::maximumLevel()
{
    return MaximumLevel;
}

int ZoomLevelInfo::iconSizeForZoomLevel(int level)
{
    // KIconLoader::SizeSmall=16, SizeSmallMedium=22, SizeMedium=32, SizeLarge=48, SizeHuge=64
    constexpr int Small = 16;
    constexpr int SmallMedium = 22;
    constexpr int Medium = 32;
    constexpr int Large = 48;
    constexpr int Huge = 64;

    switch (qBound(MinimumLevel, level, MaximumLevel)) {
        case 0:
            return Small;
        case 1:
            return SmallMedium;
        case 2:
            return Medium;
        case 3:
            return Large;
        case 4:
            return Huge;
        default:
            return Huge + ((level - 4) << 4);
    }
}

int ZoomLevelInfo::zoomLevelForIconSize(int size)
{
    switch (size) {
        case 16:
            return 0;
        case 22:
            return 1;
        case 32:
            return 2;
        case 48:
            return 3;
        case 64:
            return 4;
        default:
            return qBound(MinimumLevel, 4 + ((size - 64) >> 4), MaximumLevel);
    }
}

ZoomStatusSlider::ZoomStatusSlider(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(6);

    auto* label = new QLabel(tr("Zoom:"), this);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setAccessibleName(tr("Zoom"));
    m_slider->setAccessibleDescription(tr("Sets the size of the icons."));
    m_slider->setPageStep(1);
    m_slider->setRange(ZoomLevelInfo::minimumLevel(), ZoomLevelInfo::maximumLevel());
    m_slider->setFixedWidth(140);
    m_slider->setToolTip(tr("Zoom"));

    m_sizeLabel = new QLabel(QStringLiteral("0px"), this);
    m_sizeLabel->setFixedWidth(40);
    m_sizeLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    layout->addWidget(label);
    layout->addWidget(m_slider);
    layout->addWidget(m_sizeLabel);

    connect(m_slider, &QSlider::valueChanged, this, [this](int value) {
        m_sizeLabel->setText(tr("%1px").arg(ZoomLevelInfo::iconSizeForZoomLevel(value)));
        emit zoomLevelChanged(value);
    });
}

int ZoomStatusSlider::zoomLevel() const
{
    return m_slider->value();
}

void ZoomStatusSlider::setZoomLevel(int level)
{
    const int clamped = qBound(ZoomLevelInfo::minimumLevel(), level, ZoomLevelInfo::maximumLevel());
    if (m_slider->value() != clamped) {
        m_slider->setValue(clamped);  // emits valueChanged if the value actually changed
    }
    m_sizeLabel->setText(tr("%1px").arg(ZoomLevelInfo::iconSizeForZoomLevel(clamped)));
}

ZoomToolbarActions createZoomToolbarActions(QObject* parent)
{
    ZoomToolbarActions actions;

    actions.zoomIn = new QAction(QIcon::fromTheme(QStringLiteral("zoom-in")),
                                 QCoreApplication::translate("ZoomStatusSlider", "Zoom In"), parent);
    actions.zoomIn->setShortcut(QKeySequence::ZoomIn);
    actions.zoomIn->setShortcutContext(Qt::WindowShortcut);
    actions.zoomIn->setToolTip(QCoreApplication::translate("ZoomStatusSlider", "Make the icons bigger"));

    actions.zoomOut = new QAction(QIcon::fromTheme(QStringLiteral("zoom-out")),
                                  QCoreApplication::translate("ZoomStatusSlider", "Zoom Out"), parent);
    actions.zoomOut->setShortcut(QKeySequence::ZoomOut);
    actions.zoomOut->setShortcutContext(Qt::WindowShortcut);
    actions.zoomOut->setToolTip(QCoreApplication::translate("ZoomStatusSlider", "Make the icons smaller"));

    actions.zoomReset = new QAction(QCoreApplication::translate("ZoomStatusSlider", "Reset Zoom"), parent);
    actions.zoomReset->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    actions.zoomReset->setShortcutContext(Qt::WindowShortcut);
    actions.zoomReset->setToolTip(QCoreApplication::translate("ZoomStatusSlider", "Reset the icon size to its default"));

    return actions;
}
