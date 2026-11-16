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
 */

#pragma once

#include <QAbstractItemModel>
#include <QString>
#include <QVector>
#include <QWidget>

class QColor;
class QEvent;
class QFont;

namespace KTextEditor {
class Document;
class View;
class MovingRange;
}  // namespace KTextEditor

/**
 * The log display widget used by the Minecraft log page, the "Other Logs" page
 * and the launcher log window.
 *
 * It embeds a KTextEditor view — the same editor component that KWrite is built on,
 * from the Kate project — instead of a plain text edit. Compared to the previous
 * QPlainTextEdit implementation this gives us, for free:
 *
 *  - line numbers and a find bar (Ctrl+F) with match highlighting built into the view,
 *  - wrap-aware rendering and better performance on very large logs,
 *  - correct handling of log buffer overflows (lines removed from the model front).
 *
 * Per-line colors from the log model are applied as dynamic highlighting
 * attributes (MovingRanges). The view is read-only.
 */
class LogView : public QWidget {
    Q_OBJECT
   public:
    explicit LogView(QWidget* parent = nullptr);
    ~LogView() override;

    void setModel(QAbstractItemModel* model);
    QAbstractItemModel* model() const;

    // plain-text compatibility helpers (used e.g. to show a whole file)
    QString toPlainText() const;
    void setPlainText(const QString& text);
    void clear();

   public slots:
    void setWordWrap(bool wrapping);
    void setColorLines(bool colorLines);
    void findNext(const QString& what, bool reverse);
    void scrollToBottom();

   protected:
    void changeEvent(QEvent* event) override;

   protected slots:
    void repopulate();
    // note: this supports only appending
    void rowsInserted(const QModelIndex& parent, int first, int last);
    // note: this supports only removing from the front
    void rowsRemoved(const QModelIndex& parent, int first, int last);
    void modelDestroyed(QObject* model);

   private:
    struct LineColors {
        QColor foreground;
        QColor background;
    };

    bool isAtBottom() const;
    QStringList collectLines(const QModelIndex& parent, int first, int last, QVector<LineColors>& colors) const;
    KTextEditor::MovingRange* colorizeLine(int line, const QString& text, const LineColors& colors);
    void clearLineRanges();

    QAbstractItemModel* m_model = nullptr;
    KTextEditor::Document* m_doc = nullptr;
    KTextEditor::View* m_view = nullptr;
    /// one moving range per document line (nullptr for lines without colors), kept in sync with the document
    QVector<KTextEditor::MovingRange*> m_lineRanges;
    bool m_colorLines = true;
    QFont m_appliedFont;
};
