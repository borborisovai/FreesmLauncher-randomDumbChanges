// SPDX-License-Identifier: GPL-3.0-only
/*
 * SPDX-FileCopyrightText: 2026 FreesmLauncher Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file incorporates code adapted from Dolphin, which is
 * licensed under GPL-2.0-or-later:
 *
 *   src/views/zoomlevelinfo.h — Dolphin zoom level table
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

#pragma once

#include <QWidget>

class QSlider;
class QLabel;
class QAction;

/**
 * Port of Dolphin's ZoomLevelInfo (src/views/zoomlevelinfo.{h,cpp}).
 *
 * Dolphin maps icon sizes to zoom levels with the KDE iconloader sizes:
 *   level 0 -> 16px, level 1 -> 22px, level 2 -> 32px, level 3 -> 48px, level 4 -> 64px,
 *   each level above 4 adds 16px.
 * The KIconLoader dependency is replaced by literal constants.
 */
namespace ZoomLevelInfo {
constexpr int MinimumLevel = 0;
constexpr int MaximumLevel = 16;

int minimumLevel();
int maximumLevel();
/// Icon size in pixels for the zoom \a level (0..16).
int iconSizeForZoomLevel(int level);
/// The zoom level that maps onto (or is closest to) the given pixel \a size.
int zoomLevelForIconSize(int size);
}  // namespace ZoomLevelInfo

/**
 * Dolphin's status bar zoom slider: a horizontal slider with a 'Zoom:' label,
 * showing the current icon size in pixels. Meant to live in a QStatusBar
 * permanent slot (like Dolphin's own status bar).
 */
class ZoomStatusSlider : public QWidget {
    Q_OBJECT

   public:
    explicit ZoomStatusSlider(QWidget* parent = nullptr);

    int zoomLevel() const;

   public slots:
    void setZoomLevel(int level);

   signals:
    /// Emitted whenever the slider moves (or setZoomLevel changes the value).
    void zoomLevelChanged(int level);

   private:
    QSlider* m_slider = nullptr;
    QLabel* m_sizeLabel = nullptr;
};

/// Actions for zooming with standard shortcuts (ZoomIn/ZoomOut/ZoomReset).
struct ZoomToolbarActions {
    QAction* zoomIn = nullptr;
    QAction* zoomOut = nullptr;
    QAction* zoomReset = nullptr;
};

/**
 * Creates zoom-in/zoom-out/reset actions with the standard
 * QKeySequence::ZoomIn / QKeySequence::ZoomOut / QKeySequence::ZoomReset
 * shortcuts, to be added to a page's toolbar or menu.
 */
ZoomToolbarActions createZoomToolbarActions(QObject* parent);
