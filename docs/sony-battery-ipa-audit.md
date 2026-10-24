# Đối chiếu pin với IPA Sony 13.2.0 — Build 27

Nguồn là IPA do người dùng cung cấp: bundle `jp.co.sony.songpal.mdr`, version
13.2.0, build 510.9. SHA-256:
`163ec86d677bc1194c6028e016968be9415302b06da06284c6037f3d415f3297`.
Phân tích binary ARM64 `Payload/MDRPlugin.app/MDRPlugin`, có `cryptid = 0`.
Hash xác định đúng tệp đã kiểm tra, không phải chứng nhận chữ ký/phát hành Sony.

## Bằng chứng từ thân hàm

Địa chỉ dưới đây là virtual address trong Mach-O chưa cộng ASLR. Metadata
Objective-C nối tên lớp/selector với IMP; `llvm-objdump` dùng để đọc thân hàm.

| Thành phần | Địa chỉ | Kết quả |
|---|---|---|
| Helper đổi signed byte thành unsigned, được getter pin gọi | `0x1012ca55c` | `byte & 0xff`; không nhân, chia hoặc làm tròn |
| `HPCBatteryUtil.hasValidLevelWithInt:` | IMP `0x1018bd1dc`, leaf `0x1018bd208` | `0 <= level <= 100` |
| `HPCBatteryInformation.hasValidLevel` | `0x101c00dac` | Cùng giới hạn 0–100, bao gồm 0 |
| `THMSGV2T1PowerRetStatusBatteryBase.getBatteryLevel` | `0x101416ac8` | Đọc byte index 2 rồi gọi helper unsigned |
| Các getter index của single | `0x101416a38`, `0x101416a50` | Level index 2, charging index 3 |
| Các getter index của left/right | `0x101418224`…`0x10141826c` | Level L/R index 2/4, charging L/R index 3/5 |
| `HPCLRBatteryInformationHolderTableSet2.createInformation…` | IMP `0x101bfe9a8`, body `0x101bfe73c` | Chuyển hai level riêng vào `HPCBatteryInformation`; không đổi 0 thành absent |
| Constructor `HPCBatteryInformation` | body `0x101c00b9c` | Lưu nguyên integer level; `getCurrentLevel` trả lại trường này |
| `THMSGV2T1BatteryChargingStatus.initialize` | `0x10140e258` | `NOT_CHARGING=0`, `CHARGING=1`, `UNKNOWN=2`, `CHARGED=3`, `OUT_OF_RANGE=0xff` |
| `THMSGV2T1PowerInquiredType.initialize` | `0x10140d098` | Các type/threshold trong bảng dưới |

`THMSGV1T1BatteryParam.restoreFromPayloadWithByteArray:` (IMP `0x101329c10`)
đọc byte level tại index 0 của child payload và cũng gọi helper unsigned.
Việc này không bổ sung hỗ trợ toàn bộ giao thức V1 cho desktop.

## Cấu trúc được áp dụng cho parser desktop V2

Các index tính từ opcode là index 0. Threshold là trường riêng, **không phải**
mức pin thay thế. Getter Sony chứng minh các index tối thiểu cần đọc; desktop
kiểm tra đúng chiều dài cấu trúc đã biết trước khi đọc. Không tuyên bố Sony
cũng từ chối mọi byte bổ sung như chính sách kiểm tra của desktop.

| Type | Tên trong Sony | Chiều dài desktop nhận | Level | Threshold |
|---|---|---|---|---|
| `0x00` | BATTERY | 4 | index 2 | Không |
| `0x01` | LEFT_RIGHT_BATTERY | 6 | index 2, 4 | Không |
| `0x02` | CRADLE_BATTERY | 4 | index 2 | Không |
| `0x08` | BATTERY_WITH_THRESHOLD | 5 | index 2 | index 4 |
| `0x09` | LR_BATTERY_WITH_THRESHOLD | 8 | index 2, 4 | index 6, 7 |
| `0x0a` | CRADLE_BATTERY_WITH_THRESHOLD | 5 | index 2 | index 4 |

Getter threshold single/case là `THMSGV2T1PowerRetStatusBatteryThresholdBase`:
`THRESHOLD_INDEX` ở `0x10141f98c` trả 4. Getter threshold L/R ở
`0x10141acc8` và `0x10141ada0` đọc index 6 và 7.

## Sửa khác biệt của Build 26

- Giữ **0%** cho cả single, từng tai và case. Chỉ byte ngoài 0–100 mới hiện `--`.
  Điều này sửa giả định trước dựa trên Gadgetbridge rằng 0 của earbud là absent.
  Không suy ra trạng thái có/không kết nối của từng tai từ riêng mức pin.
- Không loại cả bản tin khi charging >1. Sony có các trạng thái UNKNOWN/CHARGED;
  giá trị level được đọc và kiểm tra độc lập với charging.
