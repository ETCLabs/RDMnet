/******************************************************************************
 * Copyright 2020 ETC Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************
 * This file is a part of RDMnet. For more information, go to:
 * https://github.com/ETCLabs/RDMnet
 *****************************************************************************/

#include "ControllerLog.h"
#include "ControllerUtils.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <QTimeZone>
#include <QtGlobal>
END_INCLUDE_QT_HEADERS()

static const QString kLogFileBasename = "controller.log";

#if defined(Q_OS_WIN)
QString GetLogFileName()
{
  static const QString kRelativeLogFilePath = "ETC/RDMnet Examples";

  QDir file_dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  if (file_dir.mkpath(kRelativeLogFilePath))
  {
    return QDir::toNativeSeparators(file_dir.absoluteFilePath(kRelativeLogFilePath + "/" + kLogFileBasename));
  }
  return QString{};
}
#elif defined(Q_OS_MACOS)
QString GetLogFileName()
{
  static const QString kRelativeLogFilePath = "Library/Logs/ETC/RDMnetExamples";

  auto home_dir = QDir::home();
  if (home_dir.mkpath(kRelativeLogFilePath))
  {
    return home_dir.absoluteFilePath(kRelativeLogFilePath + "/" + kLogFileBasename);
  }
  return QString{};
}
#elif defined(Q_OS_LINUX)
QString GetLogFileName()
{
  static const QString kRelativeLogFilePath = "rdmnet-examples";

  QDir file_dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  if (file_dir.mkpath(kRelativeLogFilePath))
  {
    return file_dir.absoluteFilePath(kRelativeLogFilePath + "/" + kLogFileBasename);
  }
  return QString{};
}
#endif

etcpal::LogTimestamp ControllerLog::GetLogTimestamp()
{
  QDateTime now = QDateTime::currentDateTime();
  QDate     qdate = now.date();
  QTime     qtime = now.time();

  return etcpal::LogTimestamp(qdate.year(), qdate.month(), qdate.day(), qtime.hour(), qtime.minute(), qtime.second(),
                              qtime.msec(), QTimeZone::systemTimeZone().offsetFromUtc(now) / 60);
}

ControllerLog::ControllerLog()
{
  file_name_ = GetLogFileName();
  if (!file_name_.isEmpty())
    file_.open(file_name_.toStdString().c_str(), std::fstream::out);

  logger_.SetLogAction(ETCPAL_LOG_CREATE_HUMAN_READABLE).SetLogMask(ETCPAL_LOG_UPTO(ETCPAL_LOG_DEBUG)).Startup(*this);

  logger_.Info("Starting RDMnet Controller...");
}

ControllerLog::~ControllerLog()
{
  logger_.Shutdown();
  file_.close();
}

void ControllerLog::HandleLogMessage(const EtcPalLogStrings& strings)
{
  if (file_.is_open())
  {
    file_ << strings.human_readable << std::endl;
    file_.flush();
  }

  for (LogOutputStream* stream : customOutputStreams)
  {
    if (stream)
      (*stream) << strings.human_readable << "\n";
  }
}

void ControllerLog::addCustomOutputStream(LogOutputStream* stream)
{
  if (stream)
  {
    if (std::find(customOutputStreams.begin(), customOutputStreams.end(), stream) == customOutputStreams.end())
    {
      // Reinitialize the stream's contents to the log file's contents.
      stream->clear();

      std::ifstream ifs(file_name_.toStdString(), std::ifstream::in);

      std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

      (*stream) << str;

      ifs.close();

      customOutputStreams.push_back(stream);
    }
  }
}

void ControllerLog::removeCustomOutputStream(LogOutputStream* stream)
{
  for (size_t i = 0; i < customOutputStreams.size(); ++i)
  {
    if (customOutputStreams.at(i) == stream)
    {
      customOutputStreams.erase(customOutputStreams.begin() + i);
    }
  }
}

size_t ControllerLog::getNumberOfCustomLogOutputStreams()
{
  return customOutputStreams.size();
}
