# Kiểm tra giao thức Sony — 24/10/2026

## Kết luận và phạm vi

Đã kiểm tra tĩnh mã nguồn tại commit nền `b6aec09` của
`BuHuDu99/SonyHeadphonesClient` và IPA người dùng cung cấp; bổ sung các biện pháp
kiểm tra dữ liệu và kiểm thử kết nối giả lập. **Chưa đủ bằng chứng để chứng nhận
mọi thông số đúng với tai nghe thực tế hoặc bảo đảm không gây hư hỏng.** Người dùng
đã xác nhận model **WH-1000XM5**, firmware **2.5.1**. Đây là thông tin người dùng
cung cấp, chưa được đọc lại từ thiết bị; chưa có bản ghi Bluetooth đối chiếu hoặc
thử nghiệm phần cứng.
Không gửi lệnh thử đến tai nghe trong lần kiểm tra này.

Mã nguồn và tài liệu kiểm tra được lưu tại
[BuHuDu99/SonyHeadphonesClient](https://github.com/BuHuDu99/SonyHeadphonesClient),
tag `v1.4.4-build25`. IPA của Sony không được phân phối cùng mã nguồn hoặc release.

## Bằng chứng từ IPA

- Tệp: `jp.co.sony.songpal.mdr_13.2.0_und3fined.ipa`.
- SHA-256: `163ec86d677bc1194c6028e016968be9415302b06da06284c6037f3d415f3297`.
- Bundle ID: `jp.co.sony.songpal.mdr`; version `13.2.0`; build `510.9`.
- Mach-O `MDRPlugin`: `LC_ENCRYPTION_INFO_64.cryptid = 0`.
- Khai báo External Accessory có `jp.co.sony.songpal.mdr.link` và `.link2`;
  đây không phải bằng chứng UUID RFCOMM trên macOS giống iOS.
- Có các tên lớp `THMSGV1T1SetNcAsmParam`, `THMSGV1T1RetNcAsmCapability`,
  `THMSGV1T1SetEqEbbParam`, `THMSGV1T1RetEqEbbCapability_EqCapability`,
  `THMSGV2T1EqEbbRetCapabilityEq`, `THMSGV2T1EqEbbSetParamEq`,
  `HPCEqBandStepsStandard`, `HPCEqBandSteps10band`.
- Metadata Objective-C của `THMSGV1T1RetEqEbbCapability_EqCapability` có các
  trường `mBand_`, `mStep_`, `mPresetList_`: hỗ trợ kết luận rằng ứng dụng Sony
  có cấu trúc mô tả khả năng EQ, thay vì một cấu hình cố định duy nhất.

Tên lớp/trường chỉ xác nhận cấu trúc; **không chứng minh byte lệnh, dải giá trị,
hoặc hỗ trợ của một model cụ thể**. Chưa giải mã đầy đủ thuật toán serializer Sony.
Không sao chép mã nguồn hay phân phối binary/tài nguyên Sony vào repo.

Tái lập kiểm tra metadata và tên lớp (cần tự cung cấp IPA có cùng SHA-256; tệp không nằm trong repository):

```sh
python3 tools/inspect_sony_ipa.py jp.co.sony.songpal.mdr_13.2.0_und3fined.ipa
```

## Lỗi phần mềm đã sửa

| Điểm kiểm tra | Trước đây | Sau thay đổi |
| --- | --- | --- |
| Khung Bluetooth | Dùng độ dài do thiết bị cung cấp để truy cập checksum, chưa kiểm tra kích thước thực; constructor chỉ assert | Kiểm tra marker, sequence 0/1, độ dài thực, checksum và ACK rỗng trong mọi cấu hình build |
| Nhận RFCOMM | Đọc 4 byte header bằng một lần recv; header có thể chứa byte escape; buffer tăng không giới hạn | Thu khung có giới hạn 2048 byte, giải escape trước khi đọc độ dài; xử lý dữ liệu bị chia nhỏ |
| Gửi RFCOMM | Có thể gửi thiếu khung khi send trả ít byte | Gửi hết phần còn lại; lỗi khi send trả 0/số âm |
| Nội dung phản hồi | Nhiều truy cập vượt độ dài; chuỗi metadata/danh sách thiết bị thiếu giới hạn | Kiểm tra độ dài tối thiểu và từng chuỗi trước khi cập nhật trạng thái |
| Giá trị trước serialize | Ép int sang char/uint8_t có thể cuộn 256 thành 0 | Nhận int và kiểm tra phạm vi trước khi đóng gói |
| EQ | Biến thể lạ có thể được dùng như EQ 5 dải; mặc định payload 0 | Chỉ nhận subtype 0/1 và layout 0 hoặc 6 giá trị, mỗi giá trị 0–20; payload ban đầu chưa biết |
| Trạng thái chưa nhận | Giá trị UI mặc định có thể bị ghi xuống thiết bị chưa báo thuộc tính | Từ chối thay đổi khi chưa có phản hồi hợp lệ cho thuộc tính; kiểm tra cả nhóm thuộc tính dùng chung lệnh |
| Multipoint | Lệnh phụ `98 00 07 01` giống nhau cho cả bật và tắt | Bỏ lệnh phụ chưa xác minh; giữ lệnh chính `d8 d2 00 !enabled` |
| Voice guidance âm | Byte 0xfe có thể hiển thị như 254 | Đọc về -2 và giữ nguyên byte wire 0xfe khi ghi |

## Phạm vi giá trị được kiểm soát, chưa chứng nhận từ IPA

Các giới hạn sau giữ theo hợp đồng sẵn có của desktop client, tránh cuộn số/truyền
ngoài phạm vi. Chúng chưa được đối chiếu từng byte với Sony hoặc từng model:

| Thuộc tính | Hợp đồng desktop hiện tại |
| --- | --- |
| Âm lượng phát | 0–30 |
| Voice guidance | -2 đến +2 |
| Xuyên âm | 1–20; UI dùng 0 để chọn chế độ chống ồn |
| EQ | Clear Bass + 5 dải, -10 đến +10, wire offset +10; clamp ngoài phạm vi |
| EQ manual | subtype 0 dùng 0xa0; subtype 1 dùng 0xff |
| Speak-to-chat | sensitivity 0–2, timeout 0–3 |
| Touch sensor | Chỉ các enum 0x20, 0x35, 0xff đã có trong client |

EQ 10 dải và subtype khác không được chuyển đổi suy đoán sang EQ 5 dải.
Nhận phản hồi trạng thái chỉ là một điều kiện tối thiểu để ghi, không thay thế
truy vấn capability đầy đủ (band/step/preset/range) như cấu trúc Sony thể hiện.

## Kiểm chứng

- Build toàn bộ ứng dụng macOS thành công với AppleClang 21.
- Bundle kiểm tra ký ad-hoc; `codesign --verify --deep --strict` thành công.
- CTest Debug và Release: 6/6 (EQ serializer, battery, protocol safety, Bluetooth transactions, menubar/config, EQ preview/recovery).
- Kiểm thử protocol safety, Bluetooth transactions và EQ preview/recovery chạy thêm với AddressSanitizer và UndefinedBehaviorSanitizer.
- Kiểm thử bao gồm frame chứa byte escape, độ dài 60/61/62/255/256/512, short
  read/write, checksum/độ dài không hợp lệ, từng tiền tố bị cắt ngắn, gói quá lớn,
  giá trị cuộn số, EQ lạ, và 10.240 payload ngắn/ngẫu nhiên với seed cố định.
- Không kiểm thử Bluetooth thật, không chạy IPA, không thử cập nhật firmware,
  factory reset, hiệu chuẩn, hoặc thay đổi thông số tai nghe.

## Giới hạn còn lại trước khi coi là tương thích phần cứng

1. Đã xác định mục tiêu WH-1000XM5 / 2.5.1; cần đọc capability rồi xác nhận band/step/preset,
   dải xuyên âm/volume và data type cho từng lệnh.
2. So sánh bản ghi thao tác tương ứng trên Sony Sound Connect với desktop client.
   Không suy ra tính an toàn chỉ từ tên lớp trong IPA hoặc từ việc test phần mềm qua.
3. Đã sửa ACK đúng sequence và thêm readback cho các thay đổi cài đặt; vẫn cần
   đối chiếu hardware cho từng layout phản hồi. ACK chỉ xác nhận vận chuyển.
4. Đã chuyển việc chuẩn bị lệnh sang luồng UI và truyền snapshot bất biến cho
   luồng gửi; luồng gửi không còn fulfill thuộc tính. Chưa kiểm định toàn bộ ứng
   dụng với ThreadSanitizer hoặc chứng nhận loại bỏ mọi data race.
5. Ứng dụng chính là `/Applications/SonyHeadphonesClient.app`, 1.4.4 Build 25.
   Bản cài và source archive được lưu trong release `v1.4.4-build25`; các bản build,
   bộ cài và tệp phân tích tạm không được giữ trong cây mã nguồn.

## Đối chiếu riêng WH-1000XM5 / firmware 2.5.1

Thông tin model/firmware do người dùng xác nhận ngày 24/10/2026; không suy luận
thành profile tương thích chỉ từ tên thiết bị Bluetooth.

- [Sony Việt Nam: firmware 2.5.1](https://www.sony.com.vn/electronics/support/software/00278245)
  xác nhận đây là firmware dành cho WH-1000XM5. Tài liệu cập nhật không công bố
  hợp đồng byte giao thức Bluetooth để chứng nhận serializer của client.
- [Help Guide WH-1000XM5](https://helpguide.sony.net/mdr/wh1000xm5/v1/en/contents/TP1000534533.html)
  xác nhận EQ tùy chỉnh, NC/Ambient, Speak-to-Chat, âm lượng, multipoint và điều
  khiển phát nhạc là tính năng của model này. Có tính năng chính thức không đồng
  nghĩa lệnh trong desktop đã đúng trên firmware 2.5.1.
- Cùng tài liệu mô tả touch panel theo thao tác bật/tắt. Lệnh gán chức năng riêng
  trái/phải của client không được tài liệu này chứng nhận cho WH-1000XM5.
- [Báo cáo trực tiếp trong upstream issue #4](https://github.com/mos9527/SonyHeadphonesClient/issues/4)
  ghi nhận EQ, NC/Ambient và âm lượng hoạt động trên WH-1000XM5 với client 1.4.1
  trên Fedora (16/07/2025), nhưng không nêu firmware 2.5.1. Báo cáo cũng ghi nhận
  lỗi touch sensor, power off và thiếu kiểm tra nhiều chức năng. Bảng cũ là bằng
  chứng lịch sử, không phải kiểm chứng bản macOS hiện tại.
- Phản hồi Speak-to-Chat trong issue ngày 19/08/2025 ghi nhận toggle hoạt động
  nhưng trạng thái ban đầu sai; vì vậy cần đọc lại trạng thái, không xem ACK là
  chứng minh cài đặt đã được áp dụng.
- Không tìm thấy tài nguyên tên WH-1000XM5/wh1000xm5 trong đường dẫn ZIP của IPA.
  Điều này không loại trừ dữ liệu model được mã hóa/đặt tên bằng ID; chưa thu được
  bảng range/preset dành riêng cho WH-1000XM5 / 2.5.1 từ IPA.

**Kết luận hiện tại:** model/firmware phù hợp với tài liệu Sony, và chức năng EQ,
NC/Ambient, âm lượng có bằng chứng tương thích upstream ở phiên bản cũ. Chưa có
bằng chứng kiểm chứng từng byte trên firmware 2.5.1. Không thay opcode, gán profile
10 dải hoặc sửa firmware dựa trên suy đoán; giữ các kiểm tra chặn dữ liệu đã thêm.

## Thay đổi Build 24: xác nhận giao dịch và menubar

- ACK chỉ khớp khi có một lệnh đang chờ và sequence bằng `1 - sequence gửi`;
  dữ liệu thông báo từ tai nghe không thay đổi sequence gửi. ACK cũ không có lệnh
  đang chờ bị bỏ qua. Đối chiếu quy tắc ACK với
  [mã SonyHeadphonesProtocol của Gadgetbridge](https://github.com/Freeyourgadget/Gadgetbridge/blob/master/app/src/main/java/nodomain/freeyourgadget/gadgetbridge/service/devices/sony/headphones/SonyHeadphonesProtocol.java).
- Mỗi thay đổi cài đặt gửi SET → chờ ACK đúng sequence → gửi GET → chờ ACK và
  RET đúng data type, opcode, subtype → so sánh giá trị. Mất ACK, thiếu RET hoặc
  giá trị không khớp gây lỗi và dừng các thay đổi tiếp theo; không tự gửi lại SET.
- Thuộc tính chỉ được cập nhật từ phản hồi của tai nghe. Luồng gửi không gọi
  fulfill; UI chuẩn bị snapshot trước khi gửi. Giá trị hiển thị trên menubar lấy
  từ trạng thái đã nhận, không lấy trực tiếp từ giá trị người dùng vừa yêu cầu.
- Khi lỗi xác nhận, ngắt kết nối và tạm dừng tự kết nối lại, giữ thông báo lỗi.
- Điều khiển phát nhạc truy vấn lại trạng thái, bỏ cập nhật play/pause lạc quan.
  Lệnh tắt nguồn không có xác nhận readback vì thiết bị được yêu cầu ngắt kết nối.
- Menu macOS và Cài đặt có chung bốn tùy chọn: biểu tượng, phần trăm pin, tên
  tai nghe, chế độ NC/Ambient. Có hiệu lực ngay, lưu trong bảng `[menubar]` của
  TOML khi thoát app; config cũ mặc định giữ biểu tượng + pin. Luôn giữ ít nhất
  một mục để menu còn truy cập được. Không có thay đổi cài đặt tai nghe khi đổi
  cách hiển thị menubar.
- Đã kiểm tra giao diện xem thử và bật tên/chế độ trong màn hình Cài đặt. Đây
  là kiểm thử UI, không phải kiểm thử tương thích trên tai nghe thực.

Do protocol chỉ có sequence 1 bit, không thể phân biệt tuyệt đối một ACK trễ
qua nhiều vòng sequence. Chính sách không tự thử lại lệnh ghi và bắt buộc readback
là các lớp kiểm tra bổ sung; chúng không chứng nhận thông số hardware từ IPA.

## Build 25: EQ Live preview và khôi phục

- Chỉ bắt đầu nghe thử khi preset gốc có thể khôi phục; chặn preset chưa biết hoặc manual ID không khớp biến thể.
- Chọn preset gửi lệnh chọn preset rồi GET lại hệ số, dùng phản hồi thực để cập
  nhật đồ thị, cả 5 dải và Clear Bass. Không dùng bảng hệ số suy đoán của model.
- Chỉnh một dải/Clear Bass hoặc Flat tạo manual curve và nghe thử. Debounce 150 ms
  gộp thao tác kéo liên tiếp; chỉ một giao dịch được gửi tại một thời điểm. Phản hồi
  của lần chỉnh cũ không ghi đè bản chỉnh mới hơn đang chờ.
- Apply chỉ bật khi lần chỉnh mới nhất đã được xác nhận; xóa bản ghi khôi phục và
  lấy cấu hình đang nghe làm mốc mới. Apply không gửi thêm một lệnh SET trùng lặp.
- Revert/rời trang/mở Cài đặt/ẩn hoặc minimize cửa sổ/thoát app/ngắt kết nối/tắt
  tai nghe chủ động đều yêu cầu khôi phục mốc trước đó trước khi tiếp tục.
- Trước SET nghe thử đầu tiên, lưu bản ghi khôi phục cục bộ gồm địa chỉ thiết bị,
  preset, payload type, Clear Bass và hệ số. Ghi config qua tệp tạm rồi rename;
  nếu ghi không thành công, không gửi lệnh nghe thử.
- Mất kết nối hoặc crash có thể khiến EQ nghe thử còn trên tai nghe cho đến khi
  kết nối lại. App chỉ khôi phục trên đúng địa chỉ thiết bị đã ghi, sau khi nhận
  trạng thái EQ. Không tuyên bố đã khôi phục khi không có kết nối/xác nhận.
- Manual baseline khôi phục cả hệ số; preset baseline khôi phục lựa chọn preset.
  Không ghi Custom 1/2. Off có thể không báo hệ số: hiển thị đường không hiệu chỉnh;
  những preset khác phải trả đủ 6 giá trị để xác nhận nghe thử.
- Thử giao diện ở chế độ demo: chọn Bright cập nhật graph/sliders, rời trang chưa
  Apply trở về Off, Apply rồi rời/quay lại vẫn giữ Bright. Đây không phải thử âm thanh
  trên phần cứng. Đường cong demo bị giới hạn trong `SONY_UI_PREVIEW`, không gửi Bluetooth.
- Giữ thuật ngữ gốc trong EQ; Flat là các giá trị bằng 0. Bỏ các câu trang trí và
  chú thích lặp khỏi giao diện, giữ hướng dẫn cần thiết về Apply/khôi phục.


## Sửa hiển thị pin — Build 26 (được hiệu chỉnh tiếp ở Build 27)

- Bỏ phép nhân 10 cho byte pin 0–10: giao thức MDR trả phần trăm trực tiếp.
  Đối chiếu [Gadgetbridge SonyProtocolImplV1.handleBattery](https://github.com/Freeyourgadget/Gadgetbridge/blob/master/app/src/main/java/nodomain/freeyourgadget/gadgetbridge/service/devices/sony/headphones/protocol/impl/v1/SonyProtocolImplV1.java).
- Xử lý cả response `0x23` và notification `0x25` của giao thức V2 hiện dùng;
  kiểm tra đủ 4 byte cho single/case và 6 byte cho dual, gồm cờ sạc.
- Single chỉ hiện phần trăm, không có L/R. Dual giữ hai giá trị độc lập L/R;
  case hiện trong menu/tooltip khi được báo. Không lấy trung bình hai bên.
- Byte 0 của từng earbud trong dual là absent/unreported: hiển thị `--` và xóa
  giá trị cũ; single/case vẫn nhận 0%. Byte >100 hiển thị `--`, không ép thành 100%.
- Cố định topology và biến thể sau phản hồi hợp lệ trong mỗi kết nối. Không để
  phản hồi của các probe không hỗ trợ đổi single thành dual hoặc ngược lại.
- Kiểm thử đi qua transport frame và `Headphones::poll`, gồm giá trị 1–10%,
  cập nhật notification, hai tai lệch pin, mất một tai, case trước dual,
  sentinel, phản hồi khác biến thể và payload thiếu/cờ sạc không hợp lệ.
- Chưa có số đo pin đối chiếu trên phần cứng thật; kết quả phần mềm không xác nhận
  độ chính xác cảm biến pin/firmware của mọi model Sony.
- Kiểm chứng Build 26: CTest Debug và Release đều đạt 6/6; riêng battery parser/formatter
  chạy qua ASan/UBSan không báo lỗi.


## Đối chiếu trực tiếp pin từ IPA — Build 27

Xem [báo cáo pin IPA](sony-battery-ipa-audit.md) và
[bằng chứng tái lập](sony-battery-ipa-evidence.json). Đã đọc thân hàm, chạy các
leaf helper ARM64 bằng Unicorn và so sánh toàn bộ 256 giá trị với decoder desktop.
Kết quả xác nhận thang 0–100; sửa Build 26 để giữ 0% cho từng tai, chấp nhận
charging UNKNOWN/CHARGED, sửa độ dài payload threshold và thêm type single 0x08.
Các phát biểu Build 26 về 0 của earbud là absent và chiều dài 6/4 cho mọi biến thể
đã được thay thế bởi bằng chứng Sony này. Chưa xác nhận trên tai nghe thật.

Bổ sung kiểm tra trực tiếp trên WH-1000XM5: xác định capability pin single (`0x20`),
loại bỏ phản hồi giả từ query dual không hỗ trợ; nhận bản tin single 95%. Chỉ query
loại pin được capability công bố. Chưa đối chiếu màn hình app Sony hoặc dung lượng pin.
