/*
SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "masktask.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "pythoninterfaces/saminterface.h"

#include <KLocalizedString>
#include <KMessageWidget>
#include <QFile>
#include <QImage>
#include <QString>

MaskTask::MaskTask(const ObjectId &owner, QMap<int, QString> maskProperties, std::pair<QString, QString> scriptPath, int in, int out, QObject *object)
    : AbstractTask(owner, AbstractTask::MASKJOB, object)
    , m_properties(maskProperties)
    , m_scriptPath(scriptPath)
    , m_in(in)
    , m_out(out)
{
    m_description = i18n("Mask creation");
}

MaskTask::~MaskTask() {}

void MaskTask::start(const ObjectId &owner, QMap<int, QString> maskProperties, std::pair<QString, QString> scriptPath, int in, int out, QObject *object)
{
    MaskTask *task = new MaskTask(owner, maskProperties, scriptPath, in, out, object);
    pCore->taskManager.startTask(owner.itemId, task);
}

void MaskTask::generateMask()
{
    // Ensure we have the source frames
    QDir srcFolder(m_properties.value(MaskTask::INPUTFOLDER));
    if (!srcFolder.exists() || srcFolder.isEmpty()) {
        m_errorMessage = i18n("No source frames to process");
        return;
    }
    const QString outFile = m_properties.value(MaskTask::OUTPUTFILE);
    const QString outFramesFolder = m_properties.value(MaskTask::OUTPUTFOLDER);
    QStringList args = {m_scriptPath.second,
                        QStringLiteral("-I"),
                        m_properties.value(MaskTask::INPUTFOLDER),
                        QStringLiteral("-O"),
                        outFramesFolder,
                        QStringLiteral("-M"),
                        KdenliveSettings::samModelFile(),
                        QStringLiteral("-C"),
                        SamInterface::configForModel()};
    if (!m_properties.value(MaskTask::POINTS).isEmpty()) {
        args << QStringLiteral("-P") << m_properties.value(MaskTask::POINTS) << QStringLiteral("-L") << m_properties.value(MaskTask::LABELS);
    }
    if (!m_properties.value(MaskTask::BOX).isEmpty()) {
        args << QStringLiteral("-B") << m_properties.value(MaskTask::BOX);
    }
    if (!KdenliveSettings::samDevice().isEmpty()) {
        args << QStringLiteral("-D") << KdenliveSettings::samDevice();
    }
    qDebug() << "---- STARTING IMAGE GENERATION: " << args;
    qDebug() << "//// STARTING PREVIEW GENERATION WITH: " << args;
    m_scriptJob = std::make_unique<QProcess>(new QProcess);
    QObject::connect(this, &AbstractTask::jobCanceled, m_scriptJob.get(), &QProcess::kill, Qt::DirectConnection);
    QObject::connect(m_scriptJob.get(), &QProcess::readyReadStandardError, this, &MaskTask::processLogInfo);
    m_scriptJob->start(m_scriptPath.first, args);
    m_scriptJob->waitForFinished(-1);
    m_isFfmpegJob = true;
    // Now convert frames to video
    // ffmpeg -framerate 25 -pattern_type glob -i '*.png' -c:v ffv1 -pix_fmt yuva420p output.mkv
    args = {QStringLiteral("-y"),
            QStringLiteral("-framerate"),
            QString::number(pCore->getCurrentFps()),
            QStringLiteral("-pattern_type"),
            QStringLiteral("glob"),
            QStringLiteral("-i"),
            QStringLiteral("%1/*.png").arg(outFramesFolder),
            QStringLiteral("-c:v"),
            QStringLiteral("ffv1"),
            QStringLiteral("-pix_fmt"),
            QStringLiteral("yuva420p"),
            outFile};
    m_scriptJob->start(KdenliveSettings::ffmpegpath(), args);
    m_scriptJob->waitForFinished(-1);
    if (!QFile::exists(outFile)) {
        QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection,
                                  Q_ARG(QString, m_errorMessage.isEmpty() ? i18n("Failed to render mask %1", outFile) : m_errorMessage),
                                  Q_ARG(int, int(KMessageWidget::Warning)));
        return;
    }
    // Save thumbnail
    QDir framesFolder(outFramesFolder);
    const QString firstFrame = QStringLiteral("00000.png");
    if (framesFolder.exists(firstFrame)) {
        QImage img(framesFolder.absoluteFilePath(firstFrame));
        QString thumbFile = outFile.section(QLatin1Char('.'), 0, -2);
        thumbFile.append(QStringLiteral(".png"));
        img = img.scaledToHeight(80);
        img.save(thumbFile);
    }
    m_progress = 100;
    if (!m_isCanceled.loadAcquire()) {
        auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.itemId));
        MaskInfo mask;
        mask.maskName = m_properties.value(MaskTask::NAME);
        mask.maskFile = outFile;
        mask.in = m_in;
        mask.out = m_out;
        QMetaObject::invokeMethod(binClip.get(), "addMask", Qt::QueuedConnection, Q_ARG(MaskInfo, mask));
    }
}

void MaskTask::run()
{
    AbstractTaskDone whenFinished(m_owner.itemId, this);
    if (m_isCanceled || pCore->taskManager.isBlocked()) {
        return;
    }
    QMutexLocker lock(&m_runMutex);
    m_running = true;
    generateMask();
    return;
}

void MaskTask::processLogInfo()
{
    const QString buffer = QString::fromUtf8(m_scriptJob->readAllStandardError());
    m_logDetails.append(buffer);
    qDebug() << "=== GOT BUFFER OUTPUT: " << buffer << "\n________________";
    if (m_isFfmpegJob) {
        // Parse FFmpeg output
        if (m_jobDuration == 0) {
            if (buffer.contains(QLatin1String("Duration:"))) {
                QString data = buffer.section(QStringLiteral("Duration:"), 1, 1).section(QLatin1Char(','), 0, 0).simplified();
                if (!data.isEmpty()) {
                    QStringList numbers = data.split(QLatin1Char(':'));
                    if (numbers.size() < 3) {
                        return;
                    }
                    m_jobDuration = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + numbers.at(2).toInt();
                }
            }
        } else if (buffer.contains(QLatin1String("time="))) {
            int progress = 0;
            QString time = buffer.section(QStringLiteral("time="), 1, 1).simplified().section(QLatin1Char(' '), 0, 0);
            if (!time.isEmpty()) {
                QStringList numbers = time.split(QLatin1Char(':'));
                if (numbers.size() < 3) {
                    progress = time.toInt();
                    if (progress == 0) {
                        return;
                    }
                } else {
                    progress = numbers.at(0).toInt() * 3600 + numbers.at(1).toInt() * 60 + qRound(numbers.at(2).toDouble());
                }
            }
            int val = 100 * progress / m_jobDuration;
            if (m_progress != val) {
                m_progress = val;
                QMetaObject::invokeMethod(m_object, "updateJobProgress");
            }
        }
    } else {
        // Parse SAM2 output
        if (buffer.contains(QLatin1String("%|"))) {
            int val = buffer.section(QStringLiteral("%|"), 0, 0).simplified().section(QLatin1Char(' '), -1).toInt();
            if (m_progress != val) {
                m_progress = val;
                qDebug() << "::: MASK PROGRESS: " << val;
                QMetaObject::invokeMethod(m_object, "updateJobProgress");
            }
        } else {
            qDebug() << ":::: DOES NOT CONTAIN STRING...";
        }
    }
}
