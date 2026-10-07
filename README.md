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

**Giai đoạn 3** — điều khiển và trạng thái mức kỹ sư:

- Cửa sổ "Điều khiển và thiết lập mức kỹ sư" đủ sáu tab: **ADMIN / AD / SW /
  Other / Params / Connect**, gửi các gói **CMD_ADMIN**, **CMD_ADMIN_AD**,
  **CMD_ADMIN_SW**, **CMD_ADMIN_OTHER**, **CMD_ADMIN_CALIB_REG**,
  **CMD_ADMIN_BUPHABD** và lệnh khởi động lại hệ thống XL MH.
- Khác tab "Điều khiển" của panel 2, lệnh mức kỹ sư chỉ đi khi bấm nút
  **"Gửi lệnh"**; cạnh nút là serial của gói vừa gửi, góc phải thanh tab là
  serial của gói phản hồi.
- Ô **"Khóa điều khiển"**: đang khoá thì mọi ô nhập bám theo trạng thái phản hồi
  của hệ thống MH; mở khoá thì giữ giá trị kỹ sư đang đặt và đánh dấu **màu đỏ**
  chỗ lệch so với phản hồi. Quy ước này áp dụng cho cả tab "Điều khiển" panel 2.
- Bảng **"Kết quả hiệu chuẩn"** đổ dữ liệu gói **STATUS_CALIB**, bảng 100 tham số
  của tab "Params" đổ dữ liệu gói **STATUS_PARAMS**.
- Chọn chế độ hiệu chuẩn bên tab ADMIN tự nạp cặp tần số AD9361 tương ứng và gửi
  **CMD_ADMIN_AD** trước **CMD_ADMIN** 100 ms.
- Cửa sổ kỹ sư mở ra ở tab "ADMIN" với kích thước vừa khít nội dung tab đó.
- Biểu tượng phần mềm lấy từ `resources/RadarIcon.ico`.

**Giai đoạn 4** — vẽ cánh sóng:

- Nút **"ViewIQ - Vẽ cánh sóng"** trên tab ADMIN mở cửa sổ cùng tên (800x600,
  thu nhỏ được còn một nửa). Cửa sổ nổi nhưng không chặn: vẫn điều khiển được
  giao diện chính và cửa sổ kỹ sư trong lúc đang vẽ.
- Dữ liệu là gói **RAW_IQ** (4805 word, không có khung Dataframe) nhận qua dòng
  `Data-RAW` của `connect.json`, 250..400 gói/giây.
- **ViewIQ** (IQType 4..7): 600 điểm IQ1 / IQ2 từ StartWord.
  **Vẽ CS** (IQType 2, 3): trung bình cộng Sum / Sub trên MeanWords word, vẽ
  theo phương vị trên đồ thị cực và đồ thị 0..360°, đơn vị dB hoặc biên độ.
  Đang chạy thì DataType tự chuyển theo IQType của gói; giá trị nhiễu
  `0x5a5a` / `0xa5a5` bị loại khỏi phép tính.
- Bỏ chọn **Start/Stop** là đứng hình để soi số liệu: di chuột trên đồ thị hiện
  giá trị hai đường tại điểm đó.

**Giai đoạn 6** (đang làm) — luồng thông tin với máy "PC", P18M và VQ, quỹ đạo:

- `connect.json` ghi **định dạng gói** (`format`) và **thứ tự byte**
  (`big_endian`) cho từng dòng; tab "Connect" hiện hai cột này nhưng không cho
  sửa. File của bản cũ được tự chuyển sang dạng mới và bổ sung các dòng còn thiếu.
- **X18-SCN** (TCP Server 10555): MX18 đóng vai thiết bị SCN cho máy "PC" — gửi
  khối Start, trả lời lệnh và keepalive, mỗi lúc một PC.
- **X18-SCN-R / X18-SCN-S** (UDP, gói nhị phân "Cf" little-endian): giải mã gói
  PC gửi đến; mỗi điểm dấu MH nhận được gửi sang PC dạng Cf loại 12 (Plot).
