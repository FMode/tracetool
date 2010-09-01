/**********************************************************************
** Copyright (C) 2010 froglogic GmbH.
** All rights reserved.
**********************************************************************/

#ifndef ENTRYITEMMODEL_H
#define ENTRYITEMMODEL_H

#include <QAbstractTableModel>
#include <QSqlDatabase>
#include <QSqlQuery>

class Server;

class EntryItemModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    EntryItemModel(QObject *parent = 0);
    ~EntryItemModel();

    bool setDatabase(const QString &databaseFileName,
                     int serverPort,
                     QString *errMsg);

    int columnCount(const QModelIndex & parent = QModelIndex()) const;
    int rowCount(const QModelIndex & parent = QModelIndex()) const;

    QModelIndex index(int row, int column,
                      const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

private:
    bool queryForEntries(QString *errMsg);

    QSqlDatabase m_db;
    QSqlQuery m_query;
    int m_querySize;
    Server *m_server;
    
};

#endif
