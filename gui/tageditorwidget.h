#ifndef TAGEDITORWIDGET_H
#define TAGEDITORWIDGET_H

#include "./previousvaluehandling.h"
#include "./webviewdefs.h"

#include <tagparser/diagnostics.h>
#include <tagparser/mediafileinfo.h>

#include <QByteArray>
#include <QFuture>
#include <QWidget>

#include <functional>

QT_FORWARD_DECLARE_CLASS(QFileSystemWatcher)
QT_FORWARD_DECLARE_CLASS(QMenu)
QT_FORWARD_DECLARE_CLASS(QTreeView)
QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QTemporaryFile)

#define TAGEDITOR_ENUM_CLASS enum class
namespace TagParser {
TAGEDITOR_ENUM_CLASS TagType : unsigned int;
}
#undef TAGEDITOR_ENUM_CLASS

namespace QtGui {

namespace Ui {
class TagEditorWidget;
}

class TagEdit;
class FileInfoModel;

class TagEditorWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QString currentDir READ currentDir)
    Q_PROPERTY(bool tagEditShown READ isTagEditShown)
    Q_PROPERTY(QByteArray fileInfoHtml READ fileInfoHtml)
    Q_PROPERTY(bool fileNameVisible READ isFileNameVisible WRITE setFileNameVisible)
    Q_PROPERTY(bool buttonsVisible READ areButtonsVisible WRITE setButtonVisible)
    Q_PROPERTY(bool fileOperationOngoing READ isFileOperationOngoing)

public:
    explicit TagEditorWidget(QWidget *parent = nullptr);
    ~TagEditorWidget() override;

public:
    bool isFileOperationOngoing() const;
    const QString &currentPath() const;
    const QString &currentDir() const;
    TagParser::MediaFileInfo &fileInfo();
    const TagParser::Diagnostics &diagnostics() const;
    bool isTagEditShown() const;
    const QByteArray &fileInfoHtml() const;
    const QByteArray &generateFileInfoHtml();
    bool isFileNameVisible() const;
    void setFileNameVisible(bool visible);
    bool areButtonsVisible() const;
    void setButtonVisible(bool visible);
    void foreachTagEdit(const std::function<void(TagEdit *)> &function);
    TagEdit *activeTagEdit();

public Q_SLOTS:
    // operations with the currently opened file: load, save, delete, close
    bool startParsing(const QString &path, bool forceRefresh = false);
    bool startSaving();
    void saveAndShowNextFile();
    bool reparseFile();
    bool applyEntriesAndSaveChangings();
    bool deleteAllTagsAndSave();
    void closeFile();
    void renameFile();
    void saveFileInfo();
    void openFileInfoInBrowser();
    // misc
    void applySettingsFromDialog();
    void addParsingNotificationLine(const QString &line);

Q_SIGNALS:
    /// \brief Emitted when loading the next file has been triggered.
    void nextFileSelected();
    /// \brief Emitted to show a status message.
    void statusMessage(const QString &message, int timeout = 0);
    /// \brief Emitted when the file status (opened/closed) has changed.
    void fileStatusChanged(bool opened, bool hasTag);
    /// \brief Emitted when the current path has changed; always emitted a saving.
    void currentPathChanged(const QString &newPath);
    /// \brief Emitted when all tag values have been parsed and loaded into tag edits.
    /// \remarks In particular, this is emitted *before* any additional data is inserted (like title from file name).
    void tagValuesLoaded();
    /// \brief Emitted when a file has been shown (file is parsed and all widgets have been updated accordingly).
    /// \remarks In particular, this is emitted *after* additional data (like title from file name) might have been inserted.
    void fileShown();

protected:
    bool event(QEvent *event) override;

