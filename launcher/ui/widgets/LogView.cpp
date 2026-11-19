// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Freesm Launcher - Minecraft Launcher
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

#include "LogView.h"

#include <KTextEditor/Attribute>
#include <KTextEditor/Cursor>
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/MovingRange>
#include <KTextEditor/View>
#include <KLocalizedString>
#include <QAction>
#include <QMenu>
#include <QRegularExpression>
#include <Application.h>
#include <translations/TranslationsModel.h>

#include <QVBoxLayout>
#include <QApplication>
#include <QColor>
#include <QFont>
#include <QList>
#include <QPalette>
#include <QEvent>
#include <QString>
#include <QStringList>
#include <QVector>

namespace {
// KSyntaxHighlighting ships "Breeze Light" and "Breeze Dark" as built-in themes;
// pick the one matching the app palette so theme-derived log colors stay readable.
QString themeForPalette(const QPalette& palette)
{
    const QColor bg = palette.color(QPalette::Base);
    // ITU-R BT.601 luma, same metric used by the theme system for dark themes
    const double luma = (0.299 * bg.red() + 0.587 * bg.green() + 0.114 * bg.blue()) / 255.0;
    return luma < 0.5 ? QStringLiteral("Breeze Dark") : QStringLiteral("Breeze Light");
}
}  // namespace

LogView::LogView(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* editor = KTextEditor::Editor::instance();
    m_doc = editor->createDocument(this);

    auto* view = m_doc->createView(this);
    m_view = view;
    layout->addWidget(m_view);
    // KTextEditor requires the document to be read-write for its text-mutating
    // API calls to succeed, so we keep flipping it back to read-only after
    // every programmatic edit instead (see clear(), rowsInserted(), rowsRemoved()).
    m_doc->setReadWrite(false);

    // a log is not source code: no modification gutter, wrap at view width,
    // and keep line numbers for orientation
    m_view->setConfigValue(QStringLiteral("line-numbers"), true);
    m_view->setConfigValue(QStringLiteral("modification-markers"), false);
    m_view->setConfigValue(QStringLiteral("dynamic-word-wrap"), true);
    // Kate's minimap and mark-aware scrollbar, rather than the narrow default bar.
    m_view->setConfigValue(QStringLiteral("scrollbar-minimap"), true);
    m_view->setConfigValue(QStringLiteral("scrollbar-marks"), true);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    const QString launcherLanguage = APPLICATION->translations()->selectedLanguage();
    if (!launcherLanguage.isEmpty()) {
        KLocalizedString::setLanguages(QStringList{launcherLanguage});
    }
    connect(m_view, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu* menu = m_view->contextMenu();
        if (!menu) return;
        const QRegularExpression editorSettings(QStringLiteral("mode|indent|tab|encoding|highlight"),
                                               QRegularExpression::CaseInsensitiveOption);
        const auto removeEditorSettings = [&editorSettings](QMenu* target, const auto& self) -> void {
            for (QAction* action : target->actions()) {
                if (editorSettings.match(action->text() + QLatin1Char(' ') + action->objectName()).hasMatch()) {
                    target->removeAction(action);
                } else if (action->menu()) {
                    self(action->menu(), self);
                }
            }
        };
        removeEditorSettings(menu, removeEditorSettings);
        menu->popup(m_view->mapToGlobal(pos));
    });
    m_doc->setHighlightingMode(QStringLiteral("Log File (simplified)"));
    m_view->setConfigValue(QStringLiteral("theme"), themeForPalette(QApplication::palette()));
}

LogView::~LogView()
{
    clearLineRanges();
}

void LogView::setWordWrap(bool wrapping)
{
    if (m_view) {
        m_view->setConfigValue(QStringLiteral("dynamic-word-wrap"), wrapping);
    }
}

void LogView::setColorLines(bool colorLines)
{
    if (m_colorLines == colorLines)
        return;
    m_colorLines = colorLines;
    if (m_doc) {
        // The syntax definition is the single source of log coloring. This
        // avoids model-provided per-line overlays fighting syntax highlights.
        m_doc->setHighlightingMode(colorLines ? QStringLiteral("Log File (simplified)") : QStringLiteral("None"));
    }
}

QString LogView::toPlainText() const
{
    return m_doc ? m_doc->text() : QString();
}

