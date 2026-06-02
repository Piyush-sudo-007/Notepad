#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "tiktoken.h"

#include <QMessageBox>

#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QFileInfo>

#include <QFontDialog>
#include <QFont>

#include <QColorDialog>
#include <QColor>

#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrinter>

#include <QSqlDatabase>
#include <QSqlQuery>

#include <QAbstractItemView>
#include <QCompleter>
#include <QStringListModel>

QString currentFile;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Untitled - SmartPad");

    currentMode = "Blog";

    setupDatabase();
    initSmartEngine();

    completer = new QCompleter(this);
    completerModel = new QStringListModel(this);
    completer->setModel(completerModel);
    completer->setWidget(ui->textEdit);
    completer->setCompletionMode(QCompleter::PopupCompletion);

    connect(completer, QOverload<const QString &>::of(&QCompleter::activated), this, [=](const QString &text)
            {
        QTextCursor cursor = ui->textEdit->textCursor();
        cursor.insertText(text);
        logWordToUserVocabulary(text); });

    connect(ui->textEdit, &QTextEdit::textChanged, this, [=]()
            {
        QString fullDocumentText = ui->textEdit->toPlainText();

        if(fullDocumentText.contains("#include") || fullDocumentText.contains("import ") || fullDocumentText.contains("using ")){
            currentMode = "Code";
        }
        else{
            currentMode = "Blog";
        }

        QString trackingContext = fullDocumentText.right(80);
        if(!trackingContext.isEmpty() && trackingContext.endsWith(" ")){
            emit requestPrediction(trackingContext, currentMode);
        } });
}

MainWindow::~MainWindow()
{
    workerThread.quit();
    workerThread.wait();
    delete ui;
}

// Update status bar
void MainWindow::on_textEdit_cursorPositionChanged()
{
    QTextCursor cursor = ui->textEdit->textCursor();

    int line = cursor.blockNumber() + 1;
    int col = cursor.positionInBlock() + 1;
    int charCount = ui->textEdit->toPlainText().length();

    QString status = QString("Line: %1 | Col: %2 | Total %3 characters")
                         .arg(line)
                         .arg(col)
                         .arg(charCount);

    ui->statusbar->showMessage(status);
}

// Check for unsaved changes
bool MainWindow::maybeSave()
{
    if (!ui->textEdit->document()->isModified())
        return true;

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Unsaved Changes",
        "You have unsaved changes. Do you want to save them?",
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (reply == QMessageBox::Yes)
        return on_actionSave_triggered(), true;
    else if (reply == QMessageBox::Cancel)
        return false;

    return true;
}

// New file
void MainWindow::on_actionNew_triggered()
{
    if (!maybeSave())
        return;

    currentFile.clear();
    ui->textEdit->clear();
    ui->textEdit->document()->setModified(false);

    setWindowTitle("Untitled - SmartPad");
}

// Save file
bool MainWindow::on_actionSave_triggered()
{
    if (currentFile.isEmpty())
        return on_actionSave_As_triggered();

    QFile file(currentFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Error", "Could not save the file.");
        return false;
    }

    QTextStream out(&file);
    out << ui->textEdit->toPlainText();
    file.close();

    ui->textEdit->document()->setModified(false);

    setWindowTitle(QFileInfo(currentFile).fileName() + " - SmartPad");
    ui->statusbar->showMessage("File saved", 3000);

    return true;
}

// Save As
bool MainWindow::on_actionSave_As_triggered()
{
    QString filename = QFileDialog::getSaveFileName(
        this,
        "Save File",
        "",
        "Text Files (*.txt);;All Files (*)");

    if (filename.isEmpty())
        return false;

    currentFile = filename;
    return on_actionSave_triggered();
}

// Open file
void MainWindow::on_actionOpen_triggered()
{
    if (!maybeSave())
        return;

    QString filename = QFileDialog::getOpenFileName(
        this,
        "Open File",
        "",
        "Text Files (*.txt);;All Files (*)");

    if (filename.isEmpty())
        return;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Error", "Could not open the file.");
        return;
    }

    QTextStream in(&file);
    ui->textEdit->setPlainText(in.readAll());
    file.close();

    currentFile = filename;
    ui->textEdit->document()->setModified(false);

    setWindowTitle(QFileInfo(currentFile).fileName() + " - NotePad");
    ui->statusbar->showMessage("File opened", 3000);
}

// Exit app
void MainWindow::on_actionExit_triggered()
{
    if (maybeSave())
        close();
}

// Edit Tools

void MainWindow::on_actionCut_triggered()
{
    ui->textEdit->cut();
}

void MainWindow::on_actionCopy_triggered()
{
    ui->textEdit->copy();
}

void MainWindow::on_actionPaste_triggered()
{
    ui->textEdit->paste();
}

void MainWindow::on_actionUndo_triggered()
{
    ui->textEdit->undo();
}

void MainWindow::on_actionRedo_triggered()
{
    ui->textEdit->redo();
}

void MainWindow::on_actionFont_triggered()
{
    bool selected;

    QFont font = QFontDialog::getFont(&selected, this);

    if (selected)
    {
        ui->textEdit->setFont(font);
    }
    else
    {
        return;
    }
}

