# Sony Headphones Client

## macOS 1.4.4 — Build 26

[Tải bộ cài và mã nguồn Build 26](https://github.com/BuHuDu99/SonyHeadphonesClient/releases/tag/v1.4.4-build26).
Bản app dành cho **Apple Silicon, macOS 27.0 trở lên**. Mở DMG, kéo app vào
Applications, tháo DMG rồi mở `/Applications/SonyHeadphonesClient.app`.

- **Pin:** thiết bị một pin hiển thị một giá trị; tai nghe hai bên hiển thị L/R riêng.
  Đọc đúng phần trăm, cập nhật qua thông báo Bluetooth; `--` khi chưa có giá trị.
- **EQ Live preview:** chọn preset hoặc chỉnh thanh EQ để nghe thử. App đọc lại
  đường cong từ tai nghe để đồng bộ 5 dải và Clear Bass. **Apply** giữ EQ đang nghe;
  **Revert**, rời trang EQ, ẩn/minimize cửa sổ hoặc thoát app sẽ khôi phục EQ trước đó.
- **Flat:** đặt cả 5 dải EQ và Clear Bass về 0. Flat cũng là nghe thử cho đến khi Apply.
- Nếu mất kết nối hoặc app dừng giữa chừng, cấu hình gốc được lưu cục bộ để khôi phục
  khi kết nối lại **đúng tai nghe**. Không thể khôi phục khi tai nghe đang mất kết nối.
- Tùy chỉnh biểu tượng, pin, tên tai nghe và chế độ âm thanh tại
  **Cài đặt → Nội dung trên menubar** hoặc menu của biểu tượng tai nghe.
- Kiểm tra ACK đúng sequence, giới hạn tham số và đọc lại giá trị sau lệnh ghi;
  không tự gửi lại lệnh ghi khi xác nhận thất bại.

Giao diện giữ các nhãn **Equalizer, Preset, Custom, Clear Bass, Flat, Apply, Revert**.
Các câu trang trí và hướng dẫn lặp đã được bỏ.

Đã kiểm thử phần mềm **6/6** (Debug và Release), kiểm tra giao diện bằng chế độ xem thử và chạy các
kiểm thử giao thức/giao dịch/EQ với ASan/UBSan. Chưa xác nhận trên phần cứng thật
WH-1000XM5 firmware 2.5.1; xem [báo cáo giao thức](docs/protocol-safety-audit.md).
App được ký ad-hoc, chưa notarize bằng Apple Developer.

Mã nguồn trên nhánh `main` và tag `v1.4.4-build26`. Release có bộ cài DMG,
source archive kèm toàn bộ dependency đang dùng, thông tin build và SHA-256.
Thư mục dự án giữ mã nguồn/Git; binary, bộ cài và tệp tạm không được commit.

Ứng dụng desktop điều khiển tai nghe Sony qua Bluetooth, với giao diện tiếng Việt. Phát triển từ [mos9527/SonyHeadphonesClient](https://github.com/mos9527/SonyHeadphonesClient), dựa trên [Plutoberth/SonyHeadphonesClient](https://github.com/Plutoberth/SonyHeadphonesClient). Giữ nguyên ghi nhận tác giả và giấy phép MIT.

Tải bản macOS Apple Silicon tại [Releases](https://github.com/BuHuDu99/SonyHeadphonesClient/releases). Bản phát hành được kiểm tra trên macOS; mã nguồn Windows/Linux được giữ lại nhưng chưa được xác nhận với giao diện này.

## Quyền riêng tư và chi phí

Ứng dụng không cần tài khoản hoặc khóa API. Điều khiển tai nghe dùng Bluetooth cục bộ và đọc thông tin đang phát trên máy. Repository không chứa cấu hình cá nhân, token GitHub, IPA/APK tham khảo hoặc tệp tạm. GitHub Actions được tắt ở repository; workflow trong mã nguồn chỉ hỗ trợ chạy thủ công. Không có workflow tự động triển khai hay gọi dịch vụ AI trả phí.

## DISCLAIMER

**THIS PROGRAM IS NOT AFFILIATED WITH SONY. YOU ARE RESPONSIBLE FOR ANY DAMAGE THAT MAY OCCUR WHILE USING THIS PROGRAM.**

## Kiểm tra giao thức

Xem [báo cáo đối chiếu IPA và kiểm tra giao thức](docs/protocol-safety-audit.md) cho thay đổi đang kiểm tra, bằng chứng và giới hạn. Kiểm thử phần mềm chưa chứng nhận an toàn phần cứng cho mọi model/firmware.

## Features

Only the device-specific functions are listed here. If unspecified, the functionalities would be generally available.

If the feature support status for your own device is missing/incorrect/untested here, feel free to submit an [Issue](https://github.com/mos9527/SonyHeadphonesClient/issues/new) or a Pull Request so this table can be kept up-to-date!

**ATTENTION:** Older devices (e.g. XM3s) **will not work** with this fork due to updated BT protocols and are therefore discarded. Use [Plutoberth's original SonyHeadphonesClient](https://github.com/Plutoberth/SonyHeadphonesClient) instead.

Check the [Device Support](docs/device-support/) folder for more details on confirmed per-device function availability.

## Downloads

Bản macOS Apple Silicon Build 26 có tại [Releases](https://github.com/BuHuDu99/SonyHeadphonesClient/releases/tag/v1.4.4-build26). Mã nguồn Windows/Linux được cung cấp để tự biên dịch; chưa có tệp phát hành đã kiểm tra cho hai nền tảng này.


## For Developers
### Cloning the repo
```git clone --recurse-submodules https://github.com/BuHuDu99/SonyHeadphonesClient.git```

### Building
- `cmake` is required for Windows, macOS and Linux builds. A C++20 compliant compiler is also required.
- `glfw` will always be built and statically linked with the application.
Please follow the guide in https://www.glfw.org/docs/3.3/compile.html#compile_deps to ensure its dependencies.
- On non-macOS platforms, OpenGL is used for GUI rendering. Make sure your system has the requisite GL libs intalled.

#### Windows
Install [cmake](https://cmake.org/download/) (3.29.3 works) and install [Visual Studio Community 2022](https://visualstudio.microsoft.com/vs/) the C++ components:
* MSVC - VS 2022 C++ x64/x86 build tools
* Windows SDK

You can build, and debug the app with [Visual Studio's CMake intergration](https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=msvc-170#ide-integration).

You can also do this from the command line. In Visual Studio Run `Tools --> Command Line --> Developer Command Prompt` ,and then:
```
cd SonyHeadphonesClient
mkdir build
cd build
cmake ..
cmake --build .
```

#### Linux
`DBus` and `libbluetooth`(`bluez`) are required for Bluetooth support.

- Debian / Ubuntu:

```bash
sudo apt install libbluetooth-dev libdbus-1-dev
# For GLFW
sudo apt-get install xorg-dev
```

- Fedora:

```bash
sudo dnf install bluez-libs-devel dbus-devel
```

- Arch Linux:

```bash
sudo pacman -S bluez dbus
```

You can then build the app with CMake, with a C++20 compliant compiler installed.
```
cd SonyHeadphonesClient
mkdir build
cd build
cmake ..
cmake --build .
```


#### macOS
You need Xcode Command Line Tools (or Xcode) and CMake to build the application.
For a local arm64 build and all tests:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
codesign --force --deep --sign - build/SonyHeadphonesClient.app
```

The source archive attached to Build 26 already includes the submodule sources;
when cloning Git, use `--recurse-submodules` as shown above.

Pair the headphones once in **System Settings → Bluetooth** before opening the client. The client can then open the Sony control channel itself, remember the last successful device, and reconnect when that paired device becomes available.

> This repository builds a macOS desktop app, not an iOS app. Its Sony control connection uses Classic Bluetooth RFCOMM through `IOBluetooth`, an API that Apple does not expose to third-party iOS apps.

To build a universal binary, you can use the following commands:
```bash
cd SonyHeadphonesClient
mkdir build
cd build
cmake -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" ..
cmake --build .
```

An app bundle will be created in the `build` directory named `SonyHeadphonesClient.app`.

## Contributing
### Capturing Bluetooth Packets
See [Packet Capture](docs/packet-capture.md) doc for more info.

## Contributors

* [Plutoberth](https://github.com/Plutoberth) - Initial Work and Windows Version
* [Mr-M33533K5](https://github.com/Mr-M33533K5) - BT Interface and Other Help
* [semvis123](https://github.com/semvis123) - macOS Version
* [jimzrt](https://github.com/jimzrt) - Linux Version
* [guilhermealbm](https://github.com/guilhermealbm) - Noise Cancelling Switch

## Third-party
* [GLFW](https://www.glfw.org/) - Window and Input
* [Dear ImGui](https://github.com/ocornut/imgui/) - GUI
* [tomlplusplus](https://github.com/marzer/tomlplusplus) - Config file parsing
* [cascadia-code](https://github.com/microsoft/cascadia-code) - Default font supplied with the app

## License

Distributed under the [MIT License](https://github.com/Plutoberth/SonyHeadphonesClient/blob/master/LICENSE). See LICENSE for more information.

## Đóng gói DMG

Sau khi biên dịch lại, chạy `bash tools/package_macos.sh build/SonyHeadphonesClient.app`. Bộ cài chứa app, lối tắt Applications và hướng dẫn; không cài dịch vụ nền hoặc cần quyền quản trị.
