#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// Bộ đọc bảng thuộc tính dBase III đi kèm shapefile.
//
// Bảng mã khai báo trong file .cpg cạnh đó và không thống nhất giữa các lớp
// (AirRoutes là CP1250, VNM_adm1 là UTF-8, số còn lại không khai báo). Qt 6 bỏ
// QTextCodec nên bảng CP1250 phải tự mang theo.
class DbfReader
{
public:
    bool open(const QString &dbfPath);

    int recordCount() const { return m_records.size(); }
    QStringList fieldNames() const { return m_fieldNames; }
    int fieldIndex(const QString &name) const;

    QString value(int record, int field) const;
    QString value(int record, const QString &field) const;

private:
    QString decode(const QByteArray &raw) const;

    QStringList m_fieldNames;
    QVector<QVector<QByteArray>> m_records;
    enum Codec { Utf8, Cp1250, Latin1 } m_codec = Latin1;
};
