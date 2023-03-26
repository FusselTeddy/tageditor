#ifndef TAGEDITORMAINWINDOW_H
#define TAGEDITORMAINWINDOW_H

#include "./filefilterproxymodel.h"
#include "./previousvaluehandling.h"

#include <tagparser/mediafileinfo.h>
#include <tagparser/tagvalue.h>

#include <QByteArray>
#include <QMainWindow>

QT_FORWARD_DECLARE_CLASS(QFileSystemModel)
QT_FORWARD_DECLARE_CLASS(QItemSelectionModel)

#define TAGEDITOR_ENUM_CLASS enum class
namespace TagParser {
TAGEDITOR_ENUM_CLASS TagType : unsigned int;
} // namespace TagParser
#undef TAGEDITOR_ENUM_CLASS

namespace QtUtilities {
class AboutDialog;
class SettingsDialog;
} // namespace QtUtilities

namespace QtGui {

namespace Ui {
class MainWindow;
}

class TagEditorWidget;
class RenameFilesDialog;
class DbQueryWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
    Q_PROPERTY(QString currentDirectory READ currentDirectory WRITE setCurrentDirectory)
    Q_PROPERTY(bool layoutLocked READ isLayoutLocked WRITE setLayoutLocked)

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // file browser
    QString currentDirectory() const;
    bool isLayoutLocked() const;

public Q_SLOTS:
    void setCurrentDirectory(const QString &path);
    void setLayoutLocked(bool locked);
    void toggleLayoutLocked();
    void startParsing(const QString &path);

protected:
    bool event(QEvent *event) override;

private Q_SLOTS:
    // file selection
    void pathEntered();
    void fileSelected();
    void selectNextFile();
    void selectNextFile(QItemSelectionModel *selectionModel, const QModelIndex &currentIndex, bool notDeeper);
    void showNextFileNotFound();
    void showOpenFileDlg();
    void showSaveAsDlg();
    void handleFileStatusChange(bool opened, bool hasTag);
    void handleCurrentPathChanged(const QString &newPath);
    void updateStyleSheet();

    // settings
    void showNewWindow();
    void showSettingsDlg();
    void applySettingsFromDialog();

    // misc
    void showAboutDlg();
    void showRenameFilesDlg();
    void spawnExternalPlayer();
    void initDbQueryWidget();
    void toggleDbQueryWidget();

private:
    bool fileOperationOngoing() const;
    TagParser::MediaFileInfo &fileInfo();

    // UI
    std::unique_ptr<Ui::MainWindow> m_ui;
    // models
    QFileSystemModel *m_fileModel;
    FileFilterProxyModel *m_fileFilterModel;
    bool m_internalFileSelection;
    // dialogs
    QtUtilities::AboutDialog *m_aboutDlg;
    QtUtilities::SettingsDialog *m_settingsDlg;
    std::unique_ptr<RenameFilesDialog> m_renameFilesDlg;
    DbQueryWidget *m_dbQueryWidget;
};

} // namespace QtGui

#endif // TAGEDITORMAINWINDOW_H
