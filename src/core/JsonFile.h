#pragma once

#include <QJsonObject>
#include <QString>

// Đọc/ghi một file JSON cấu hình. Thiếu file hoặc file hỏng thì trả về đối
// tượng rỗng để lớp gọi tự điền mặc định rồi ghi đè lại.
namespace JsonFile {

QJsonObject read(const QString &path);
bool write(const QString &path, const QJsonObject &obj);

double num(const QJsonObject &o, const QString &key, double def);
int i(const QJsonObject &o, const QString &key, int def);
bool b(const QJsonObject &o, const QString &key, bool def);
QString str(const QJsonObject &o, const QString &key, const QString &def);

} // namespace JsonFile