- Cửa sổ **"Trạng thái SCN"** (panel 3) tạm hiện trạng thái phiên làm việc với PC.
- **X18-VQ** (UDP, ASTERIX CAT034/048 từ P18M): giải mã nhiều khối / nhiều bản ghi
  mỗi datagram, có kiểm tra biên. Bản ghi CAT048 có Track Number đưa vào **danh
  sách quỹ đạo** (gói TRACK `0x2051`): phương vị - cự ly, lat/lng tính từ tâm đài,
  vết suốt đời quỹ đạo; xoá khi P18M gửi bản tin cuối (I048/170 TRE = 1) hoặc khi
  quá `track_drop_sec` giây (mặc định 40) không có cập nhật.
- **SCH-VQ** (UDP, ASTERIX gửi VQ): quỹ đạo mỗi lần cập nhật (bản tin cuối TRE = 1
  khi xoá), điểm dấu MH (gói PLOT `0x2031` trên `Data-Status`), North marker và
  Sector crossing theo góc anten của VIDEO_R hoặc VIDEO_I. Time of Day đúng chuẩn
  (giây từ nửa đêm UTC × 128).
- Gói **ALARM_HEAD** (`0x7720`, hướng báo động) đã có định nghĩa trường.

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
├── resources/      FlashScreen.jpg, logo.png, RadarIcon.ico
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
| `connect.json` | Bảng cổng gửi/nhận cho từng loại dữ liệu; mỗi dòng có `format` (`dataframe`, `raw_iq`, `scn_text`, `scn_cf`, `asterix`) và `big_endian` — hai khoá này chỉ sửa trong file |
| `setupadmin.json` | Thiết lập mức kỹ sư: mật khẩu (mặc định `X18`; ô "Khóa điều khiển" không lưu — mỗi lần mở cửa sổ đều khoá sẵn). Các khoá chưa có giao diện, sửa trong file: luồng SCH-VQ `vq_range_change` (2.0 = LSB chuẩn ASTERIX), `vq_output_p18m` (false = SP kiểu ELM-2288), `vq_sac`/`vq_sic` (148/101), `vq_sector_source` (`VIDEO_R`/`VIDEO_I`), `vq_send_tre`, `vq_site_height_m`; bộ bám quỹ đạo từ điểm dấu MH `mh_*` (số vòng khởi tạo, vận tốc giới hạn, số vòng ngoại suy, chu kỳ quét mặc định, cửa sổ dự đoán, số hiệu bắt đầu) |
| `statuserror.json` | Ngưỡng báo lỗi của cửa sổ "Trạng thái MH": `Min50V`, `Max50V`, `Min5V`, `Max5V`, `MinCs`, `MaxT`, `MaxH` |
| `params.json` | Tham số đài (để dành cho giai đoạn sau; bảng tham số của tab "Params" lấy trực tiếp từ gói `STATUS_PARAMS`) |

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
  mặc định **big-endian**, đổi được bằng khoá `big_endian` của từng dòng trong
  `connect.json`. Các luồng X18-* không theo khung này: ASTERIX luôn big-endian,
  gói "Cf" của SCN luôn little-endian.
  `checksum` hiện gán 0 và chưa kiểm tra.
- **Video**: đường quét MH đến khoảng 400 gói/giây. Mỗi tia được vẽ bằng một
  phép biến đổi quay + giãn của một ảnh 600×1 điểm — rẻ hơn nhiều so với 600 đoạn
  thẳng cho mỗi tia — vào một lớp ARGB riêng; lớp này mờ dần bằng phép **trừ**
  alpha (không phải nhân, vì phép nhân số nguyên đứng lại ở mức alpha thấp và để
  lại vệt xanh không bao giờ tắt) và được ghép lên nền bản đồ ở nhịp 25 hình/giây.
- **RAW_IQ**: mỗi gói 19 KB, đến 400 gói/giây. Gói được tính ngay trên luồng
  nhận của cổng `Data-RAW` rồi chỉ giữ phần cần vẽ (ViewIQ: gói mới nhất; Vẽ CS:
  một cặp Sum/Sub cho mỗi phương vị encoder); cửa sổ lấy bản chụp ở nhịp 25
  hình/giây. Giao diện vẽ không kịp thì gói bị gói sau đè lên chứ bộ nhớ không
  phình ra. Thứ tự byte theo khoá `big_endian` của dòng `Data-RAW`.
