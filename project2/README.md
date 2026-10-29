# Project 2－EOL DRC 檢查器

本程式使用 Boost.Geometry 實作線端（End-of-Line，EOL）的正前方 spacing 設計規則檢查。

## 規則參數

所有數值的單位皆為 μm。

```text
eolWidth    = 0.6
eolSpace    = 1.0
eolWithin   = 0.4
```

程式會檢查名稱為 `M` 加數字的所有 metal layer，例如 M1、M2、M3，各層使用相同規則參數。每個輸入矩形或多邊形保留原有 shape ID 與幾何形狀。程式另建立同層金屬聯集作為外露邊界的判定依據，不以聯集取代原始輸入；不同層不互相遮蔽，也不互相構成 EOL violation。

同層 metal 若完整包含另一個 metal（允許邊界重合），只保留外側的 metal 與其 ID，作為檢查及繪圖對象；無論大小圖形的輸入順序如何，結果相同。若兩者幾何完全相同，則保留先讀入的 ID。程式以 `bg::covered_by()` 判斷實際多邊形的完整覆蓋，不能只依包圍盒判斷。部分重疊及不同層的圖形仍各自保留；這裡判斷的是被另一個單一 metal 完整覆蓋，不是被多個 metal 拼起來覆蓋。

程式先從同層聯集的外環辨識 EOL：完整外露邊長須小於或等於 `eolWidth`，且兩端相鄰的邊皆朝金屬內部延伸。重疊處被遮蔽的邊及相接後的內部接縫不算 EOL。有效線端再映射回原始 shape 的外露部分，各自建立檢查區域；寬度判定使用完整的實際邊，避免把較寬的線端拆成多條短 EOL。

依題目圖 2(a)，每一個 EOL edge 的矩形檢查區域沿導體外側向前延伸 `eolSpace`，並沿 EOL 邊的方向從兩端各向外擴張 `eolWithin`，因此檢查區域的總寬度為「EOL 邊長 + 2 × eolWithin」。例如長度為 0.6 μm 的水平 EOL 邊，其前方檢查範圍為寬 1.4 μm、深 1.0 μm。

這個範圍包含正前方及前方兩側。

若其他同層圖形侵入檢查區域，程式便會回報 EOL 違規。終端機只列出違規項目；結果圖標示所有 EOL 邊，並只繪出發生違規的檢查區域。

兩個矩形端對端相接時，接縫已成為內部邊界，因此不會從該接縫建立 EOL 檢查區域；重疊本身也不構成 EOL spacing 違規。仍外露且符合條件的線端則繼續檢查。「僅邊界接觸不算違規」是指目標金屬只碰到有效檢查矩形的邊界、沒有進入其內部。

## 檔案說明

```text
project2.cpp       程式原始碼
sample_input.txt   題目提供的輸入範例
README.md          編譯與執行說明
metal_clean_M1.svg M1 的純金屬圖（每層各一張）
metal_clean_M2.svg M2 的純金屬圖（依實際圖層產生）
eol_drc.svg       所有圖層合併的 DRC 結果圖
```

## 編譯

```
g++ project2.cpp -std=c++17 -O2 -o project2.exe
```

## 執行

```
./project2.exe sample_input.txt
```

程式在終端機列出總違規數、各層違規數及明細。純金屬圖每層各一張，DRC 結果則將所有層合併成一張，直接放在執行時的目前工作目錄，例如：

```text
metal_clean_M1.svg
metal_clean_M2.svg
eol_drc.svg
```

每次作圖前，程式先清除目前目錄中本程式命名的舊 SVG，包括舊版的 `metal_clean.svg`、`eol_drc.svg`，以及 `eol_drc_M*.svg`／`metal_clean_M*.svg`（`M` 後須為數字，可含 `_L圖層編號` 後綴）。因此，從多層輸入改跑單層輸入時，不會留下上次其他圖層的結果。不清除其他名稱的 SVG，也不搜尋子目錄或建立額外資料夾。輸入解析與 EOL 檢查成功後才進行清除。

`metal_clean_M1.svg` 等純金屬圖只顯示該層 metal、編號與圖例，不畫 EOL 邊或違規標示。`eol_drc.svg` 顯示所有層的金屬、EOL 邊及違規位置，標題列出全部圖層的違規總數；沒有違規時仍輸出，顯示 `0 violation(s)`。DRC 圖的標示方式如下：

- 各圖層顏色的半透明區域：金屬圖形。
- 鮮橘色透明區域：發生 EOL spacing 違規的完整檢查範圍。
- 鮮紅色透明區域：其他金屬實際侵入檢查範圍的部分（目標金屬與檢查範圍的交集）。橘色與紅色區域的外框採所屬圖層顏色。
- 圖層顏色的粗線：所有被判定為 EOL 的邊。不同層的金屬採不同顏色半透明填滿，底部圖例列出各層名稱；`M2/S7` 標籤表示 M2 的 shape 7。

通過檢查的範圍不會繪出。滑鼠停在鮮橘色範圍、鮮紅色區域或 EOL 邊上，可查看對應的圖層與 EOL 編號；鮮紅色區域另提供涉及的金屬編號與違規原因。EOL 編號對應終端機訊息中的 `E#`，終端機同時輸出各層及全部違規數。

## 輸入格式

`sample_contained_metals.txt` 測試完整包含去重：涵蓋大小圖形的兩種輸入順序、邊界重合、跨層相同圖形，以及包圍盒包含但實際未覆蓋的凹多邊形。11 個輸入保留 8 個 metal，共 2 筆違規。

`sample_independent_metals.txt` 提供相接與重疊 metal 的範例：保留 5 個輸入 shape，排除內部接縫及被覆蓋的邊後，共 0 筆違規。可用 `.\project2_multilayer.exe sample_independent_metals.txt` 執行。

`sample_exposed_eol.txt` 另外測試端點相接、重疊、包含、部分遮蔽與線端變寬等情況。11 個輸入 shape 去除被完整包含的圖形後保留 10 個，共辨識出 8 個外露 EOL 線段；僅 M4 的分離金屬間產生 2 筆違規，其他層為 0 筆。

`sample_multilayer.txt` 提供 M1、M2、M3 與非金屬層 BPV 的範例。執行後應得到 M1、M2 各 2 筆、M3 為 0 筆，共 4 筆違規；BPV 不參與檢查。純金屬圖分層輸出，DRC 合併輸出。提供的 `project2_multilayer.exe` 可直接執行：

```powershell
.\project2_multilayer.exe sample_multilayer.txt
```

執行此範例後，目前工作目錄產生 `metal_clean_M1.svg`、`metal_clean_M2.svg`、`metal_clean_M3.svg` 與一張 `eol_drc.svg`，可直接開啟查看，不建立額外資料夾。

```text
number_of_layers 1
M1  1
end_of_layer

number_of_rectangles 1
1  4.0 0.0, 7.8 0.6
end_of_rectangle
```

程式亦支援 `number_of_polygons`。EOL 判定會使用 polygon 的實際邊界。目前支援本題使用的 Manhattan 幾何圖形，也就是所有邊皆與 X 軸或 Y 軸平行；非水平或垂直的邊不會被判定為 EOL edge。
