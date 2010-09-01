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
struct TraceEntry;

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

private slots:
    void handleNewTraceEntry(const TraceEntry &e);
    void insertNewTraceEntries();

private:
    bool queryForEntries(QString *errMsg);

    QSqlDatabase m_db;
    QSqlQuery m_query;
    int m_querySize;
    Server *m_server;
    QList<TraceEntry> m_newTraceEntries;
};

#endif
