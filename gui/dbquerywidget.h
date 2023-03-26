#ifndef DBQUERYWIDGET_H
#define DBQUERYWIDGET_H

#include <QWidget>

#include <memory>

QT_FORWARD_DECLARE_CLASS(QItemSelection)
QT_FORWARD_DECLARE_CLASS(QMenu)
QT_FORWARD_DECLARE_CLASS(QAction)

namespace Settings {
class KnownFieldModel;
}

namespace QtGui {

namespace Ui {
class DbQueryWidget;
}

class QueryResultsModel;
class TagEditorWidget;
class TagEdit;
struct SongDescription;

class DbQueryWidget : public QWidget {
    Q_OBJECT

public:
    explicit DbQueryWidget(TagEditorWidget *tagEditorWidget, QWidget *parent = nullptr);
    ~DbQueryWidget() override;

    void insertSearchTermsFromTagEdit(TagEdit *tagEdit, bool songSpecific = false);
    SongDescription currentSongDescription() const;
    void applyResults(TagEdit *tagEdit, const QModelIndex &resultIndex);

public Q_SLOTS:
    void searchMusicBrainz();
    void searchLyricsWikia();
    void searchMakeItPersonal();
    void abortSearch();
    void applySelectedResults();
    void applySpecifiedResults(const QModelIndex &modelIndex);
    void applyMatchingResults();
    void applyMatchingResults(TagEdit *tagEdit);
    void autoInsertMatchingResults();
    void insertSearchTermsFromActiveTagEdit();
    void clearSearchCriteria();

private Q_SLOTS:
    void updateStyleSheet();
    void showResults();
    void setStatus(bool aborted);
    void fileStatusChanged(bool opened, bool hasTags);
    void showResultsContextMenu(const QPoint &pos);
#ifndef QT_NO_CLIPBOARD
    void copySelectedResult();
#endif
    void fetchAndShowCoverForSelection();
    void fetchAndShowLyricsForSelection();
    void openSelectionInBrowser();
    void showCover(const QByteArray &data);
    void showCoverFromIndex(const QModelIndex &index);
    void showLyrics(const QString &data);
    void showLyricsFromIndex(const QModelIndex &index);

protected:
    bool event(QEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void useQueryResults(QueryResultsModel *queryResults);
    QModelIndex selectedIndex() const;

    std::unique_ptr<Ui::DbQueryWidget> m_ui;
    TagEditorWidget *m_tagEditorWidget;
    QueryResultsModel *m_model;
    int m_coverIndex, m_lyricsIndex;
    QMenu *m_menu;
    QAction *m_insertPresentDataAction;
    QAction *m_searchMusicBrainzAction;
    QAction *m_searchLyricsWikiaAction;
    QAction *m_searchMakeItPersonalAction;
    QAction *m_lastSearchAction;
    QAction *m_refreshAutomaticallyAction;
    QPoint m_contextMenuPos;
};

} // namespace QtGui

#endif // DBQUERYWIDGET_H
