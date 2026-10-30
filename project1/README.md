# Project 1：Multi-layer BPV Generator

## 專案說明

此程式讀取簡化的 layout 文字檔，依 metal layer 分別辨識 pins、計算安全間距，並產生 blockage rectangles。

每一層都會獨立運算，不會因為不同 layer 在相同 XY 位置重疊，就互相扣除 blockage。完成後程式會輸出：

- 每個有效 layer 各一張 SVG，例如 `M1.svg`、`M2.svg`、`M3.svg`。
- 每個 layer 各一張 BPV 圖，例如 `BPV_M1.svg`、`BPV_M2.svg`。

圖中不顯示座標或 pin 名稱。各 layer 的獨立 SVG 使用一致配色；各層 BPV 圖中的 blockage 使用灰色，pins 依 metal layer 使用不同顏色。

## 專案檔案

```text
project1/
├── project1.cpp        # C++17 主程式
├── M*.svg              # M* layer 結果
├── BPV_M*.svg          # 各 layer 獨立的 BPV 結果
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

各 layer SVG 直接繪製完整的 blockage geometry，不顯示 rectangle 切割線。圖中灰色為 blockage、淡藍色為輸入檔中該 layer 原有的 polygons 與 rectangles；原始 blocks 的重疊區會用較深的同色系藍色標示，深藍色則為 pins。左上角圖例會標示各元素。各層 BPV 圖則會顯示該 layer 切割後的 rectangles。

`write_bpv_svg()` 為每個 layer 輸出獨立的 `BPV_<layer>.svg`，不再合併。每張圖顯示該層的 bounding box、水平 blockage rectangles 和 pins。每層使用自己的 pin keep-out，並保留 rectangle 邊框與 layer 名稱。

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

`multi_layer.txt` 定義 M1、M2、M3。所有 pins 都接觸各 layer bounding box 的左側或右側邊界，部分 pins 也會在不同 layer 的 XY 投影上重疊。M1 另外包含一個 polygon 與一個不帶 text label 的 rectangle，它們在 `(28, 28) -> (34, 36)` 重疊，用來確認：

- 每層 blockage 是否獨立產生。
- 邊界 pin 的 spacing 是否會正確被 layer bounding box 截斷。
- 同一 XY 位置上的不同 layer 是否互不干擾。
- 同一 layer 的原始 block 重疊區是否會以較深藍色顯示。
- 各 layer 的 BPV 是否分別輸出到獨立的 SVG。

執行後會產生：

```text
M1.svg
M2.svg
M3.svg
BPV_M1.svg
BPV_M2.svg
BPV_M3.svg
```

## 編譯與執行

在 `project1` 目錄中編譯 C++ 程式：

```bash
g++ project1.cpp -std=c++17 -O2 -o project1
```

用法：

```bash
./project1 <input.txt>
```

例如：

```bash
./project1 multi_layer.txt
```

每次開始輸出圖檔前，程式會先刪除目前執行目錄中所有 `.svg` 檔，再產生本次的 layer SVG 與各層 BPV SVG。請在專案輸出資料夾中執行程式，避免刪除其他需要保留的 SVG。

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

最後會列出本次產生的所有 layer SVG 與各層 BPV SVG。
