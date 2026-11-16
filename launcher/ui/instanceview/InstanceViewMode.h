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
 *
 *  This file incorporates code adapted from Dolphin, which is
 *  licensed under GPL-2.0-or-later:
 *
 *    src/views/dolphinview.h — DolphinView::Mode enum
 *    SPDX-FileCopyrightText: 2006-2009 Peter Penz <peter.penz19@gmail.com>
 *    SPDX-FileCopyrightText: 2006 Gregor Kališnik <gregor@podnapisi.net>
 */

#pragma once

#include <QString>

/// View modes for the instance list, modeled after Dolphin's DolphinView::Mode.
/// The mode is persisted globally in the "InstViewMode" setting
/// (analogous to Dolphin's global view properties default).
enum class InstanceViewMode {
    /// Items are shown as icons with the name label below them (DolphinView::IconsView).
    Icons,
    /// Items are shown as a table with sortable columns (DolphinView::DetailsView).
    Details,
};

inline QString instanceViewModeToString(InstanceViewMode mode)
{
    switch (mode) {
        case InstanceViewMode::Details:
            return QStringLiteral("Details");
        case InstanceViewMode::Icons:
        default:
            return QStringLiteral("Icons");
    }
}

inline InstanceViewMode instanceViewModeFromString(const QString& mode)
{
    if (mode.compare(QStringLiteral("Details"), Qt::CaseInsensitive) == 0) {
        return InstanceViewMode::Details;
    }
    return InstanceViewMode::Icons;
}
