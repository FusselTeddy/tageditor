#include "./renamingengine.h"
#include "./filesystemitemmodel.h"
#include "./filteredfilesystemitemmodel.h"
#include "./tageditorobject.h"

#include <QDir>
#include <QStringBuilder>

#include <memory>

using namespace std;

namespace RenamingUtility {

RenamingEngine::RenamingEngine(QObject *parent)
    : QObject(parent)
    ,
#ifndef TAGEDITOR_NO_JSENGINE
    m_tagEditorQObj(new TagEditorObject(&m_engine))
    , m_tagEditorJsObj(TAGEDITOR_JS_QOBJECT(m_engine, m_tagEditorQObj))
    ,
#endif
    m_itemsProcessed(0)
    , m_errorsOccured(0)
    , m_aborted(false)
    , m_includeSubdirs(false)
    , m_isBusy(false)
    , m_model(nullptr)
    , m_currentModel(nullptr)
    , m_previewModel(nullptr)
{
#ifndef TAGEDITOR_NO_JSENGINE
    m_engine.globalObject().setProperty(QStringLiteral("tageditor"), m_tagEditorJsObj);
#endif
    connect(this, &RenamingEngine::previewGenerated, this, &RenamingEngine::processPreviewGenerated);
    connect(this, &RenamingEngine::changingsApplied, this, &RenamingEngine::processChangingsApplied);
}

RenamingEngine::~RenamingEngine()
{
#ifndef TAGEDITOR_NO_JSENGINE
    for (auto *const child : children()) {
        if (auto *const childThread = qobject_cast<QThread *>(child)) {
            childThread->wait();
        }
    }
#endif
}

#ifndef TAGEDITOR_NO_JSENGINE
bool RenamingEngine::setProgram(const TAGEDITOR_JS_VALUE &program)
{
    if (program.isError()) {
        m_errorMessage = program.property(QStringLiteral("message")).toString();
        m_errorLineNumber = TAGEDITOR_JS_INT(program.property(QStringLiteral("lineNumber")));
        return false;
    } else if (!TAGEDITOR_JS_IS_VALID_PROG(program)) {
        m_errorMessage = tr("Program is not callable. Please don't close a function you didn't open.");
        m_errorLineNumber = 0;
        return false;
    }

    m_errorMessage.clear();
    m_errorLineNumber = 0;
    m_program = program;
    return true;
}
#endif

bool RenamingEngine::setProgram(const QString &program)
{
#ifndef TAGEDITOR_NO_JSENGINE
    return setProgram(m_engine.evaluate(QStringLiteral("(function(){") % program % QStringLiteral("})")));
#else
    Q_UNUSED(program)
    m_errorLineNumber = 0;
    m_errorMessage = tr("Not compiled with ECMA support.");
    return false;
#endif
}

bool RenamingEngine::generatePreview(const QDir &rootDirectory, bool includeSubdirs)
{
#ifndef TAGEDITOR_NO_JSENGINE
    if (m_isBusy) {
        return false;
    }
    setRootItem();
    m_includeSubdirs = includeSubdirs;
    m_dir = rootDirectory;

    (new PreviewGenerator(this))->start();
    return m_isBusy = true;
#else
    Q_UNUSED(rootDirectory)
    Q_UNUSED(includeSubdirs)
    return false;
#endif
}

bool RenamingEngine::applyChangings()
{
    if (!m_rootItem || m_isBusy) {
        return false;
    }
#ifndef TAGEDITOR_NO_JSENGINE
    (new RenamingThing(this))->start();
#endif
    return m_isBusy = true;
}

bool RenamingEngine::clearPreview()
{
    if (m_isBusy) {
        return false;
    }

    updateModel(nullptr);
    m_rootItem.reset();
    return true;
}

FileSystemItemModel *RenamingEngine::model()
{
    if (!m_model) {
        m_model = new FileSystemItemModel(m_rootItem.get(), this);
    }
    return m_model;
}

FilteredFileSystemItemModel *RenamingEngine::currentModel()
{
    if (!m_currentModel) {
        m_currentModel = new FilteredFileSystemItemModel(ItemStatus::Current, this);
        m_currentModel->setSourceModel(model());
    }
    return m_currentModel;
}

FilteredFileSystemItemModel *RenamingEngine::previewModel()
{
    if (!m_previewModel) {
        m_previewModel = new FilteredFileSystemItemModel(ItemStatus::New, this);
        m_previewModel->setSourceModel(model());
    }
    return m_previewModel;
}

void RenamingEngine::processPreviewGenerated()
{
    finalizeTaskCompletion();
    setRootItem(std::move(m_newlyGeneratedRootItem));
}

void RenamingEngine::processChangingsApplied()
{
    finalizeTaskCompletion();
    updateModel(nullptr);
    updateModel(m_rootItem.get());
}

void RenamingEngine::resetStatus()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    m_aborted.storeRelaxed(false);
#else
    m_aborted.store(false);
#endif
    m_itemsProcessed = 0;
    m_errorsOccured = 0;
}

