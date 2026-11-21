// SPDX-License-Identifier: GPL-3.0-only
#include <QStandardItemModel>
#include <QTest>

#include <KTextEditor/View>

#include "ui/widgets/LogView.h"

class LogViewTest : public QObject {
    Q_OBJECT

   private slots:
    void modelLinesAndRemoval()
    {
        QStandardItemModel model;
        model.appendRow(new QStandardItem(QStringLiteral("first")));
        model.appendRow(new QStandardItem(QStringLiteral("second")));
        LogView view;
        view.setModel(&model);
        QCOMPARE(view.toPlainText(), QStringLiteral("first\nsecond"));

        model.appendRow(new QStandardItem(QStringLiteral("third")));
        QCOMPARE(view.toPlainText(), QStringLiteral("first\nsecond\nthird"));
        model.removeRow(0);
        QCOMPARE(view.toPlainText(), QStringLiteral("second\nthird"));
        model.removeRows(0, 2);
        QCOMPARE(view.toPlainText(), QString());
        model.appendRow(new QStandardItem(QStringLiteral("new")));
        QCOMPARE(view.toPlainText(), QStringLiteral("new"));
    }

    void plainTextAndSearch()
    {
        LogView view;
        view.setPlainText(QStringLiteral("one two one"));
        view.setColorLines(false);
        QCOMPARE(view.toPlainText(), QStringLiteral("one two one"));
        auto* editorView = view.findChild<KTextEditor::View*>();
        QVERIFY(editorView);
        view.findNext(QStringLiteral("one"), false);
        QCOMPARE(editorView->selectionRange().start().column(), 0);
        view.findNext(QStringLiteral("one"), false);
        QCOMPARE(editorView->selectionRange().start().column(), 8);
        view.findNext(QStringLiteral("one"), false);
        QCOMPARE(editorView->selectionRange().start().column(), 0);
        view.findNext(QStringLiteral("one"), true);
        QCOMPARE(editorView->selectionRange().start().column(), 8);
    }
};

QTEST_MAIN(LogViewTest)
#include "LogView_test.moc"
