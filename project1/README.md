# Project 1：Multi-layer BPV Generator

## 專案說明

此程式讀取簡化的 layout 文字檔，依 metal layer 分別辨識 pins、計算安全間距，並產生 blockage rectangles。

每一層都會獨立運算，不會因為不同 layer 在相同 XY 位置重疊，就互相扣除 blockage。完成後程式會輸出：

- 每個有效 layer 各一張 SVG，例如 `M1.svg`、`M2.svg`、`M3.svg`。
- 所有 layer pins 聯集後產生的總體結果 `BPV.svg`。

圖中不顯示座標或 pin 名稱。各 layer 的獨立 SVG 使用一致配色，其中 pins 統一使用 `BPV.svg` 中 M1 pins 的藍色；`BPV.svg` 的 blockage 使用灰色，pins 則依 metal layer 使用不同顏色。`BPV.svg` 會先合併所有 layers 的 pins 與 spacing keep-out，再從整體 bounding box 扣除，最後切成 rectangles。

## 專案檔案

```text
project1/
├── project1.cpp        # C++17 主程式
├── M1.svg              # M1 layer 結果
├── M2.svg              # M2 layer 結果
├── M3.svg              # M3 layer 結果
├── BPV.svg             # 所有 layer pins 聯集後的總體結果
└── README.md            # 本文件
```

SVG 會建立在執行程式時的目前工作目錄。

## 核心設定

目前所有 layer 共用以下安全間距：

```cpp
MIN_SPACING = 0.5
```

也就是每個 pin 的邊界向外保留 `0.5`，blockage 不會進入該區域。輸入格式目前沒有提供各 layer 個別 spacing rule，因此尚未支援不同層使用不同間距。

## 處理架構

### 1. 解析 layout

`parse_layout()` 讀取 layers、polygons、rectangles 與 text labels，並將幾何圖形轉成 Boost.Geometry polygons。

### 2. 辨識 pins

`identify_pins()` 會將 text label 與 shape 配對。必須同時符合：

1. text 與 shape 的 layer number 相同。
2. text 座標位於 shape 內部或邊界上。

符合條件的 shape 會被視為該 text 名稱的 pin。若 text 沒有對應到任何 shape，程式會輸出 warning。

### 3. 逐 layer 獨立運算

主程式依 `number_of_layers` 區段宣告的順序處理每個 layer：

1. 收集該 layer 的 polygons 與 rectangles。
2. 計算該 layer 自己的最小 bounding box。
3. 只保留該 layer 的 pins。
4. 將 pins 向外擴張 `MIN_SPACING`。
5. 從 layer bounding box 扣除擴張後的 pin 區域。
6. 將剩餘 blockage 分解成水平 rectangles。

沒有任何 polygon 或 rectangle 的 layer 會被略過，並在終端機顯示 warning。如果所有宣告的 layers 都沒有幾何資料，程式會以錯誤狀態結束。

### 4. Rectangle 分解

`decompose_to_rectangles()` 使用 layer bounding box 與擴張後 pin bounding boxes 的邊界建立切割線，再將 blockage 分割成 grid cells。

只有完全位於 blockage 區域內的 cells 會被保留。相鄰 cells 只在同一個水平 y-band 內合併，因此輸出結果以水平 rectangles 為主。

### 5. SVG 輸出

`write_layer_svg()` 產生各 layer 的獨立結果圖，檔名取自輸入中的 layer name：

```text
M1 -> M1.svg
M2 -> M2.svg
M3 -> M3.svg
```

檔名中的非英數字元會轉換成 `_`，但 `-` 與 `_` 會保留。

各 layer SVG 直接繪製完整的 blockage geometry，不顯示 rectangle 切割線，並在左上角標示 metal layer 名稱；只有總體 `BPV.svg` 會顯示切割後的 rectangles。

`write_bpv_svg()` 會合併所有 layer 的 pins，再輸出 `BPV.svg`：

- 合併各 layer 的 bounding boxes，建立總體 bounding box。
- 收集所有 layers 的 pins，並將每個 pin 向外擴張 `MIN_SPACING`。
- 對所有擴張後的 pins 執行幾何聯集，重疊部分會合併成真正的 keep-out union shape，內部邊界會消失。
- 從總體 bounding box 扣除 pin keep-out 聯集，得到總體 blockage。
- 只使用 keep-out union shape 的外輪廓與洞口輪廓座標作為切割線，不保留各 pin 重疊區域中的原始邊界。
- 忽略外擴輪廓上的共線中間點，避免原始 pin 頂點在外擴邊上形成多餘切線與細長 rectangles。
- 根據這些聯集後的輪廓，將總體 blockage 分解成互不重疊的水平 rectangles。
- 每個 rectangle 都保留邊框，因此可以看出總體結果的切割方式。
- 所有 blockage 使用相同的灰色半透明填色。
- Pins 依所屬 metal layer 使用不同顏色，並畫在 blockage 上方。
- 各 layer 的獨立 SVG 使用一致的灰階配色，不依 layer 分色。
- 左上角圖例會列出 `Overall blockage` 及各 metal layer 的 pin 顏色。
- Pin 名稱與座標都不會顯示在 SVG 上。

因此，只要任一 layer 在某處存在 pin keep-out，該位置就會從 `BPV.svg` 的總體 blockage 中扣除。

## 輸入格式

```text
number_of_layers <數量>
<layer_name> <layer_number>
...
end_of_layer

number_of_polygons <數量>
<layer_number> <x1> <y1>, <x2> <y2>, ...
...
end_of_polygon

number_of_rectangles <數量>
<layer_number> <x1> <y1>, <x2> <y2>
...
end_of_rectangle

number_of_texts <數量>
<layer_number> <pin_name> <x> <y>
...
end_of_text
```

空白行及以 `#` 開頭的註解行會被忽略。

### 多層範例

`multi_layer.txt` 定義 M1、M2、M3。所有 pins 都接觸各 layer bounding box 的左側或右側邊界，部分 pins 也會在不同 layer 的 XY 投影上重疊，用來確認：

- 每層 blockage 是否獨立產生。
- 邊界 pin 的 spacing 是否會正確被 layer bounding box 截斷。
- 同一 XY 位置上的不同 layer 是否互不干擾。
- `BPV.svg` 是否正確合併三層 pins，並在聯集後產生總體 blockage rectangles。

執行後會產生：

```text
M1.svg
M2.svg
M3.svg
BPV.svg
```

## 編譯與執行

在 `project1` 目錄中編譯 C++ 程式：

```bash
g++ project1.cpp -std=c++17 -O2 -I/opt/homebrew/include -o project1
```

用法：

```bash
./project1 <input.txt>
```

例如：

```bash
./project1 multi_layer.txt
```

## 終端機輸出

每個 layer 都會分別列出：

- Layer name 與 number。
- 該 layer 的 bounding box。
- Pins 及其 bounding boxes。
- Blockage rectangles。

Rectangle 格式如下：

```text
R<編號>: <xl> <yl>, <xh> <yh>
```

例如：

```text
R1: 0.000 0.000, 60.000 7.500
```

最後會列出本次產生的所有 layer SVG 與 `BPV.svg`。
