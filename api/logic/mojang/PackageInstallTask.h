#pragma once

#include "tasks/Task.h"
#include "net/Mode.h"

#include <memory>

namespace {
struct PackageInstallTaskData;
}

class PackageInstallTask : public Task
{
    Q_OBJECT
public:
    enum class Mode
    {
        Launch,
        Resolution
    };

public:
    explicit PackageInstallTask(
        Net::Mode netmode,
        QString version,
        QString packageURL,
        QString targetPath,
        QObject *parent = 0
    );
    virtual ~PackageInstallTask();

protected:
    void executeTask() override;

private slots:
    void inspectionFinished();
    void inspectionCancelled();

private:
    std::unique_ptr<::PackageInstallTaskData> d;
};


