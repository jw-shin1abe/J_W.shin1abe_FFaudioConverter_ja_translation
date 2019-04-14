/* ----------------------------------------------------------------------
FFaudioConverter
Copyright (C) 2018-2019  Bleuzen
https://github.com/Bleuzen/FFaudioConverter
supgesu@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
---------------------------------------------------------------------- */

#include "ffmpegtask.h"
#include "mainwindow.h"

#include <QMessageBox>

FFmpegTask::FFmpegTask(int id, QString inpath)
{
    this->id = id;
    this->infile = inpath;
    this->outdir = Settings::OutputDirectory;
}

void FFmpegTask::run()
{
    // Skip if input file can not be an audio or video file
    if(!Util::mayBeAudioOrVideoFile(infile)) {
        qDebug() << "Skipping job" << id;
        emit ConvertDone(id, ConvertStatus::Skipped);
        return;
    }

    // Prepare some stuff (fill variables and ffmpeg arguments...)
    prepare();

    qDebug() << "Starting job" << id << "|" << infile << "->" << outfile;

    // Create output dir if it does not exist
    QDir().mkpath(outdir);

    // Skip this convert job under some conditions in fast mode
    if(Settings::QuickConvertMode) {
        // Skip file if it has already been converted
        if(QFile::exists(outfile)) {
            qDebug() << "Skipping job" << id;
            emit ConvertDone(id, ConvertStatus::Skipped);
            return;
        }
        // Copy file if input and output format are the same
        if(infileExt.toLower() == outfileExt) {
            qDebug() << "Copy file of job" << id;
            if(QFile::copy(infile, outfile)) {
                emit ConvertDone(id, ConvertStatus::Done);
            } else {
                emit ConvertDone(id, ConvertStatus::Failed);
            }
            return;
        }
    }

    // Create FFmpeg process
    QProcess *ffmpeg = new QProcess();

    ffmpeg->setProgram(Settings::FFmpegBinary);
    ffmpeg->setArguments(ffmpegArgs);

    QString result;
    ffmpeg->setProcessChannelMode(QProcess::MergedChannels);
    QObject::connect(ffmpeg, &QProcess::readyReadStandardOutput, [ffmpeg,&result]()->void{result+=ffmpeg->readAllStandardOutput();});

    // Run FFmpeg process
    ffmpeg->start();
    ffmpeg->waitForFinished(-1);
    int exitCode = ffmpeg->exitCode();

    qDebug() << "Result of job" << id << "|" << result;
    qDebug() << "Finished job" << id << "with exit code" << exitCode;

    bool success = (exitCode == 0);
    if(success) {
        emit ConvertDone(id, ConvertStatus::Done);
    } else {
        emit ConvertDone(id, ConvertStatus::Failed);
    }
}

