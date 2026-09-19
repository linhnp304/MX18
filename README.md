# MX18 — Phần mềm trắc thủ ra đa

Phần mềm hiển thị và điều khiển cho trắc thủ ra đa, viết bằng **C++17 / Qt 6**,
biên dịch được trên cả **Windows 10+** và **Ubuntu 24.04+** từ cùng một mã nguồn.

Kho này chỉ chứa mã nguồn. Dữ liệu chạy (bản đồ số, ảnh, cấu hình) nằm cạnh file
chạy trên máy đích, xem mục [Thư mục chạy](#thư-mục-chạy).

## Trạng thái

**Giai đoạn 1** — layout 3 panel và nền bản đồ số:

- Màn hình giới thiệu 2 giây rồi mở cửa sổ chính ở chế độ toàn màn hình.
- Panel 1: nền bản đồ số (9 lớp shapefile + sân bay + địa danh), vòng cự ly,
  đường chia độ, zoom bằng thanh trượt hoặc con lăn chuột, kéo khung nhìn bằng
  chuột trái.
- Panel 2: các tab **Danh sách / Điều khiển / Ghi lưu / Cài đặt** và cửa sổ biên độ.
- Panel 3: thanh trạng thái với các cửa sổ popup trạng thái, đồng hồ hệ thống,
  toạ độ con trỏ, góc đường quét, nút ẩn/hiện panel 2 và nút toạ độ tâm đài.

**Giai đoạn 2** — kết nối dữ liệu, điều khiển và hiển thị video:

- Quản lý kết nối theo `settings/connect.json`: mỗi cổng gửi/nhận một luồng
  riêng, UDP (unicast/broadcast) và TCP (server/client).
- Tab "Điều khiển": đầy đủ các nhóm lệnh **CMD_AT** (ăng ten) và **CMD_USER**
  (MH, mã hỏi đáp, phát, hệ thống phát hiện). Mỗi lần đổi một điều khiển thì cả
  gói lệnh được đóng lại và gửi đi một lần.
- Cửa sổ **"Điều khiển và thiết lập mức kỹ sư"**: cửa sổ nổi, không chặn giao
  diện chính; tab "Connect" sửa được bảng cổng gửi/nhận và danh sách nút mạng.
- Cửa sổ **"Trạng thái MH"** đổ dữ liệu gói **STATUS_MH**, giá trị vượt ngưỡng
  trong `settings/statuserror.json` đổi sang màu đỏ.
- Đường quét **RD** (xanh biển) và **MH** (xanh lá) vẽ trên nền bản đồ, 600 điểm
  biên độ của gói **VIDEO_I** vẽ đồng bộ theo đường quét MH và mờ dần theo thanh
  trượt "Tốc độ mờ video"; cùng dữ liệu đó hiện trên cửa sổ biên độ (panel 2.2).

## Yêu cầu biên dịch

| | Windows | Ubuntu |
|---|---|---|
| Trình biên dịch | MSVC 2019 trở lên | g++ 11 trở lên |
| Qt | 6.4 trở lên (`qtbase`) | 6.4 trở lên (`qt6-base-dev`) |
| CMake | 3.19 trở lên | 3.19 trở lên |

Phần mềm **chỉ dùng Qt base** (Core, Gui, Widgets, Network). Không cần vcpkg,
conan, GDAL, PROJ hay shapelib — bộ đọc shapefile/DBF và các phép chiếu toạ độ
đều nằm trong mã nguồn.

### Ubuntu

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build \
    qt6-base-dev qt6-base-dev-tools libgl1-mesa-dev

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Để **chạy** bản dựng sẵn tải từ GitHub Actions (artifact `MX18-ubuntu-x86_64`)
trên một máy Ubuntu chưa có Qt:

```bash
sudo apt update
sudo apt install -y libqt6core6 libqt6gui6 libqt6widgets6 libqt6network6 \
    libgl1 libxkbcommon-x11-0 libxcb-cursor0 libxcb-icccm4 libxcb-image0 \
    libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 libxcb-shape0 \
    libxcb-xinerama0 fonts-dejavu-core
chmod +x MX18 && ./MX18
```

### Windows

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Artifact `MX18-windows-x64` do GitHub Actions dựng đã kèm đầy đủ thư viện Qt
(`windeployqt`): giải nén, đặt cạnh thư mục dữ liệu rồi chạy `MX18.exe`, không
cần cài thêm gì.

> GitHub Actions dựng bản Windows bằng **Qt 6.8 LTS** chứ không phải 6.4, vì các
> file CMake của Qt 6.4 không còn cấu hình được với CMake 4 trên runner. Mức API
> tối thiểu **Qt 6.4** vẫn được job Ubuntu kiểm chứng (Ubuntu 24.04 đóng gói đúng
> Qt 6.4), nên mã nguồn vẫn biên dịch được bằng Qt 6.4 trên máy có CMake 3.x.

## Thư mục chạy

Đường dẫn dữ liệu tính **tương đối với file chạy**:

```
MX18(.exe)
├── maps/mc/        dữ liệu bản đồ số (shapefile, Diadanh.txt, Airport2.dat)
├── resources/      FlashScreen.jpg, logo.png
├── settings/       swinfo.json, setups.json, checkip.json, connect.json,
│                   setupadmin.json, statuserror.json, params.json
├── logs/           log và thông báo hệ thống
└── records/        file ghi lưu
```

`settings/` được tạo tự động với giá trị mặc định trong lần chạy đầu tiên.
`maps/mc` và `resources/` phải chép sang máy đích cùng file chạy.

### Các file cấu hình

| File | Nội dung |
|---|---|
| `swinfo.json` | `info_line0` (chữ trên màn hình giới thiệu), `info_line1`/`info_line2` (ô thông tin phần mềm góc trên bên trái panel 1) |
| `setups.json` | Toàn bộ lựa chọn trong tab "Cài đặt", bảng màu và toạ độ tâm đài |
| `checkip.json` | Danh sách nút mạng cần ping: `name`, `address`, `kind` (0 không cảnh báo, 1 cảnh báo, 2 báo lỗi) |
| `connect.json` | Bảng cổng gửi/nhận cho từng loại dữ liệu, và `big_endian` (thứ tự byte của gói tin) |
| `setupadmin.json` | Thiết lập cửa sổ mức kỹ sư, kể cả mật khẩu (mặc định `X18`) |
| `statuserror.json` | Ngưỡng báo lỗi của cửa sổ "Trạng thái MH": `Min50V`, `Max50V`, `Min5V`, `Max5V`, `MinCs`, `MaxT`, `MaxH` |
| `params.json` | Tham số đài (tab "Params" của cửa sổ kỹ sư, giai đoạn sau) |

Thiếu file nào thì phần mềm tự tạo file đó với giá trị mặc định. File **đã có mà
đọc lỗi** (sai cú pháp json, hỏng file) thì phần mềm vẫn chạy bằng giá trị mặc
định và đẩy một dòng `[Lỗi]` vào cửa sổ "Thông báo hệ thống" để trắc thủ gọi kỹ
sư sửa — `connect.json` và `checkip.json` **chỉ đọc lúc khởi động**, sửa xong
phải chạy lại phần mềm.

## Ghi chú kỹ thuật

- **Phép chiếu**: các lớp bản đồ không cùng hệ toạ độ (Lambert Conformal Conic,
  Transverse Mercator, WGS84 địa lý) nên `.prj` được đọc và nghịch chuyển về
  WGS84 lúc nạp. Khi vẽ, toàn bộ hình học chiếu sang mặt phẳng *phương vị cách
  đều* lấy tâm đài làm gốc, nhờ vậy vòng cự ly là đường tròn chính xác và số
  phương vị/cự ly hiển thị khớp với vị trí vẽ.
- **Hiệu năng**: ~680 nghìn điểm hình học được rút gọn sẵn theo 4 mức chi tiết
  (Douglas-Peucker) và loại theo khung bao trước khi vẽ; nền bản đồ được kết xuất
  vào một pixmap đệm, chỉ dựng lại khi khung nhìn hoặc tuỳ chọn thay đổi.
- **Biểu tượng**: mọi ký hiệu (sân bay, nút trạng thái) vẽ bằng `QPainterPath`
  để đổi màu theo trạng thái và không phụ thuộc file ảnh.
- **Gói tin**: khung cố định 24 byte (`header`, `category`, `length`, `serial`,
  `time`, `checksum`) bọc quanh `data_fields[]`, mỗi trường 4 byte. Thứ tự byte
  mặc định **big-endian**, đổi được bằng khoá `big_endian` trong `connect.json`.
  `checksum` hiện gán 0 và chưa kiểm tra.
- **Video**: đường quét MH đến khoảng 400 gói/giây. Mỗi tia được vẽ bằng một
  phép biến đổi quay + giãn của một ảnh 600×1 điểm — rẻ hơn nhiều so với 600 đoạn
  thẳng cho mỗi tia — vào một lớp ARGB riêng; lớp này mờ dần bằng phép **trừ**
  alpha (không phải nhân, vì phép nhân số nguyên đứng lại ở mức alpha thấp và để
  lại vệt xanh không bao giờ tắt) và được ghép lên nền bản đồ ở nhịp 25 hình/giây.
