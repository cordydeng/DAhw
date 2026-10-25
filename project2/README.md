# Project 2－EOL DRC 檢查器

本程式使用 Boost.Geometry 實作線端（End-of-Line，EOL）的正前方 spacing 設計規則檢查。

## 規則參數

所有數值的單位皆為 μm。

```text
eolWidth        = 0.6
eolSpace        = 1.0
eolWithin       = 0.4
parallelLength  = 0.7
parallelHalo    = 0.4
```

檢查前，程式會使用 Boost.Geometry 的 `union_` 合併同一層的 M1 金屬圖形。因此，互相重疊或接觸的金屬會被視為同一個導體。

合併完成後，程式會逐一檢查所有直角 M1 polygon 的實際邊界。當邊界長度小於或等於 `eolWidth`，且相鄰的兩條邊皆朝導體內部延伸時，該邊界會被判定為 EOL edge。EOL edge 的判定只依據金屬本身的幾何形狀，不受附近是否存在其他圖形影響。

依題目圖 2(a)，每一個 EOL edge 的矩形檢查區域沿導體外側向前延伸 `eolSpace`，並沿 EOL 邊的方向從兩端各向外擴張 `eolWithin`，因此檢查區域的總寬度為「EOL 邊長 + 2 × eolWithin」。例如長度為 0.6 μm 的水平 EOL 邊，其前方檢查範圍為寬 1.4 μm、深 1.0 μm。

這個範圍包含正前方及前方兩側。

另外依題目第一張圖，每一個 EOL edge 的兩側各有一塊 parallel halo：與金屬側邊相距 `parallelHalo` 以內，並從 EOL 邊沿金屬往後延伸 `parallelLength`。例如垂直金屬的上端 EOL 位於 y = a，兩側 halo 的範圍為 y ∈ [a−0.7, a]、寬 0.4。

每一個 EOL edge 都一定會檢查上述三塊區域（前方一塊、兩側 halo 各一塊）。區域內沒有其他金屬代表規則通過，而不是規則未啟用。

若其他同層圖形侵入任一檢查區域，程式便會回報 EOL 違規。侵入的判定為「交集面積大於 0」，只有邊界接觸（例如間距恰好 1.0 或恰好 0.4）不算違規。終端機只列出違規項目；結果圖標示所有 EOL 邊，並只繪出發生違規的檢查區域。

## 檔案說明

```text
project2.cpp       程式原始碼
sample_input.txt   題目提供的輸入範例
README.md          編譯與執行說明
metal_clean.svg    程式產生的原始金屬圖
eol_drc.svg        程式產生的 DRC 結果圖
```

## 編譯

需要支援 C++17 的編譯器，以及 Boost 標頭檔（Boost.Geometry 為 header-only，不需連結函式庫）。

```
g++ project2.cpp -std=c++17 -O2 -o project2.exe
```

若編譯器找不到 `boost/geometry.hpp`，請用 `-I` 指定 Boost 標頭檔所在目錄，例如：

```
g++ project2.cpp -std=c++17 -O2 -I/opt/homebrew/include -o project2.exe
```

## 執行

```
./project2.exe sample_input.txt
```

程式只會在終端機列出偵測到的違規。每筆違規會顯示 EOL 編號、來源與目標金屬、違規原因（`EOL spacing < 1.0` 或 `parallel halo intruded`）、檢查區域座標，以及目標金屬實際侵入的範圍（intruding metal）。程式並產生下列檔案：

```text
metal_clean.svg
eol_drc.svg
```

無論是否有違規，程式都會產生以上兩張圖。沒有違規時，`eol_drc.svg` 與 `metal_clean.svg` 內容完全相同，僅顯示金屬圖形與編號，不加上 EOL 邊或違規標示。

`metal_clean.svg` 顯示合併後的金屬圖形。`eol_drc.svg` 顯示 DRC 結果：

- 藍色：金屬圖形。
- 鮮橘色透明區域：發生違規的完整檢查範圍（前方 spacing 區域或兩側 parallel halo）。
- 鮮紅色透明區域：其他金屬實際侵入檢查範圍的部分（目標金屬與檢查範圍的交集）。
- 深綠色粗線：所有被判定為 EOL 的邊。

通過檢查的範圍不會繪出。滑鼠停在鮮橘色範圍、鮮紅色區域或深綠色 EOL 邊上，可查看對應的 EOL 編號；鮮紅色區域另提供涉及的金屬編號與違規原因。EOL 編號對應終端機訊息中的 `E#`。

## 輸入格式

```text
number_of_layers 1
M1  1
end_of_layer

number_of_rectangles 1
1  4.0 0.0, 7.8 0.6
end_of_rectangle
```

程式亦支援 `number_of_polygons`。EOL 判定會使用 polygon 的實際邊界。目前支援本題使用的 Manhattan 幾何圖形，也就是所有邊皆與 X 軸或 Y 軸平行；非水平或垂直的邊不會被判定為 EOL edge。