void FFmpegTask::prepare() {
    QFileInfo infileInfo(infile);
    QDir qdir = infileInfo.absoluteDir();
    QString sourcedir = qdir.path();
    QString basename = infileInfo.completeBaseName();
    infileExt = infileInfo.suffix();
#ifdef Q_OS_WIN
    // Fix path on Windows
    if(!outdir.startsWith("{sourcedir}")) {
        sourcedir = sourcedir.replace(":", "");
    }
#endif
    outdir = outdir.replace("{sourcedir}", sourcedir);  // Allow to use source directory as variable in output directory
    outfile = outdir + QDir::separator() + basename;  // Output file path without extension (will be added later)

    ffmpegArgs << "-hide_banner";
    if(Settings::QuickConvertMode) {
        ffmpegArgs<< "-n";  // Not overwrite (keep existing file)
    } else {
        ffmpegArgs<< "-y";  // Overwrite (reconvert if file exists)
    }
    ffmpegArgs << "-i" << infile;

    if(Settings::OutputFormat == "mp3") {
        outfileExt = "mp3";
        ffmpegArgs << "-c:a" << "libmp3lame";
        if(wantCustomQualityOptions()) {
            addCustomQualityOptions();
        } else if(Settings::Quality == "extreme") {
            ffmpegArgs << "-b:a" << "320k";
        } else if(Settings::Quality == "high") {
            ffmpegArgs << "-q:a" << "1";
        } else if(Settings::Quality == "medium") {
            ffmpegArgs << "-q:a" << "4";
        }
        if (!Util::isNullOrEmpty(Settings::OutputSamplerate)) ffmpegArgs << "-ar" << Settings::OutputSamplerate;
        ffmpegArgs << "-map_metadata" << "0";
        ffmpegArgs << "-map_metadata" << "0:s:0";
        ffmpegArgs << "-id3v2_version" << "3";

    } else if(Settings::OutputFormat == "m4a") {
        outfileExt = "m4a";
        ffmpegArgs << "-c:a" << "aac";
        ffmpegArgs << "-vn";  // removes cover art but is the only way I know to get m4a working
        if(wantCustomQualityOptions()) {
            addCustomQualityOptions();
        } else if(Settings::Quality == "extreme") {
            ffmpegArgs << "-b:a" << "256k";
        } else if(Settings::Quality == "high") {
            ffmpegArgs << "-b:a" << "192k";
        } else if(Settings::Quality == "medium") {
            ffmpegArgs << "-b:a" << "128k";
        }
        if (!Util::isNullOrEmpty(Settings::OutputSamplerate)) ffmpegArgs << "-ar" << Settings::OutputSamplerate;
        ffmpegArgs << "-map_metadata" << "0";
        ffmpegArgs << "-map_metadata" << "0:s:0";

    } else if(Settings::OutputFormat == "ogg") {
        outfileExt = "ogg";
        ffmpegArgs << "-vn"; //TODO: remove this to keep album art but without the output is a video
        ffmpegArgs << "-c:a" << "libvorbis";
        if(wantCustomQualityOptions()) {
            addCustomQualityOptions();
        } else if(Settings::Quality == "extreme") {
            ffmpegArgs << "-q:a" << "8";
        } else if(Settings::Quality == "high") {
            ffmpegArgs << "-q:a" << "6";
        } else if(Settings::Quality == "medium") {
            ffmpegArgs << "-q:a" << "4";
        }
        if (!Util::isNullOrEmpty(Settings::OutputSamplerate)) ffmpegArgs << "-ar" << Settings::OutputSamplerate;
        ffmpegArgs << "-map_metadata" << "0";
        ffmpegArgs << "-map_metadata" << "0:s:0";

    } else if(Settings::OutputFormat == "opus") {
        outfileExt = "opus";
        ffmpegArgs << "-c:a" << "libopus";  //TODO: opus currently loses album art (at least with ffmpeg 4.1)
        if(wantCustomQualityOptions()) {
            addCustomQualityOptions();
        } else if(Settings::Quality == "extreme") {
            ffmpegArgs << "-b:a" << "192k";
        } else if(Settings::Quality == "high") {
            ffmpegArgs << "-b:a" << "160k";
        } else if(Settings::Quality == "medium") {
            ffmpegArgs << "-b:a" << "128k";
        }
        // ChangeSamplerate not possible for opus (always uses 48 kHz)
        ffmpegArgs << "-map_metadata" << "0";
        ffmpegArgs << "-map_metadata" << "0:s:0";

    } else if(Settings::OutputFormat == "flac") {
        outfileExt = "flac";
        ffmpegArgs << "-c:a" << "flac";
        if(wantCustomQualityOptions()) {
            addCustomQualityOptions();
        } else if (Settings::Quality == "medium") {
            ffmpegArgs << "-sample_fmt" << "s16";
        }
        if (!Util::isNullOrEmpty(Settings::OutputSamplerate)) ffmpegArgs << "-ar" << Settings::OutputSamplerate;
        ffmpegArgs << "-map_metadata" << "0";
        ffmpegArgs << "-map_metadata" << "0:s:0";

    } else if(Settings::OutputFormat == "wav") {
        outfileExt = "wav";
        if(wantCustomQualityOptions()) {
            if(Settings::CustomQualityArguments == "32") {
                ffmpegArgs << "-c:a" << "pcm_s32le";
            } else if(Settings::CustomQualityArguments == "24") {
                ffmpegArgs << "-c:a" << "pcm_s24le";
            } else {
                ffmpegArgs << "-c:a" << "pcm_s16le";
            }
        } else {
            ffmpegArgs << "-c:a" << "pcm_s16le";
        }
        if (!Util::isNullOrEmpty(Settings::OutputSamplerate)) ffmpegArgs << "-ar" << Settings::OutputSamplerate;

    } else {
        // unknown format options (this should never happen)
        outfileExt = Settings::OutputFormat;
    }

    // Add SoX
    if(Settings::UseSoXresampler) {
        audioFilters.append("aresample=resampler=soxr ");  // Space at the end is important to allow further audio filters
    }

    // Add user audio filters
    audioFilters.append(Settings::AudioFilters);

    // Apply audio filters
    QString audioFiltersArg = audioFilters.simplified().replace(" ", ",");  // Convert newlines to spaces and spaces to commas
    if(!Util::isNullOrEmpty(audioFiltersArg)) {
        ffmpegArgs << "-af" << audioFiltersArg;
    }

    // Set output file
    outfile += "." + outfileExt;
    ffmpegArgs << outfile;
}

bool FFmpegTask::wantCustomQualityOptions() {
    return Settings::Quality == "custom" && !Settings::CustomQualityArguments.isEmpty();
}

void FFmpegTask::addCustomQualityOptions() {
    ffmpegArgs << Settings::CustomQualityArguments.split(" ");
}
