#ifndef MOUNT_FSTYPE_H
#define MOUNT_FSTYPE_H

#include "_fstype.h"

class MountFsType : public _FsType
{
    Q_OBJECT
public:
    explicit MountFsType(
            QWidget *parent = 0,
            QString _type = QString());

public slots:
    QDomDocument getDevDocument() const;

private slots:
    void getSourcePath();
    void driverTypeChanged(QString);
};

#endif // MOUNT_FSTYPE_H