void LogView::setPlainText(const QString& text)
{
    setModel(nullptr);
    clear();
    if (!text.isEmpty()) {
        m_doc->setReadWrite(true);
        m_doc->setText(text);
        m_doc->setReadWrite(false);
    }
}

void LogView::clear()
{
    clearLineRanges();
    if (m_doc) {
        m_doc->setReadWrite(true);
        m_doc->clear();
        m_doc->setReadWrite(false);
    }
}

void LogView::setModel(QAbstractItemModel* model)
{
    if (m_model) {
        disconnect(m_model, &QAbstractItemModel::modelReset, this, &LogView::repopulate);
        disconnect(m_model, &QAbstractItemModel::rowsInserted, this, &LogView::rowsInserted);
        disconnect(m_model, &QAbstractItemModel::rowsRemoved, this, &LogView::rowsRemoved);
        disconnect(m_model, &QAbstractItemModel::destroyed, this, &LogView::modelDestroyed);
    }
    m_model = model;
    if (m_model) {
        connect(m_model, &QAbstractItemModel::modelReset, this, &LogView::repopulate);
        connect(m_model, &QAbstractItemModel::rowsInserted, this, &LogView::rowsInserted);
        connect(m_model, &QAbstractItemModel::rowsRemoved, this, &LogView::rowsRemoved);
        connect(m_model, &QAbstractItemModel::destroyed, this, &LogView::modelDestroyed);
    }
    repopulate();
}

QAbstractItemModel* LogView::model() const
{
    return m_model;
}

void LogView::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    // keep the editor theme in sync with the launcher's light/dark palette
    if (event->type() == QEvent::PaletteChange && m_view) {
        m_view->setConfigValue(QStringLiteral("theme"), themeForPalette(QApplication::palette()));
    }
}

void LogView::modelDestroyed(QObject* model)
{
    if (m_model == model) {
        setModel(nullptr);
    }
}

bool LogView::isAtBottom() const
{
    if (!m_view) {
        return true;
    }
    // independent of word-wrap units: the last real line is displayed when following the tail
    const int lastLine = m_doc ? m_doc->lines() - 1 : 0;
    return m_view->lastDisplayedLine() >= lastLine;
}

void LogView::repopulate()
{
    const int topLine = isAtBottom() ? -1 : (m_view ? m_view->firstDisplayedLine() : -1);

    clear();

    if (m_model) {
        const int rows = m_model->rowCount();
        if (rows > 0) {
            rowsInserted(QModelIndex(), 0, rows - 1);
        }
    }

    // keep the view where it was (unless it was following the tail, which is
    // handled by the insert path itself)
    if (topLine >= 0 && m_view) {
        m_view->setScrollPosition(KTextEditor::Cursor(topLine, 0));
    }
}

void LogView::rowsInserted(const QModelIndex& parent, int first, int last)
{
    if (!m_model || !m_doc) {
        return;
    }

    const bool atBottom = isAtBottom();

    QVector<LineColors> colors;
    const QStringList lines = collectLines(parent, first, last, colors);
    if (lines.isEmpty()) {
        return;
    }

    // apply a font from the model, if any
    for (int i = first; i <= last; i++) {
        const auto font = m_model->data(m_model->index(i, 0, parent), Qt::FontRole);
        if (font.isValid()) {
            const QFont qfont = font.value<QFont>();
            if (qfont != m_appliedFont) {
                m_appliedFont = qfont;
                m_view->setConfigValue(QStringLiteral("font"), qfont);
            }
            break;
        }
    }

    // A cleared KTextEditor document still contains one empty line. Replace
    // it for the first model rows rather than introducing a phantom line.
    const bool firstBatch = m_lineRanges.isEmpty();
    const int firstLine = firstBatch ? 0 : m_doc->lines();
    m_doc->setReadWrite(true);
    const bool inserted_ok = firstBatch ? m_doc->setText(lines.join(QLatin1Char('\n'))) : m_doc->insertLines(firstLine, lines);
    m_doc->setReadWrite(false);
    if (!inserted_ok) {
        return;
    }

    const int inserted = lines.size();
    m_lineRanges.reserve(m_lineRanges.size() + inserted);
    for (int i = 0; i < inserted; i++) {
        const int lineNumber = firstLine + i;
        m_lineRanges.append(colorizeLine(lineNumber, lines[i], colors[i]));
    }

    if (atBottom) {
        m_view->setScrollPosition(m_view->maxScrollPosition());
    }
}

