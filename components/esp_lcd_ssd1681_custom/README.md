# ESP LCD SSD1681 驱动 (自定义版本)

本组件是基于 Espressif 官方的 `esp_lcd_ssd1681` 组件修改而来，添加了对内置 LUT 局部刷新的支持。

## 原始来源

- 原始仓库: https://github.com/espressif/esp-bsp
- 组件路径: components/lcd/esp_lcd_ssd1681
- 版本: 0.1.0~1
- Commit: 30d2504a679c1cb0d10833f772d97453098b4c08
- 许可证: Apache License 2.0

## 自定义修改

### 1. 支持内置 LUT 局部刷新模式

添加了 `epaper_panel_set_refresh_mode()` 函数，允许在内置 LUT 的局刷和全刷模式之间切换：

- **局刷模式** (`partial_refresh = true`):
  - 使用 `0x22=0xFF` (显示更新控制)
  - 使用 `0x3C=0x80` (边框波形控制)
  - 刷新速度快，但需要定期全刷来重置屏幕

- **全刷模式** (`partial_refresh = false`):
  - 使用 `0x22=0xCF` (显示更新控制)
  - 使用 `0x3C=0x01` (边框波形控制)
  - 刷新完整，清除残影

### 2. 新增命令参数定义

在 `esp_lcd_ssd1681_commands.h` 中添加：
- `SSD1681_PARAM_DISP_PARTIAL_REFRESH` (0xFF)
- `SSD1681_PARAM_BORDER_WAVEFORM_PARTIAL` (0x80)

### 3. 修改的文件

- `esp_lcd_panel_ssd1681.c` - 添加局刷模式支持
- `esp_lcd_ssd1681_commands.h` - 添加新的命令参数
- `include/esp_lcd_panel_ssd1681.h` - 添加新的 API 函数声明

## 使用方法

### 局刷模式示例

```c
// 设置局刷模式
epaper_panel_set_refresh_mode(panel_handle, true);

// 写入黑色 VRAM
epaper_panel_set_bitmap_color(panel_handle, SSD1681_EPAPER_BITMAP_BLACK);
esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 200, 200, black_bitmap);

// 写入红色 VRAM（与黑色相同）
epaper_panel_set_bitmap_color(panel_handle, SSD1681_EPAPER_BITMAP_RED);
esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 200, 200, black_bitmap);

// 触发刷新
epaper_panel_refresh_screen(panel_handle);
```

### 全刷模式示例

```c
// 设置全刷模式
epaper_panel_set_refresh_mode(panel_handle, false);

// 写入并刷新
epaper_panel_set_bitmap_color(panel_handle, SSD1681_EPAPER_BITMAP_BLACK);
esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 200, 200, black_bitmap);
epaper_panel_refresh_screen(panel_handle);
```

## 注意事项

1. **定期全刷**: 建议每 10-20 次局刷后进行一次全刷，以清除残影和重置屏幕状态
2. **双 VRAM 写入**: 使用内置 LUT 局刷时，需要同时写入黑色和红色 VRAM，通常写入相同数据
3. **温度补偿**: 内置 LUT 会使用芯片的温度传感器进行温度补偿，比自定义 LUT 更稳定

## 许可证

本组件继承原始组件的 Apache License 2.0 许可证。详见 [license.txt](license.txt) 文件。

## 贡献者

- 原始代码: Espressif Systems
- 局刷修改: ESPaperPlay_RE 项目