void RenamingEngine::finalizeTaskCompletion()
{
#ifndef TAGEDITOR_NO_JSENGINE
    m_engine.moveToThread(thread());
#endif
    m_isBusy = false;
}

inline void RenamingEngine::setRootItem(unique_ptr<FileSystemItem> &&rootItem)
{
    updateModel(rootItem.get());
    m_rootItem = std::move(rootItem);
}

void RenamingEngine::updateModel(FileSystemItem *rootItem)
{
    if (m_model) {
        m_model->setRootItem(rootItem);
    }
}

#ifndef TAGEDITOR_NO_JSENGINE
unique_ptr<FileSystemItem> RenamingEngine::generatePreview(const QDir &dir, FileSystemItem *parent)
{
    auto item = make_unique<FileSystemItem>(ItemStatus::Current, ItemType::Dir, dir.dirName(), parent);
    item->setApplied(false);
    for (const QFileInfo &entry : dir.entryInfoList()) {
        if (entry.fileName() == QLatin1String("..") || entry.fileName() == QLatin1String(".")) {
            continue;
        }
        FileSystemItem *subItem; // will be deleted by parent
        if (entry.isDir() && m_includeSubdirs) {
            subItem = generatePreview(QDir(entry.absoluteFilePath()), item.get()).release();
        } else if (entry.isFile()) {
            subItem = new FileSystemItem(ItemStatus::Current, ItemType::File, entry.fileName(), item.get());
            subItem->setApplied(false);
        } else {
            subItem = nullptr;
        }
        if (subItem) {
            executeScriptForItem(entry, subItem);
            if (subItem->errorOccured()) {
                ++m_errorsOccured;
            }
        }
        ++m_itemsProcessed;
        if (isAborted()) {
            return item;
        }
    }
    emit progress(m_itemsProcessed, m_errorsOccured);
    return item;
}
#endif

void RenamingEngine::applyChangings(FileSystemItem *parentItem)
{
    for (auto *const item : parentItem->children()) {
        if (!item->applied() && !item->errorOccured()) {
            switch (item->status()) {
            case ItemStatus::New: {
                const FileSystemItem *counterpartItem = item->counterpart(); // holds current name
                const QString currentPath = counterpartItem ? counterpartItem->relativePath() : QString();
                const QString newPath = item->relativePath();
                if (item->name().isEmpty()) {
                    // new item name mustn't be empty
                    item->setNote(tr("generated name is empty"));
                    item->setErrorOccured(true);
                } else if (counterpartItem && !counterpartItem->name().isEmpty()) {
                    // rename current item
                    if (item->parent() != counterpartItem->parent() || item->name() != counterpartItem->name()) {
                        if (m_dir.exists(newPath)) {
                            if (item->parent() == counterpartItem->parent()) {
                                item->setNote(tr("unable to rename, there is already an entry with the same name"));
                            } else {
                                item->setNote(tr("unable to move, there is already an entry with the same name"));
                            }
                            item->setErrorOccured(true);
                        } else if (m_dir.rename(currentPath, newPath)) {
                            if (item->parent() == counterpartItem->parent()) {
                                item->setNote(tr("renamed"));
                            } else {
                                item->setNote(tr("moved"));
                            }
                            item->setApplied(true);
                        } else {
                            item->setNote(tr("unable to rename"));
                            item->setErrorOccured(true);
                        }
                    } else {
                        item->setNote(tr("nothing to be changed"));
                        item->setApplied(true);
                    }
                } else if (item->type() == ItemType::Dir) {
                    // create new item, but only if its a dir
                    if (m_dir.exists(newPath)) {
                        item->setNote(tr("directory already existed"));
                        item->setApplied(true);
                    } else if (m_dir.mkpath(newPath)) {
                        item->setNote(tr("directory created"));
                        item->setApplied(true);
                    } else {
                        item->setNote(tr("unable to create directory"));
                        item->setErrorOccured(true);
                    }
                } else {
                    // can not create new file
                    item->setNote(tr("unable to create file"));
                    item->setErrorOccured(true);
                }
                break;
            }
            case ItemStatus::Current:
                break;
            }
        }
        if (item->errorOccured()) {
            ++m_errorsOccured;
        }
        // apply changings for child items as well
        if (item->type() == ItemType::Dir) {
            applyChangings(item);
        }
    }
    m_itemsProcessed += parentItem->children().size();
    emit progress(m_itemsProcessed, m_errorsOccured);
}

