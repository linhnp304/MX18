#pragma once

#include <QColor>

// Bảng màu nền bản đồ theo mức sáng 1..10.
//
// Hai đầu mút lấy từ ảnh mẫu docs/images/map_dark.png (mức 1) và map_light.png
// (mức 10); các mức ở giữa nội suy tuyến tính. Màu đường bờ biển/biên giới và
// ký hiệu sân bay giữ nguyên ở mọi mức vì trong ảnh mẫu chúng không đổi.
struct MapPalette {
    QColor sea;
    QColor land;
    QColor coast;      // bờ biển + đường biên giới + ranh giới quốc gia
    QColor province;   // ranh giới tỉnh
    QColor river;
    QColor airRoute;
    QColor placeText;
    QColor routeText;
    QColor airportVn;
    QColor airportForeign;
    QColor airportTextVn;
    QColor airportTextForeign;

    static MapPalette forBrightness(int level);
};
