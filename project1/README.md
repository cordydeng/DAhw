# Project 1 README

## 專案目的

本專案會讀取一份簡化 layout 輸入檔，找出 M1 layer 上的 pin，並在 M1 的最小外框內產生 blockage rectangles。程式也會輸出 SVG 圖，方便檢查原始 layout、產生後的 blockage，以及兩者的比較。

目前 spacing 設定為：

```cpp
MIN_SPACING = 0.5
```

也就是每個 pin 周圍會保留 0.5 的距離，不會被 blockage 覆蓋。

## 檔案架構

```text
project1/
├── project1.cpp        # 主程式
├── sample_input.txt    # 範例輸入
├── A_original.svg      # 原始 layout 視覺化結果
├── B_blockage.svg      # blockage rectangles 視覺化結果
├── AB_comparison.svg   # 原始 layout 與 blockage 結果比較圖
└── README.md           # 本說明文件
```

## 程式架構

`project1.cpp` 主要分成以下幾個部分：

### 1. 資料結構

主要結構包含：

```cpp
Layer
Shape
TextLabel
Layout
Pin
Rect
```

用途如下：

- `Layer`：記錄 layer 名稱與編號，例如 `M1 1`
- `Shape`：儲存 polygon 或 rectangle 幾何資料
- `TextLabel`：儲存 text/pin label 的名稱與座標
- `Layout`：整份輸入檔解析後的資料集合
- `Pin`：由 text label 對應到所在 shape 後得到的 pin
- `Rect`：最後輸出的 blockage rectangle

### 2. 輸入解析

函式：

```cpp
parse_layout()
```

會讀取輸入檔中的四個區塊：

```text
number_of_layers
number_of_polygons
number_of_rectangles
number_of_texts
```

polygon 會被轉成 Boost.Geometry 的 `Polygon`，rectangle 也會被轉成四邊形 polygon，方便後續做幾何運算。

### 3. 找出 M1 與 Pin

相關函式：

```cpp
m1_layer_number()
layer_bbox()
identify_pins()
```

流程如下：

1. 找出名稱為 `M1` 的 layer number。
2. 計算 M1 上所有 polygon/rectangle 的最小外框。
3. 檢查每個 text label 是否落在某個 shape 裡。
4. 若 text label 落在 shape 裡，該 shape 會被視為 pin。

### 4. 產生 blockage 區域

相關函式：

```cpp
buffer_polygon()
carve_blockage()
```

流程如下：

1. 先建立一個大矩形，範圍是 M1 的最小外框。
2. 對每個 pin 做 buffer，也就是向外擴張 `MIN_SPACING`。
3. 從大矩形中扣掉這些擴張後的 pin 區域。
4. 剩下的區域就是可以放 blockage 的區域。

例如 pin C 原本是：

```text
(0, 20) -> (3, 24)
```

spacing 為 `0.5` 時，會保留大約：

```text
x: 0 ~ 3.5
y: 19.5 ~ 24.5
```

這個區域不會產生 blockage。

### 5. 切成 rectangles

函式：

```cpp
decompose_to_rectangles()
```

切 rectangle 的流程目前以水平切割為主：

1. 收集 M1 最小外框的邊界座標。
2. 對每個 pin 向外擴張 `MIN_SPACING`。
3. 只取外擴後 pin bounding box 的左/右/下/上邊界作為切割線。
4. 用這些 x/y 座標切出許多小 grid cell。
5. 保留完全位於 blockage 區域內的小格子。
6. 由下往上逐層掃描每一個水平 y-band。
7. 在同一個 y-band 裡，將連續可用的小格子往右合併。
8. 不跨 y-band 往上合併，因此輸出的 rectangles 會以水平長條為主。
9. 合併後輸出成 `Rect`。

範例輸出：

```text
R1: 0.000 0.000, 40.000 9.500
R2: 0.000 9.500, 37.500 13.500
R3: 0.000 13.500, 40.000 19.500
R4: 3.500 19.500, 40.000 24.500
R5: 0.000 24.500, 40.000 45.500
R6: 0.000 45.500, 12.500 50.000
R7: 17.500 45.500, 40.000 50.000
```

切割線只來自 M1 最小外框與 pin 外擴後的邊界，不會再使用原始 pin 邊界切出額外薄片。

### 6. SVG 輸出

相關函式：

```cpp
write_svg_original()
write_svg_final()
write_comparison_svg()
```

程式會輸出三張 SVG：

- `A_original.svg`：原始 layout
- `B_blockage.svg`：產生後的 blockage rectangles
- `AB_comparison.svg`：左右比較圖

SVG 會標示：

- 每個圖形的左下角座標
- 每個圖形的右上角座標
- pin label 的座標

## 輸入格式

範例：

```text
number_of_layers 1
M1  1
end_of_layer

number_of_polygons  1
1    0 0, 0 10, 10 10, 10 0, 0 0
end_of_polygon

number_of_rectangles 1
1  0 20, 3 24
end_of_rectangle

number_of_texts 1
1  C  1 21
end_of_text
```

說明：

- `number_of_layers`：宣告 layer 資訊
- `number_of_polygons`：宣告 polygon，每列格式為 `layer x1 y1, x2 y2, ...`
- `number_of_rectangles`：宣告 rectangle，每列格式為 `layer xl yl, xh yh`
- `number_of_texts`：宣告 text label，每列格式為 `layer name x y`

如果 text label 的座標落在某個 shape 裡，該 shape 會被視為 pin。

## 編譯方式

此程式使用 Boost.Geometry。若使用 MSYS2 UCRT64，可先安裝 Boost：

```powershell
C:\msys64\usr\bin\pacman.exe -S --needed mingw-w64-ucrt-x86_64-boost
```

編譯：

```powershell
cd C:\Users\User\Desktop\DAhw
g++ project1\project1.cpp -std=c++17 -O2 -o project1\project1.exe
```

## 執行方式

```powershell
cd C:\Users\User\Desktop\DAhw\project1
.\project1.exe sample_input.txt
```

執行後會在終端機輸出：

- M1 最小外框
- 找到的 pins
- 最後產生的 blockage rectangles
- 產生的 SVG 檔名

也會產生或更新：

```text
A_original.svg
B_blockage.svg
AB_comparison.svg
```

## 輸出結果說明

終端機中的 rectangle 格式為：

```text
R編號: xl yl, xh yh
```

例如：

```text
R2: 0.000 9.500, 37.500 13.500
```

代表第二個 blockage rectangle 的左下角是：

```text
(0, 9.5)
```

右上角是：

```text
(37.5, 13.5)
```

## 注意事項

- 輸入檔必須定義 `M1` layer。
- pin 是透過 text label 的座標判斷，不是只看名稱。
- 目前 spacing 固定寫在程式中的 `MIN_SPACING`。
- Boost.Geometry 的 SVG mapper 會將座標轉換到 800x800 的 SVG 畫布中。
- SVG 上標示的是原始 layout 座標，不是 SVG 畫布座標。
