# Project 2－EOL DRC 檢查器

本程式使用 Boost.Geometry 實作題目規格中的線端（End-of-Line，EOL）設計規則檢查。

## 規則參數

所有數值的單位皆為 μm。

```text
eolWidth    = 0.6
eolSpace    = 1.0
eolParallel = 0.7
eolWithin   = 0.4
```

檢查前，程式會使用 Boost.Geometry 的 `union_` 合併同一層的 M1 金屬圖形。因此，互相重疊或接觸的金屬會被視為同一個導體。

合併完成後，程式會逐一檢查所有直角 M1 polygon 的實際邊界。當邊界長度小於或等於 `eolWidth`，且相鄰的兩條邊皆朝導體內部延伸時，該邊界會被判定為 EOL edge。EOL edge 的判定只依據金屬本身的幾何形狀，不受附近是否存在其他圖形影響。

程式會對每一個 EOL edge 檢查：

- 長度為 `eolSpace` 的正前方 EOL spacing 區域。
- 平行長度為 `eolParallel`、向外距離為 `eolWithin` 的兩側 halo 區域。

若其他同層圖形侵入任一檢查區域，程式便會回報 EOL 違規。沒有圖形侵入時仍會完成檢查，但通過的 EOL edge 不會出現在輸出中，輸出只保留違規項目。

## 檔案說明

```text
project2.cpp       程式原始碼
sample_input.txt   題目提供的輸入範例
README.md          編譯與執行說明
metal_clean.svg    程式產生的原始金屬圖
eol_drc.svg        程式產生的 DRC 結果圖
```

## 編譯

```bash
g++ project2.cpp -std=c++17 -O2 -o project2.exe
```

指令用法：

- `project2.cpp`：要編譯的原始碼。
- `-std=c++17`：使用 C++17 標準。
- `-O2`：啟用第二級最佳化。
- `-o project2.exe`：將輸出執行檔命名為 `project2.exe`。

## 執行

```bash
./project2.exe sample_input.txt
```

程式只會在終端機列出偵測到的違規，並產生下列檔案：

```text
metal_clean.svg
eol_drc.svg
```

`metal_clean.svg` 顯示輸入的原始金屬圖形。`eol_drc.svg` 顯示 DRC 結果，其中藍色為金屬圖形，紅色透明區域為發生違規的 EOL 檢查區域；通過檢查的區域不會繪出。

## 輸入格式

輸入格式沿用 Project 1：

```text
number_of_layers 1
M1  1
end_of_layer

number_of_rectangles 1
1  4.0 0.0, 7.8 0.6
end_of_rectangle
```

程式亦支援 `number_of_polygons`。EOL 判定會使用 polygon 的實際邊界。目前支援本題使用的 Manhattan 幾何圖形，也就是所有邊皆與 X 軸或 Y 軸平行；非水平或垂直的邊不會被判定為 EOL edge。