void RenamingEngine::setError(const QList<FileSystemItem *> items)
{
    for (auto *const item : items) {
        item->setErrorOccured(true);
        item->setNote(tr("skipped due to error of superior item"));
    }
}

#ifndef TAGEDITOR_NO_JSENGINE
void RenamingEngine::executeScriptForItem(const QFileInfo &fileInfo, FileSystemItem *item)
{
    // make file info for the specified item available in the script
    m_tagEditorQObj->setFileInfo(fileInfo, item);

    // execute script
    const auto scriptResult(m_program.call());
    if (scriptResult.isError()) {
        // handle error
        item->setErrorOccured(true);
        item->setNote(scriptResult.toString());
        return;
    }

    // create preview for action
    const QString &newName = m_tagEditorQObj->newName();
    const QString &newRelativeDirectory = m_tagEditorQObj->newRelativeDirectory();
    switch (m_tagEditorQObj->action()) {
    case ActionType::None:
        item->setNote(tr("no action specified"));
        break;
    case ActionType::Rename:
        if (!newRelativeDirectory.isEmpty()) {
            FileSystemItem *const counterpartParent = item->root()->makeChildAvailable(newRelativeDirectory);
            const QString &counterpartName = newName.isEmpty() ? item->name() : newName;
            if (const auto *const conflictingItem = counterpartParent->findChild(counterpartName, item)) {
                QString conflictingName;
                if (const auto *const conflictingCounterpart = conflictingItem->counterpart()) {
                    conflictingCounterpart->relativePath(conflictingName);
                } else {
                    conflictingName = conflictingItem->currentName();
                }
                item->setNote(tr("name is already used at new location by '%1'").arg(conflictingName));
                item->setErrorOccured(true);
            } else {
                auto *const counterpart = new FileSystemItem(ItemStatus::New, item->type(), counterpartName, counterpartParent);
                item->setCounterpart(counterpart);
                counterpart->setCheckable(true);
                counterpart->setChecked(true);
            }
        } else if (!newName.isEmpty()) {
            item->setNewName(newName);
        }
        if (FileSystemItem *const newItem = item->counterpart()) {
            if ((newItem->name().isEmpty() || newItem->name() == item->name()) && (newItem->parent() == item->parent())) {
                item->setNote(tr("name doesn't change"));
            } else if (newItem->parent() && newItem->parent()->findChild(newItem->name(), newItem)) {
                item->setNote(tr("generated name is already used"));
                item->setErrorOccured(true);
            } else if (newItem->parent() == item->parent()) {
                item->setNote(tr("will be renamed"));
                newItem->setCheckable(true);
                newItem->setChecked(true);
            } else {
                item->setNote(tr("will be moved"));
            }
        } else if (item->note().isEmpty()) {
            item->setNote(tr("can not be renamed"));
        }
        break;
    default:
        item->setNote(m_tagEditorQObj->note().isEmpty() ? tr("skipped") : m_tagEditorQObj->note());
    }
}

PreviewGenerator::PreviewGenerator(RenamingEngine *engine)
    : QThread(engine)
    , m_engine(engine)
{
    m_engine->m_engine.moveToThread(this);
    connect(this, &PreviewGenerator::finished, m_engine, &RenamingEngine::previewGenerated, Qt::QueuedConnection);
    connect(this, &PreviewGenerator::finished, this, &PreviewGenerator::deleteLater);
}

void PreviewGenerator::run()
{
    m_engine->resetStatus();
    m_engine->m_newlyGeneratedRootItem = m_engine->generatePreview(m_engine->m_dir);
}

RenamingThing::RenamingThing(RenamingEngine *engine)
    : QThread(engine)
    , m_engine(engine)
{
    m_engine->m_engine.moveToThread(this);
    connect(this, &RenamingThing::finished, m_engine, &RenamingEngine::changingsApplied, Qt::QueuedConnection);
    connect(this, &RenamingThing::finished, this, &RenamingThing::deleteLater);
}

void RenamingThing::run()
{
    m_engine->resetStatus();
    m_engine->applyChangings(m_engine->m_rootItem.get());
}

#endif

} // namespace RenamingUtility
