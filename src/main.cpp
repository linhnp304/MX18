#include "core/AppPaths.h"
#include "core/Settings.h"
#include "ui/MainWindow.h"
#include "ui/SplashWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QFont>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("MX18"));
    QApplication::setOrganizationName(QStringLiteral("MX18"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0926"));

    AppPaths::ensureWritableDirs();
    Settings::instance().load();

    // Biểu tượng phần mềm nằm cạnh file chạy như các tài nguyên khác; Qt đọc
    // được .ico nên không phải đổi sang .png.
    QApplication::setWindowIcon(QIcon(AppPaths::resourceFile(QStringLiteral("RadarIcon.ico"))));

    // Đặt cỡ chữ trước khi nạp stylesheet: bảng kiểu tham chiếu cỡ chữ hiện hành.
    QFont f = QApplication::font();
    f.setPointSizeF(9.5);
    QApplication::setFont(f);
    Theme::apply();

    // Cửa sổ chính dựng sẵn trong lúc màn hình giới thiệu đang hiện, nhờ vậy 2
    // giây chờ cũng là 2 giây nạp nền bản đồ số.
    SplashWindow splash(AppPaths::resourceFile(QStringLiteral("FlashScreen.jpg")),
                        Settings::instance().swInfo().line0);
    splash.showFor(2000);
    app.processEvents();

    MainWindow window;
    window.loadMapData();

    QObject::connect(&splash, &SplashWindow::finished, &window, [&window] {
        window.showFullScreen();
    });

    return app.exec();
}
