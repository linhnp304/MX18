#pragma once

#include <QString>

// Mọi dữ liệu chạy (bản đồ, cấu hình, ảnh) đều nằm cạnh file chạy chứ không
// cạnh cây thư mục phát triển, vì máy trắc thủ chỉ nhận thư mục runtime.
namespace AppPaths {

QString appDir();
QString mapsDir();      // ./maps/mc
QString settingsDir();  // ./settings
QString resourcesDir(); // ./resources
QString logsDir();      // ./logs
QString recordsDir();   // ./records

QString settingsFile(const QString &name);
QString resourceFile(const QString &name);

// Tạo trước các thư mục ghi được; gọi một lần lúc khởi động.
void ensureWritableDirs();

} // namespace AppPaths