private Q_SLOTS:
    // editor
    void fileChangedOnDisk(const QString &path);
    void showFile(char result, const QString &ioError);
    void handleReturnPressed();
    void handleKeepPreviousValuesActionTriggered(QAction *action);
    void addTag(const std::function<TagParser::Tag *(TagParser::MediaFileInfo &)> &createTag);
    void removeTag(TagParser::Tag *tag);
    void changeTarget(TagParser::Tag *tag);

    // saving
    void showSavingResult(QString ioError, bool processingError, bool canceled);

    // info (web) view
    void initInfoView();
    void updateInfoView();
    void showInfoTreeViewContextMenu(const QPoint &position);
#ifndef TAGEDITOR_NO_WEBVIEW
    void showInfoWebViewContextMenu(const QPoint &position);
#endif
    bool handleFileInfoUnavailable();
    bool writeFileInfoToFile(QFile &file);

private:
    void updateDocumentTitleEdits();
    void updateTagEditsAndAttachmentEdits(bool updateUi = true, PreviousValueHandling previousValueHandling = PreviousValueHandling::Auto);
    void updateTagSelectionComboBox();
    void updateFileStatusStatus();
    void updateTagManagementMenu();
    void updateKeepPreviousValuesButton();
    void insertTitleFromFilename();
    bool confirmCreationOfId3TagForUnsupportedFile();
    void invalidateTags();

    // UI
    std::unique_ptr<Ui::TagEditorWidget> m_ui;
    QMenu *m_keepPreviousValuesMenu;
    QMenu *m_tagOptionsMenu;
    QMenu *m_addTagMenu;
    QMenu *m_removeTagMenu;
    QMenu *m_changeTargetMenu;
#ifndef TAGEDITOR_NO_WEBVIEW
    TAGEDITOR_WEB_VIEW *m_infoWebView;
#endif
    FileInfoModel *m_infoModel;
    QTreeView *m_infoTreeView;
    std::unique_ptr<QTemporaryFile> m_temporaryInfoFile;
    // tag, file, directory management
    QString m_currentPath;
    QFileSystemWatcher *m_fileWatcher;
    bool m_fileChangedOnDisk;
    TagParser::MediaFileInfo m_fileInfo;
    std::vector<TagParser::Tag *> m_tags;
    QByteArray m_fileInfoHtml;
    QString m_fileName;
    QString m_currentDir;
    QString m_lastDir;
    QString m_saveFilePath;
    // status
    TagParser::Diagnostics m_diag;
    TagParser::Diagnostics m_diagReparsing;
    QFuture<void> m_ongoingFileOperation;
    bool m_nextFileAfterSaving;
    bool m_makingResultsAvailable;
    bool m_abortClicked;
};

/*!
 * \brief Returns the mutex which is internally used for thread-synchronization.
 */
inline bool TagEditorWidget::isFileOperationOngoing() const
{
    return m_ongoingFileOperation.isRunning();
}

/*!
 * \brief Returns the path of the currently opened file including filename.
 */
inline const QString &TagEditorWidget::currentPath() const
{
    return m_currentPath;
}

/*!
 * \brief Returns the path of the currently opened file excluding filename.
 * \remarks This is the actual directory of the opened file which may differ from the directory selected in the tree view of the main window.
 */
inline const QString &TagEditorWidget::currentDir() const
{
    return m_currentDir;
}

/*!
 * \brief Return file info.
 */
inline TagParser::MediaFileInfo &TagEditorWidget::fileInfo()
{
    return m_fileInfo;
}

/*!
 * \brief Returns the diagnostic messages.
 */
inline const TagParser::Diagnostics &TagEditorWidget::diagnostics() const
{
    return m_diag;
}

/*!
 * \brief Returns the HTML source of the info website.
 * \remarks Returns an empty string if no file info has been generated yet. See generateFileInfoHtml() for a method which will ensure
 *          the file info has been generated.
 */
inline const QByteArray &TagEditorWidget::fileInfoHtml() const
{
    return m_fileInfoHtml;
}

/*!
 * \brief Returns whether currently a tag edit is shown.
 */
inline bool TagEditorWidget::isTagEditShown() const
{
    return !m_tags.empty();
}

} // namespace QtGui

#endif // TAGEDITORWIDGET_H