- Sửa type `0x09` thành payload 8 byte và `0x0a` thành 5 byte; thêm single `0x08`.
- Single chỉ hiện một số; dual hiện L/R riêng. Chọn layout từ inquired type của
  phản hồi đã được capability cho phép, không suy từ số phần trăm. Variant được giữ
  ổn định trong mỗi kết nối. Không thăm dò những layout chưa được thiết bị công bố.

## Tái lập bằng mã Sony được giả lập

Công cụ `tools/verify_sony_battery_ipa.py` yêu cầu đúng SHA-256 ở trên. Dùng Unicorn
chạy riêng các leaf helper ARM64 trong bộ nhớ cô lập, giới hạn số lệnh. Không mở
ứng dụng iOS, không có Bluetooth/network/API iOS trong môi trường giả lập.

```sh
python3 -m venv /tmp/sony-battery-audit-env
/tmp/sony-battery-audit-env/bin/pip install unicorn
/tmp/sony-battery-audit-env/bin/python tools/verify_sony_battery_ipa.py /path/to/app.ipa \
  --client-checker /path/to/build/SonyBatteryTests \
  --output /tmp/sony-battery-evidence.json
```

Kết quả lưu ở [sony-battery-ipa-evidence.json](sony-battery-ipa-evidence.json):
256/256 đầu vào byte của decoder desktop khớp helper unsigned + kiểm tra hợp lệ
của Sony. Các getter index cũng được chạy giả lập. Ánh xạ type/charging và index
threshold L/R được kiểm tra tĩnh tại địa chỉ cố định; không gọi toàn bộ parser
hay state machine Sony trong giả lập. Công cụ cũng so sánh bảng kích thước
payload desktop với các cấu trúc đã đối chiếu tĩnh.

IPA, binary trích xuất, bytecode và disassembly không được đưa lên repository
hoặc release. App desktop dùng triển khai C++ riêng; không nạp thư viện iOS Sony.

Kết quả này kiểm chứng các quy tắc giải mã đã nêu, không đo độ chính xác cảm biến
pin, không chạy app Sony trên thiết bị, không xác nhận toàn bộ capability hoặc
firmware WH-1000XM5 2.5.1 qua Bluetooth thật.

Kiểm thử Build 27: CTest Debug và Release đạt 6/6; SonyBatteryTests qua
ASan/UBSan không báo lỗi. Các kiểm tra này không cần kết nối tai nghe.


## Chọn loại pin theo capability và phản hồi thật

`THMSGV2T1ConnectRetSupportFunction.getSupportFunctions` (IMP `0x1016c7624`)
đọc count ở byte 2, sau đó các cặp (function code, version) từ byte 3.
`THMSGV2T1Command.initialize` (`0x101494314`) xác nhận GET `0x06`, RET `0x07`;
truy vấn table 1 là `[06, 00]`. Parser desktop yêu cầu đúng `3 + 2*count` byte,
header table 0, rồi chỉ cho phép các function pin sau:

| Function Sony | Code capability | Type bản tin pin |
|---|---|---|
| BATTERY_LEVEL_INDICATOR | `0x20` | `0x00` |
| LEFT_RIGHT_BATTERY_LEVEL_INDICATOR | `0x21` | `0x01` |
| CRADLE_BATTERY_LEVEL_INDICATOR | `0x22` | `0x02` |
| BATTERY_LEVEL_WITH_THRESHOLD | `0x28` | `0x08` |
| LR_BATTERY_LEVEL_WITH_THRESHOLD | `0x29` | `0x09` |
| CRADLE_BATTERY_LEVEL_WITH_THRESHOLD | `0x2a` | `0x0a` |

Các code được đối chiếu `ComSonySongpalTandemfamilyMessageMdrV2FunctionType.initialize`
(`0x1016d0ef4`), literal tên enum và lệnh gán byte code. Công cụ tái lập cũng kiểm tra
ánh xạ này. Không xem byte version `0xff` là loại pin hay phần trăm.

Nếu chưa nhận capability hợp lệ, app chờ và chỉ hỏi lại capability, không đoán loại
pin bằng cách thử các query. Nếu capability đồng thời khai báo single và dual,
app giữ pin chưa xác định. Nếu cùng một layout có cả bản thường và threshold,
app chọn bản threshold và không trộn giá trị giữa các variant. Case chỉ dùng với dual.

Khi kiểm tra chỉ đọc trên WH-1000XM5 / firmware 2.5.1 của người dùng, bảng capability
thực tế có `20 ff`, không công bố chức năng dual/case. Thăm dò cũ vẫn nhận được
`23 09 00 00 09 00 64 64`, gây số giả L 0% / R 9%; đây là lý do không thể tin một
phản hồi chỉ vì các byte nằm trong 0–100. Sau khi lọc capability, client chỉ hỏi
single và nhận `23 00 5f 00`: mức pin **95%**, một giá trị duy nhất.

Đây là xác nhận parsing/hiển thị từ phản hồi thật, chưa so sánh với app Sony chạy
trên iPhone hay đo dung lượng pin. Không gửi lệnh thay đổi cấu hình trong kiểm tra này.
