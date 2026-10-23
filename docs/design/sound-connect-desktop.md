# Sound Connect Desktop — Build 23

Bản phát hành chính thức của giao diện đã chốt ngày 23/10/2026, dùng lõi C++/IOBluetooth của SonyHeadphonesClient.

## Thiết kế

Giao diện tiếng Việt, chữ nội dung mặc định 14, màu chữ có độ tương phản rõ, tiêu đề ứng dụng lớn cùng biểu tượng riêng. Thanh tiêu đề macOS đồng màu với nền. Thanh bên dùng biểu tượng vector, nút bo tròn, điểm đánh dấu mục đang chọn và chuyển màu theo thời gian khung hình. Cửa sổ nhỏ dùng tab. Các thẻ có khoảng đệm dưới; trình phát dùng nền tối và nút tròn.

Các nút chống ồn/xuyên âm/tắt được vẽ trực tiếp, không dùng ảnh có nền vuông. Vị trí hiển thị thanh trượt được giới hạn trong khoảng hợp lệ; giá trị 0 của chế độ chống ồn vẫn được giữ nguyên trong giao thức. EQ dùng 5 dải tần cùng Clear Bass và thao tác Áp dụng.

## Tham khảo

Sony Sound Connect 13.3.0 (`com.sony.songpal.mdr`), từ XAPK do người dùng cung cấp. Đã giải mã 3.980 tài nguyên ảnh/XML, tham khảo `nc_asm_shortcut_view.xml`, `shortcut_nc_asm_component_layout.xml`, `equalizer_custom_layout.xml`. Các ảnh tham khảo và công cụ giải mã tạm đã được loại khỏi mã nguồn phát hành vì giao diện sử dụng hình vẽ bằng mã.

## Biên dịch và kiểm tra

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 6
ctest --test-dir build --output-on-failure
open build/SonyHeadphonesClient.app
```

Bản phát hành đặt tại `output/SonyHeadphonesClient.app`, phiên bản 1.4.4, CFBundleVersion 23. Có thể dùng `SONY_UI_PREVIEW=1` và `SONY_UI_PREVIEW_PAGE=0..3` để kiểm tra giao diện mà không kết nối Bluetooth. Ứng dụng không ghi `imgui.ini`; tùy chọn người dùng vẫn lưu trong thư mục Preferences.

Khả năng điều khiển thực tế tùy model và firmware, theo backend của mã nguồn cũ. APK chỉ là tham khảo giao diện. Kiểm thử EQ/pin và kiểm tra mở ứng dụng không thay thế kiểm tra Bluetooth trên tai nghe thật.
