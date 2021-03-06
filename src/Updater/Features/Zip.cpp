/****************************************************************************
**
** Copyright (C) 2015 Yash Pal, Speedovation
** Copyright (C) 2012 Linas Valiukas
**
** Contact: Speedovation Lab (info@speedovation.com)
**
** KineticWing Auto Updater
** http:// kineticwing.com
** This file is part of the KiWi Editor (IDE)
**
** Author: Yash Pal, Linas Valiukas
** License : Apache License 2.0
**
** All rights are reserved.
**
**/

#include "Zip.h"
#include "UpdaterWindow.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QNetworkReply>
#include <QVector>
#include <QDebug>
#include <QUuid>

#ifdef Q_OS_MAC
#include "CoreFoundation/CoreFoundation.h"
#endif

#include "zip_file.hpp"


Zip::Zip(UpdaterWindow* window) : d(window)
{

}

Zip::~Zip()
{

}

void  Zip::extractAll(zip_file *zipFile)
{

    QVector<zip_info> files =  QVector<zip_info>::fromStdVector(zipFile->infolist());

    QString random = QDir::tempPath() + "/" + QUuid::createUuid().toString() + "/" ;

    QDir dir;
    dir.mkdir(random);

    foreach (const zip_info &f , files )
    {

        if( f.crc > 0 )
        {
            qDebug() <<"File:" <<  QString::fromStdString(f.filename);;
            QFile file(  random  + QString::fromStdString(f.filename));

            if (!file.open(QIODevice::WriteOnly))
            {
                qDebug() << file.errorString() << " :error" <<  random +  QString::fromStdString(f.filename);
                continue;
            }

            QDataStream out(&file);
            out.setVersion(QDataStream::Qt_5_4);

            zipFile->getData(f,&out);

        }
        else
        {
            dir.mkdir( random + QString::fromStdString(f.filename));
            qDebug() << "Dir: " <<  QString::fromStdString(f.filename);;
        }


    }

    d->manager()->actionUpdate()->getUpdateInfo().localFolderPath = random;

    dir.mkdir(random + "rollback/");

    copyDir(qApp->applicationDirPath() , random + "rollback/");



}

/**
 * @brief Zip::copyDir
 * @param src
 * @param dest
 * @return bool
 *
 *  Authors: wysota , Yash
 *
 *  I've corrected few problems in original one
 */
bool Zip::copyDir(const QString &src, const QString &dest)
{

    QDir dir(src);

    QDir dirdest(dest);

    if(!dir.isReadable())
        return false;

    QFileInfoList entries = dir.entryInfoList();

    for(QList<QFileInfo>::iterator it = entries.begin(); it!=entries.end();++it)
    {
        QFileInfo &fileInfo = *it;

        if(fileInfo.fileName()=="." || fileInfo.fileName()=="..")
        {
            continue;
        }
        else if(fileInfo.isDir())
        {
            qDebug() << dest + fileInfo.fileName() ;
            QDir d;
            d.mkdir(dest + "/" + fileInfo.fileName());

            copyDir(fileInfo.filePath(),dest + "/" + fileInfo.fileName());
            continue;
        }
        else if(fileInfo.isSymLink())
        {
            continue;
        }
        else if(fileInfo.isFile() && fileInfo.isReadable())
        {

            QFile file(fileInfo.filePath());
            file.copy(dirdest.absoluteFilePath(fileInfo.fileName()));
        }
        else
            return false;

    }

    return true;
}


/*!
 * \brief Zip::extract
 *
 * \param reply
 */

void Zip::extract(QNetworkReply* reply)
{


#ifdef Q_OS_MAC
    //    CFURLRef appURLRef = CFBundleCopyBundleURL(CFBundleGetMainBundle());
    //    char path[PATH_MAX];
    //    if (!CFURLGetFileSystemRepresentation(appURLRef, TRUE, (UInt8 *)path, PATH_MAX)) {
    //        // error!
    //    }

    //    CFRelease(appURLRef);
    //    QString filePath = QString(path);
    //    QString rootDirectory = filePath.left(filePath.lastIndexOf("/"));

    QDir dir = QDir(QCoreApplication::applicationDirPath());
    dir.cdUp();
    dir.cdUp();


#else
    QString rootDirectory = QCoreApplication::applicationDirPath() + "/";
#endif

    // Write download into File
    QFileInfo fileInfo=reply->url().path();
    qDebug() << reply->url().path();
    QString fileName = rootDirectory + fileInfo.fileName();
    qDebug()<<"Writing downloaded file into "<<fileName;

    QFile file(fileName);
    file.open(QIODevice::WriteOnly);
    file.write(reply->readAll());
    file.close();

    zip_file zipFile(fileName.toStdString());
    try
    {

        std::vector <zip_info> updateFiles = zipFile.infolist();

        // Rename all current files with available update.
        for (size_t i=0;i<updateFiles.size();i++)
        {
            QString sourceFilePath = rootDirectory + QString::fromStdString(updateFiles[i].filename);
            QDir appDir( QCoreApplication::applicationDirPath() );

            QFileInfo file(	sourceFilePath );
            if(file.exists())
            {
                //qDebug()<<tr("Moving file %1 to %2").arg(sourceFilePath).arg(sourceFilePath+".oldversion");
                appDir.rename( sourceFilePath, sourceFilePath+".oldversion" );
            }
        }

        // Install updated Files
        extractAll(&zipFile);



    }

    catch(std::exception const& e)
    {
        qDebug() << "Exception: " << e.what() << "\n";
    }


    // Delete update archive
    while(QFile::remove(fileName) )
    {
    };



    /// Restart extracted Updater in install mode
    /// ./extractedpath/TARGET appPath extractedpath


    QApplication::exit(200);

    // Restart ap to clean up and start usual business
    ///d->manager()->helper()->restartApplication();
}