void MainWindow::on_actionText_Color_triggered()
{
    QColor color = QColorDialog::getColor(Qt::white, this, "Choose Text Color");
    if (color.isValid())
    {
        ui->textEdit->setTextColor(color);
    }
}

void MainWindow::on_actionBg_Color_triggered()
{
    QColor color = QColorDialog::getColor(Qt::white, this, "Choose Background Color");
    if (color.isValid())
    {
        ui->textEdit->setTextBackgroundColor(color);
    }
}

// About tool

void MainWindow::on_actionAbout_triggered()
{
    QString about_txt =
        "<h2>SmartPad</h2>"

        "<p><b>Version:</b> 2.0.0 (AI Powered)</p>"

        "<p>"
        "A lightweight yet powerful text editor built using the Qt framework, "
        "supercharged with completely offline, private AI next-word predictions."
        "</p>"

        "<h3>✨ Features</h3>"
        "<ul>"
        "<li><b>Offline Next-Word Prediction:</b> Powered by an embedded local AI model.</li>"
        "<li><b>Real-Time Personalization:</b> Adapts and learns from your typing habits on the fly.</li>"
        "<li><b>Context Switching:</b> Automatically shifts profiles between Blog writing and Coding modes.</li>"
        "<li>Create, open, and save text files</li>"
        "<li>Undo & redo editing actions</li>"
        "<li>Cut, copy, and paste support</li>"
        "<li>Real-time cursor tracking (line, column, character count)</li>"
        "<li>Custom font and text formatting</li>"
        "<li>Text and background color customization</li>"
        "<li>Zoom in, zoom out, and reset zoom</li>"
        "<li>Print support</li>"
        "<li>Unsaved changes protection</li>"
        "</ul>"

        "<h3>⚙️ Technology</h3>"
        "<p>Built with <b>Qt (C++)</b>, <b>ONNX Runtime Engine</b>, and <b>SQLite3 Embedded Database</b></p>"

        "<h3>👨‍💻 Developer</h3>"
        "<p>Piyush Dev</p>"

        "<p style='margin-top:15px; font-size:10pt; color:gray;'>"
        "© 2026 All rights reserved."
        "</p>";

    QMessageBox::about(this, "About SmartPad", about_txt);
}

// View tools

void MainWindow::on_actionPrint_triggered()
{
    QPrinter printer;
    QPrintDialog dialog(&printer, this);

    if (dialog.exec() == QDialog::Rejected)
    {
        return;
    }
    ui->textEdit->print(&printer);
}

int zoomLevel = 0;

void MainWindow::on_actionZoom_In_triggered()
{
    ui->textEdit->zoomIn(1);
    zoomLevel++;
}

void MainWindow::on_actionZoom_Out_triggered()
{
    ui->textEdit->zoomOut(1);
    zoomLevel--;
}

void MainWindow::on_actionRestor_to_Normal_Zoom_triggered()
{
    if (zoomLevel > 0)
        ui->textEdit->zoomOut(zoomLevel);
    else if (zoomLevel < 0)
        ui->textEdit->zoomIn(-zoomLevel);

    zoomLevel = 0;
}

void MainWindow::setupDatabase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("user_smartprofile.db");
    if (db.open())
    {
        QSqlQuery query;
        query.exec("CREATE TABLE IF NOT EXISTS UserVocabulary ("
                   "TokenID INTEGER, "
                   "ProfileMode TEXT, "
                   "BiasCount INTEGER DEFAULT 0, "
                   "PRIMARY KEY(TokenID, ProfileMode));");
    }
}

void MainWindow::initSmartEngine()
{
    worker = new PredictionWorker();
    worker->moveToThread(&workerThread);

    connect(this, &MainWindow::requestPrediction, worker, &PredictionWorker::processPrediction);
    connect(worker, &PredictionWorker::predictionReady, this, &MainWindow::handlePredictions);

    workerThread.start();

    QMetaObject::invokeMethod(worker, "loadModel", Q_ARG(QString, "assets/gpt2-124m.onnx"));
}

void MainWindow::handlePredictions(const QStringList &suggestions)
{
    if (suggestions.isEmpty())
        return;

    completerModel->setStringList(suggestions);

    QRect cursorRectangle = ui->textEdit->cursorRect();
    cursorRectangle.setWidth(completer->popup()->sizeHint().width());
    completer->complete(cursorRectangle);
}

void MainWindow::logWordToUserVocabulary(const QString &word)
{
    // auto tokenizerInstance = sw::tokenizer::Tiktoken::tiktoken_init("assets/tiktoken.toml");
    sw::tokenizer::TiktokenFactory factory("assets/tiktoken.toml");
    sw::tokenizer::Tiktoken tokenizerInstance = factory.create("gpt2");

    std::vector<uint64_t> ids = tokenizerInstance.encode(word.toStdString());
    if (ids.empty())
        return;

    int acceptedTokenId = static_cast<int>(ids[0]);

    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO UserVocabulary (TokenID, ProfileMode, BiasCount)"
                  "VALUES (:id, :mode, COALESCE((SELECT BiasCount FROM UserVocabulary WHERE TokenID = :id AND ProfileMode = :mode), 0)+1);");
    query.bindValue(":id", acceptedTokenId);
    query.bindValue(":mode", currentMode);
    query.exec();
}