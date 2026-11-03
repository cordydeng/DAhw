# Project 3－GDS & Qt object_viewer

本程式以老師提供的 `object_viewer`（Qt5）為基礎，加入題目中 Karl 建議的三項功能：將畫面存成圖檔、合併多條 path、追蹤同一 layer 上相連的圖形（connectivity / color propagation），另外加入多選、移動圖形與 Undo。啟動畫面、寫死的範例圖形與開啟 GDS 的行為皆與原程式相同。資料夾結構與原始的 `../object_viewer_02102023/` 相同，可單獨編譯執行；原始資料夾保持不動，作為對照。

## 檔案說明

| 檔案 | 內容 |
| --- | --- |
| `main.cpp` | 建立視窗，進入 event loop |
| `main_window.h`、`main_window.cpp` | 選單、快捷鍵、Open / Query / Save Image / Undo / Merge Paths / Connectivity、互動建立圖形、寫死的範例圖形 |
| `canvas.h`、`canvas.cpp` | 繪圖區：存放圖形、繪製、滑鼠選取（多選、輪流選取重疊圖形）、拖曳移動、Undo、Query、合併 path、connectivity |
| `shape.h`、`shape.cpp` | Rectangle / Boundary / Path 圖形類別與 layer 顏色 |
| `transform.h` | 使用者座標與螢幕座標轉換、zoom、pan |
| `gdsFileParser/` | GDS parser（老師提供，未修改） |
| `gdsFileParser/testData/` | 測試用 GDS 檔（與原始資料夾相同） |
| `images/`、`object_viewer.qrc` | 工具列圖示 |
| `report.md`、`report_images/` | 專題報告與附圖 |

## 編譯

需要 Qt5。與原始的 `object_viewer` 相同，在 `project3/object_viewer/` 目錄執行：

```bash
qmake -o Makefile object_viewer.pro
make
```

老師的 CentOS-7 機器（Qt 5.12.12）：

```bash
export LD_LIBRARY_PATH=/opt/qt5.12.12/5.12.12/gcc_64/lib:$LD_LIBRARY_PATH
export QT_QPA_FONTDIR=/usr/share/fonts/dejavu
/opt/qt5.12.12/5.12.12/gcc_64/bin/qmake -o Makefile object_viewer.pro
make
```

EE 工作站（CentOS-8，Qt 5.15.3）：

```csh
setenv QT_QPA_FONTDIR /usr/share/fonts/dejavu
/usr/lib64/qt5/bin/qmake -o Makefile object_viewer.pro
make
```

## 執行

需要圖形介面，例如以 MobaXterm 或 `ssh -X` 連線；VSCode 內建的終端機沒有 display，無法開啟視窗。

```bash
./object_viewer
```

由 File → Open 選擇 GDS 檔，終端機會逐行印出每個 GDS record，視窗中顯示寫死的範例圖形；按 Cancel 則只顯示範例圖形。

| 操作 | 功能 |
| --- | --- |
| 滑鼠左鍵 | 選取游標下最上層的圖形（紅色虛線） |
| Shift + 滑鼠左鍵 | 加入游標下最上層、尚未選取的圖形；在同一點重複按可一層層往下加選 |
| Ctrl + 滑鼠左鍵 | 取消游標下最上層、已選取的圖形 |
| N | 在上次點選的位置，改選下一層的圖形（到最底層後回到最上層） |
| Shift + N | 在上次點選的位置，改選上一層的圖形（到最上層後回到最底層） |
| Esc | 取消全部選取 |
| 拖曳選取的圖形 | 移動選取的圖形 |
| Ctrl+Z | 復原上一步（新增、刪除、移動、合併） |
| Q | 顯示選取圖形的類型、layer、座標 |
| R / B / P | 互動建立 Rectangle / Boundary / Path；Boundary 與 Path 按 Enter 結束 |
| M | 將選取的 path（兩條以上、同一 layer、端點互相重疊）合併成一條 |
| C | 以橘色標示與選取圖形相連的同 layer 圖形 |
| Delete | 刪除選取的圖形 |
| F | 顯示全部 |
| `[` / `]` | 放大 / 縮小 |
| 方向鍵 | 平移 |
| Ctrl+S | 將目前畫面存成圖檔 |

## 測試方式

先 File → Open → Cancel 顯示寫死的圖形。

1. **輪流選取：** 在綠色三角形內點一下，按 N 改選下層的黑色大矩形，按 Shift + N 回到上層的三角形。
2. **Connectivity：** 按 R 畫三個依序重疊的矩形與一個獨立的矩形，點選第一個後按 C，重疊的三個變成橘色。
3. **合併 path：** 按 P 畫兩條以上的 path（按 Enter 結束），後一條的起點要畫在前一條的終點上，使端點重疊；以 Shift + 左鍵選取全部後按 M。端點沒有重疊的 path 不會被合併。
4. **移動與 Undo：** 點選一個圖形後拖曳到別處，再按 Ctrl+Z 回到原位。
5. **存成圖檔：** 按 Ctrl+S，輸入檔名後存檔。
6. **Parse GDS：** File → Open 選擇 `gdsFileParser/testData/two_stage/two_stage.gds`，於終端機查看輸出的 record。
