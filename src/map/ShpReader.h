#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

// Bộ đọc shapefile tối giản: chỉ lấy hình học 2 chiều (bỏ Z/M) của các kiểu
// Point/PolyLine/Polygon. Máy build không có shapelib/GDAL nên phải tự đọc.
namespace ShpReader {

enum ShapeKind {
    Null = 0,
    Point = 1,
    PolyLine = 3,
    Polygon = 5,
    MultiPoint = 8,
};

struct Feature {
    // Mỗi phần tử là một "part": đường gấp khúc hoặc một vòng của đa giác.
    QVector<QVector<QPointF>> parts;
};

// shapeTypeOut nhận kiểu hình học chính của file (theo header).
bool read(const QString &shpPath, QVector<Feature> *out, int *shapeTypeOut);

} // namespace ShpReader