void LogView::rowsRemoved(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent);
    if (!m_doc || first < 0 || last < first) {
        return;
    }

    const int count = qMin(last - first + 1, static_cast<int>(m_lineRanges.size()));
    if (count == 0) {
        return;
    }
    for (int i = 0; i < count; i++) {
        delete m_lineRanges.takeFirst();
    }
    m_doc->setReadWrite(true);
    if (count == m_doc->lines()) {
        m_doc->clear();
    } else {
        // remove the first `count` lines including their line terminators; this stays
        // valid even if the last removed line is empty (lineLength == 0)
        const KTextEditor::Range removed(KTextEditor::Cursor(0, 0), KTextEditor::Cursor(count, 0));
        m_doc->removeText(removed);
    }
    m_doc->setReadWrite(false);
}

void LogView::scrollToBottom()
{
    if (m_view) {
        m_view->setScrollPosition(m_view->maxScrollPosition());
    }
}

void LogView::findNext(const QString& what, bool reverse)
{
    if (!m_doc || !m_view || what.isEmpty())
        return;

    KTextEditor::SearchOptions options = KTextEditor::CaseInsensitive;
    if (reverse) {
        options |= KTextEditor::Backwards;
    }

    const KTextEditor::Range documentRange = m_doc->documentRange();
    const KTextEditor::Cursor cursor = m_view->selection()
        ? (reverse ? m_view->selectionRange().start() : m_view->selectionRange().end())
        : m_view->cursorPosition();
    const KTextEditor::Range searchRange = reverse
        ? KTextEditor::Range(documentRange.start(), cursor)
        : KTextEditor::Range(cursor, documentRange.end());

    auto matches = m_doc->searchText(searchRange, what, options);
    if (matches.isEmpty() || !matches.first().isValid()) {
        matches = m_doc->searchText(documentRange, what, options);
    }
    if (matches.isEmpty() || !matches.first().isValid()) {
        return;
    }

    const auto match = matches.first();
    m_view->setCursorPosition(reverse ? match.start() : match.end());
    m_view->setSelection(match);
}

QStringList LogView::collectLines(const QModelIndex& parent, int first, int last, QVector<LineColors>& colors) const
{
    QStringList lines;
    lines.reserve(last - first + 1);
    colors.reserve(last - first + 1);

    for (int i = first; i <= last; i++) {
        const auto idx = m_model->index(i, 0, parent);

        QString text = m_model->data(idx, Qt::DisplayRole).toString();
        // the document is line based; stray line breaks would desync it from the model
        text.replace(QStringLiteral("\r"), QStringLiteral(" "));
        text.replace(QStringLiteral("\n"), QStringLiteral(" "));
        lines.append(text);

        // Color Lines is implemented by the logfile syntax definition; do not
        // apply model foreground/background roles on top of syntax highlighting.
        colors.append(LineColors{});
    }

    return lines;
}

KTextEditor::MovingRange* LogView::colorizeLine(int line, const QString& text, const LineColors& colors)
{
    const bool hasForeground = colors.foreground.isValid();
    const bool hasBackground = colors.background.isValid();

    if (!hasForeground && !hasBackground) {
        return nullptr;
    }
    if (text.isEmpty()) {
        // moving ranges cannot be empty
        return nullptr;
    }

    auto attribute = KTextEditor::Attribute::Ptr(new KTextEditor::Attribute());
    if (hasForeground) {
        attribute->setForeground(colors.foreground);
    }
    if (hasBackground) {
        attribute->setBackground(colors.background);
        attribute->setBackgroundFillWhitespace(true);
    }

    auto* range = m_doc->newMovingRange(KTextEditor::Range(KTextEditor::Cursor(line, 0), KTextEditor::Cursor(line, text.length())));
    range->setAttribute(attribute);
    range->setAttributeOnlyForViews(true);
    return range;
}

void LogView::clearLineRanges()
{
    for (auto* range : m_lineRanges) {
        delete range;
    }
    m_lineRanges.clear();
}
