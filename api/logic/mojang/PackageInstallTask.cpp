#include "PackageInstallTask.h"

#include <QThread>
#include <QFuture>
#include <QFutureWatcher>
#include <QRunnable>
#include <QtConcurrent>

#include <net/NetJob.h>
#include <Env.h>

#include "PackageManifest.h"

using Package = mojang_files::Package;

namespace {
struct PackageInstallTaskData
{
    QString root;
    QString version;
    QString packageURL;
    Net::Mode netmode;
    QFuture<Package> inspectionFuture;
    QFutureWatcher<Package> inspectionWatcher;
    NetJobPtr manifestDownloadJob;
    Package downloadedPackage;
};

class InspectFolder
{
public:
    InspectFolder(const QString & folder) : folder(folder) {}
    Package operator()()
    {
        return Package::fromInspectedFolder(folder);
    }
private:
    QString folder;
};

class ParsingValidator : public Net::Validator
{
public: /* con/des */
    ParsingValidator(Package &package) : m_package(package)
    {
    }
    virtual ~ParsingValidator() = default;

public: /* methods */
    bool init(QNetworkRequest &) override
    {
        m_package.valid = false;
        return true;
    }
    bool write(QByteArray & data) override
    {
        this->data.append(data);
        return true;
    }
    bool abort() override
    {
        return true;
    }
    bool validate(QNetworkReply &) override
    {
        m_package = Package::fromManifestContents(data);
        return m_package.valid;
    }

private: /* data */
    QByteArray data;
    Package &m_package;
};

}

PackageInstallTask::PackageInstallTask(
    Net::Mode netmode,
    QString version,
    QString packageURL,
    QString targetPath,
    QObject* parent
) : Task(parent) {
    d.reset(new PackageInstallTaskData);
    d->netmode = netmode;
    d->root = targetPath;
    d->packageURL = packageURL;
    d->version = version;
}

PackageInstallTask::~PackageInstallTask() {}

void PackageInstallTask::executeTask() {
    d->inspectionFuture = QtConcurrent::run(QThreadPool::globalInstance(), InspectFolder(d->root));
    connect(&d->inspectionWatcher, &QFutureWatcher<Package>::finished, this, &PackageInstallTask::inspectionFinished);
    connect(&d->inspectionWatcher, &QFutureWatcher<Package>::canceled, this, &PackageInstallTask::inspectionCancelled);
    d->inspectionWatcher.setFuture(d->inspectionFuture);

    // FIXME: offline mode
    d->manifestDownloadJob.reset(new NetJob(QObject::tr("Download of package manifest %1").arg(d->packageURL)));
    auto url = d->packageURL;
    auto dl = Net::Download::makeCached(url, entry);
    /*
     * The validator parses the file and loads it into the object.
     * If that fails, the file is not written to storage.
     */
    dl->addValidator(new ParsingValidator(d->downloadedPackage));
    d->manifestDownloadJob->addNetAction(dl);
    connect(&d->manifestDownloadJob, &NetJob::succeeded, [&]()
    {
        m_loadStatus = LoadStatus::Remote;
        m_updateStatus = UpdateStatus::Succeeded;
        m_updateTask.reset();
    });
    QObject::connect(job, &NetJob::failed, [&]()
    {
        m_updateStatus = UpdateStatus::Failed;
        m_updateTask.reset();
    });
    m_updateTask->start();

    d->manifestDownloadJob.reset(new NetJob("Package Manifest"));
    d->manifestDownloadJob->addNetAction();
}

void PackageInstallTask::inspectionFinished() {
    emitSucceeded();
}

void PackageInstallTask::inspectionCancelled() {
    emitAborted();
}
