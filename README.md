# FTEC flag protocol general verifier

判斷一個 fault-tolerant 協定是否真的容錯:給定 FPDL 寫的協定描述與它引用的 QASM 電路,
枚舉所有可能的執行路徑,在每個 fault location 注入所有可能的 fault,然後檢查**是否存在
一條路徑,使得兩個無法區分的 error 相乘落在 `N(S)\S`**(也就是 decoder 修正其中一個就會
傷到另一個)。

```
.fpdl ──► fpdl::Parser ──┬─► code: [[n,k,d]] + generators ──► tau = ⌊(d−1)/2⌋
                          ├─► SE 宣告(qd/qm/qf, file)
                          └─► SymbolicPaths ──► ftec::build_dag ──► trie
                                                                     │
                                                          DFS + 條件路由
                                                                     ▼
                                                         ┌─── ftec::Backend ───┐
                                                         │  mock               │
                                                         │  dd(手寫 / BuDDy)   │
                                                         │  spbdd(SPBDD 函式庫)│
                                                         │  未來其他解法         │
                                                         └─────────────────────┘
```

## 檔案結構

```
CMakeLists.txt
cmake/              BuDDy、SPBDD(與 CUDD 版所需的 CUDD)的取得與建置
docs/               FPDL 與 parser 的說明,samples/ 是文件裡指令的產出範例
protocols/          協定測資,一個目錄一個協定(.fpdl + 它引用的 .qasm)
include/fpdl/       前端:協定解析、路徑圖
include/ftec/       驗證核心:trie、backend 介面、走訪
src/fpdl/  src/ftec/
tools/              各個執行檔的進入點,以及 bench_backends.py(backend 對跑)
tests/
backends/dd/        decision-diagram backend(自成一套,含自己的文件與測試)
backends/spbdd/     同一個模型,改用 SPBDD 函式庫實作(BuDDy 或 CUDD 版皆可)
```

## 建置

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build
```

BuDDy(`dd` backend 的依賴)由 `cmake/BuDDy.cmake` 直接當一般 CMake target 編,不需要
autotools;已有 checkout 可用 `-DFTEC_BUDDY_SOURCE_DIR=<path>` 指過去省下下載。

`spbdd` backend 預設也不需要 autotools:**SPBDD 的 `main` 現在是 BuDDy 版**,而那個 BuDDy
就是上面同一個 target(所以一個行程裡只有一份,兩個 backend 才能連進同一個執行檔)。

SPBDD 有兩份同一套 API 的實作,`cmake/SPBDD.cmake` **不從分支名判斷**,而是讀
`manager.hpp` 裡有沒有 `DdManager` 來認,configure 時會印出來:

```bash
cmake -S . -B build                                          # main:BuDDy 版
cmake -S . -B build -DFTEC_SPBDD_GIT_TAG=try/CUDD_backend    # CUDD 版
```

只有 CUDD 版**要 autoconf / automake / libtool**(CUDD 的 `config.h` 是幾十個探測出來的
巨集,手寫不划算):

```bash
sudo apt install -y build-essential autoconf automake libtool   # Debian/Ubuntu
brew install autoconf automake libtool                          # macOS
```

不想要 spbdd backend 就整個關掉,其餘照舊:

```bash
cmake -S . -B build -DFTEC_ENABLE_SPBDD=OFF
```

已有 checkout 可用 `-DFTEC_SPBDD_SOURCE_DIR=<path>`、`-DFTEC_CUDD_SOURCE_DIR=<path>` 指
過去。CUDD 只在第一次建置時編一次。

## 使用

```bash
./build/ftec-verify <protocol.fpdl> [選項]
```

```bash
./build/ftec-verify "protocols/CR17_[[5,1,3]]/CR17_[[5,1,3]].fpdl" --backend=dd
./build/ftec-verify "protocols/CR17_[[5,1,3]]/CR17_[[5,1,3]].fpdl" --dag
```

| 選項 | 預設值 | 說明 |
|---|---|---|
| `--backend=NAME` | `mock` | 用哪個解法。`dd` 與 `spbdd` 是真正的驗證,`mock` 只走結構(見下)。兩者都**不做動態變數重排**(理由見下),要開就用 `spbdd:sift`。`spbdd-fixed` 是 `spbdd` 的別名,保留是因為下面的量測表格用了這個名字。 |
| `--bound=N` | 倍增搜尋 | 固定 BMC bound。不給的話從 16 開始倍增,直到展開完整為止(上限 65536)。 |
| `--max-paths=N` | `5000` | 符號路徑數超過就放棄。 |
| `--first` | 關閉 | 遇到第一條不受保護的路徑就停。`min_fault_count` 仍然正確,只是不再列出其他失效路徑。 |
| `--dag` | 關閉 | 只印合併後的路徑結構然後結束,不做走訪。 |

每次跑完都會印出成本,展開與走訪分開計:

```
expansion       : 4.90 s
traversal       : 3.36 s
total runtime   : 8.27 s
peak memory     : 66.9 MiB
```

分開計是有意義的——CB18 [[17,1,5]] 的 8.3 秒裡有 4.9 秒花在 BMC bound 的倍增搜尋上,
跟走訪本身無關。`--bound=N` 若已知答案就能省下這段。

其餘工具維持原樣:`fpdlc`(解析成 paths JSON)、`fpdl-path-graph`、`fpdl-path-dag`、
`fpdl-dag-graph`(視覺化),以及 `build/backends/dd/dd-propagate`(直接對一串 QASM 跑
decision-diagram backend,不經過 FPDL)。

## 為什麼要合併路徑

協定的符號路徑共用很長的前綴。把它們合併成 **trie** 之後深度優先走訪,每個 SE 電路只算
一次而不是每條路徑各算一次:

| 協定 | paths | SE 節點 | 逐路徑列舉需要 | 倍數 |
|---|---|---|---|---|
| CR17 [[5,1,3]] | 5 | 8 | 18 | 2.3× |
| Bha23 [[5,1,3]] | 13 | 24 | 62 | 2.6× |
| CB18 [[17,1,5]] plain | 21 | 28 | 97 | 3.5× |
| **CB18 [[17,1,5]]** | **532** | **1000** | **12538** | **12.5×** |

(上表用 `--bound=200`,好跟舊數字對照;實際收斂需要 512,見下。)

深度優先還有第二個好處:每個節點的狀態集算完就留在該層的 stack frame,底下所有 subtree
共用,**不需要 memo 表**;同時存活的只有 root→目前節點這一條鏈的狀態,而不是整個 frontier。
`tests/ftec_tests.cpp` 會斷言合併後的結構真的是樹——如果 builder 出錯把節點接到兩個父節點
上,走訪會悄悄退化成逐路徑列舉,唯一的症狀只是變慢。

## BMC bound 會自動收斂,而且不能只看 `truncated`

符號展開需要有限的 transition bound。`ftec-verify` 用倍增法找到讓展開完整的最小 bound,
因為**一次解析算不算完整要看兩件事**:`ParseResult::truncated`(路徑總數被砍)以及
每條路徑各自的 `bound_exceeded`(那條路徑被砍)。

實測 CB18 [[17,1,5]]:

| bound | paths | `truncated` | `bound_exceeded` 的路徑數 |
|---|---|---|---|
| 200 | 532 | **false** | **120** |
| 256 | 889 | false | **120** |
| 512 | 1701 | false | 0 |

在 bound=200 與 256 時 `truncated` 都是 **false**,但各有 **120 條**路徑其實被截斷了。
只檢查 `truncated` 會得到「分析完整」的錯覺,而實際上有一批路徑只走到被砍的地方——而且
從 200 加到 256 雖然多找出 357 條路徑,被截斷的那 120 條**一條都沒有解決**,所以「加一點
bound 看看數字有沒有變」也不是可靠的判準。

## Backend

`ftec::Backend` 的粒度是**一個電路**而不是一整條路徑:

```cpp
virtual std::vector<std::pair<Outcome, StateId>> step(StateId, const CircuitRef&) = 0;
```

只有 backend 知道一個電路實際能產生哪些測量結果,在那裡分裂才讓 driver 能依條件路由,
也才讓前綴共用有意義。`merge` 不是可選的:同一節點、相同 record、相同 fault 數的兩個
狀態是同一個情況(不管 fault 分布在哪一輪),分開處理會報出 decoder 其實分得出來的假失效。

目前的實作狀態:

- **dd** —— decision diagram over Pauli sets,在 `backends/dd/`。把 error 集合帶過電路的
  每一條指令、在每個 2-qubit gate 注入全部 16 種 Pauli、在每次 `measure` 依被測 qubit 的
  **x 分量**分裂、照 `reset` 指令清掉 ancilla,最後在每個 terminal 問「有沒有兩個 error
  的乘積落在 `N(S)\S`」。
- **spbdd** —— **同一個模型**,改用 [SPBDD](https://github.com/jtsai1120/SPBDD) 函式庫寫,
  在 `backends/spbdd/`。差別不在語意而在誰來做事:fault injection、reset、measurement
  分裂、`N(S)\S` 查詢本來就是那個函式庫的字彙,所以 `dd` 裡那層把它們拆成 quantification
  與 symplectic 換基的程式碼(`pauli_bdd.cpp` + `stabilizer.cpp`,約 850 行)在這裡不存在。

  SPBDD 有 **BuDDy 版(`main`)與 CUDD 版(`try/CUDD_backend`)** 兩份同一套 API 的實作,
  這個 backend **兩份都能編**,由 `cmake/SPBDD.cmake` 讀原始碼認出來。兩份都跑同一份走訪
  與快取程式碼,所以量到的差就只是底下那層。唯一的 API 差別在重排控制:CUDD 版透過
  `Manager::raw()` 可以拿到全部方法與門檻,BuDDy 版只有開/關。

  三者必須對每個協定給出**完全相同的判決**,不同就是 bug——`tools/bench_backends.py`
  就是拿來檢查這件事,順便量價差。
- **mock** —— 不模擬任何物理:從全零 outcome 出發,在預算內才回報偏離的 outcome。用來
  檢查走訪、路由與 record 記帳,以及證明 `ftec::Backend` 真的是抽象層。
  **它不判斷容錯性**,CLI 的輸出也會這樣說。

### 已驗證的結果

| 協定 | code | tau | 結果 | traversal |
|---|---|---|---|---|
| CR17(Chao–Reichardt flag) | [[5,1,3]] | 1 | 無不受保護的路徑 | 0.01 s |
| Bha23 | [[5,1,3]] | 1 | 無不受保護的路徑 | 0.01 s |
| CB18 plain | [[17,1,5]] | 2 | 無不受保護的路徑 | 1009 s |
| LL25 | [[19,1,5]] | 2 | 無不受保護的路徑 | 1338 s |

CB18 [[17,1,5]](完整版,532 條路徑)尚未跑過。

### 效能:瓶頸是重複,不是 BDD

LL25 的第一次執行花了 **4739 秒**,峰值記憶體只有 131 MiB——診斷很清楚:**diagram 從來不是問題,走訪在重複做同樣的事**。

放大的來源是「在每個被測位元上分裂狀態」。`FSE_f` 寫 42 個古典位元、有 132 個
2-qubit gate,所以在 tau=2 下能產生上千種相異 record,再沿路徑相乘——**4 條符號路徑
變成 1,188,764 個 record**,5 個 SE 節點被跑了 1,042,681 次。

但 `step` 和 `check` **只看集合、不看 record**,而不同的 record 經常留下相同的集合
(record 是 decoder 觀察到的東西,不是電路留下的東西)。所以兩者都改成以集合為鍵快取。
BuDDy 是 hash-consed 的,相同的集合就是同一批節點,拿 root id 當鍵是**精確**的而非啟發式。

| 協定 | step 命中 | check 命中 | 相異 step | 相異 check | traversal |
|---|---|---|---|---|---|
| LL25 | 88% | 95% | 123,863 | 55,278 | 4739 s → **1338 s** |
| CB18 plain | 95% | 98% | 91,985 | 30,676 | 1009 s |

**3.5×,不是命中率暗示的 10–20×**——因為被快取掉的多半是**便宜**的操作(集合小、好算),
留下來的相異狀態才是貴的那些。這是快取常見的現象,值得記下來免得下次又高估。

### 三方比較:手寫 vs SPBDD/BuDDy vs SPBDD/CUDD

條件全部拉平(都不重排,`dd` 用 `1<<20`/`1<<16`,也就是 SPBDD 自己的預設),CB18
[[17,1,5]] plain,機器閒置、兩個執行檔背靠背跑,`dd` 兩輪各是 142.5 / 144.0 s(差 1%,
所以絕對值可以跨輪比):

| | traversal | peak | vs dd |
|---|---|---|---|
| **dd**(手寫 + BuDDy) | **142.5–144.0 s** | 196 MiB | 1.00× |
| SPBDD on **BuDDy** | 155.7 s | 240 MiB | 0.92× |
| SPBDD on **CUDD** | 256.8 s | 440 MiB | 0.55× |

兩個結論:

- **BuDDy 版明顯優於 CUDD 版**:快 1.65×、記憶體少 1.83×。同一份 API、同一份走訪程式碼,
  只換底下的套件。
- **SPBDD 這層抽象大約值 8%**(慢 8%、記憶體多 22%)。用它換掉 850 行自己維護的 Pauli 層,
  這個價錢很便宜。

(下面 CUDD 版的重排實驗是在兩者交換位置之前做的,那時 CUDD 版還在 `main`。數字仍然有效,
只是要用 `-DFTEC_SPBDD_GIT_TAG=try/CUDD_backend` 才重現得出來。)

### dd vs spbdd:換掉 BDD 函式庫值多少

```bash
tools/bench_backends.py --build-dir build                       # 全部協定
tools/bench_backends.py --protocol CB18 --backends dd,spbdd:sift
```

腳本會把每個協定丟給每個 backend、量 traversal 時間與峰值記憶體,並且**先比判決**:
`paths` / `records` / `circuits` / `min_t` / 失效路徑清單有任何一項不同就大聲報出來,
因為那是 bug 而不是效能差異。所有協定的判決到目前為止**完全一致**,包含三個
distance-3 的失效案例(Bha23 fig6、LL25 `[2,2]`、LL25 `[2,2]^T`)。

distance-5 的協定(每次 4–12 分鐘,才量得出東西;distance-3 的都在 0.05 秒以下,只有
量化雜訊):

| 協定 | dd | spbdd(sifting) | spbdd-fixed(不重排) | 峰值記憶體 dd → spbdd |
|---|---|---|---|---|
| CB18 [[17,1,5]] plain | 277 s / 231 s | 441 s(0.63×) | 253 s(0.91×) | 181 → 441 MiB |
| LL25 [[17,1,5]] `[1,1,1,1,1,...]^T` | 449 s / 327 s | 742 s(0.61×) | 391 s(0.83×) | 162 → 406 MiB |
| LL25 [[17,1,5]] `[2,2,2,1,1]^T` | 386 s / 376 s | 655 s(0.59×) | 443 s(0.85×) | 203 → 441 MiB |

`dd` 有兩個數字是因為它在每一輪 sweep 裡都重跑一次當基準——機器上還有別的東西在跑,
同樣的工作量會差到 37%(449 s vs 327 s)。所以**倍率只在同一輪 sweep 內比才有意義**,
表格裡的括號都是這樣算的;跨欄比絕對秒數會被雜訊蓋過去。

量的是同一件事:兩個 backend 的快取結構是逐行照搬的(同樣兩個快取、同樣的鍵、同樣在
`grow_to` 清掉),只有指紋取什麼不同——`dd` 取 BuDDy 的節點索引 `bdd::id()`,`spbdd` 取
CUDD 的節點位址 `Bdd::node()`,兩者都靠套件的 canonical 保證而不是啟發式。實測也對得起
來,CB18 [[17,1,5]] 上三個 backend 的記帳**逐字相同**:

```
dd           2082957 step(s), 1991697 from cache (95%); 2374764 check(s), 2344088 from cache (98%) | 30676 checked, 91260 stepped
spbdd        (同上,一字不差)
spbdd-fixed  (同上,一字不差)
```

順帶回答了一個本來要擔心的問題:**CUDD 的重排不會動到這些鍵**,它是就地改節點,位址跟
語意都保住。

三件事:

1. **判決一樣、走訪一樣,所以這是純粹的價差**,不是兩個不同的答案。
2. **慢的是重排,不是換函式庫**。關掉重排之後 SPBDD 跟手寫的 BuDDy 版本大致打平
   (0.83–0.92×),開著就掉到 0.6× 上下。細節見下一節。
3. **記憶體是 2.2–2.4 倍,但不是 diagram 變大**。CUDD 自己回報的 `memory_in_use` 開不開
   重排都是 240 MB 上下,而 `dd` **整個 process** 的峰值才 181 MiB——所以那是 CUDD 的
   unique table / cache 把自己撐大的:`dd` 那邊 `bdd_init(100000, 10000)` 給了明確的起始
   大小,`spbdd` 這邊 `ManagerConfig` 全留 0(用 CUDD 預設)。`unique_slots`、`cache_size`、
   `max_memory` 三個欄位都可以壓,還沒試。

換句話說,以「跑得更快」為目的的話目前**沒有賺**;真正的好處在別處——fault injection、
reset、measurement 分裂、`N(S)\S` 查詢都是函式庫的公開 API,`backends/spbdd/` 只剩下
電路走訪與快取,`pauli_bdd.cpp` + `stabilizer.cpp` 那 850 行等價物不用自己維護。

### 重排:21 種方法與各種門檻都試過了,沒有一個比關掉好

SPBDD 的 `ManagerConfig` 只開放「重排開/關」,方法寫死成 `CUDD_REORDER_SIFT`。這裡的
backend 因此繞過它,從 `Manager::raw()` 拿到 `DdManager*` 直接呼叫 `Cudd_AutodynEnable`
與 `Cudd_SetNextReordering`,把 CUDD 全部 21 種方法與觸發門檻都接成 CLI:

```bash
./build/ftec-verify <protocol.fpdl> --backend=spbdd:symm_sift
./build/ftec-verify <protocol.fpdl> --backend=spbdd:sift:1000000   # 門檻 = 首次觸發的節點數
tools/bench_backends.py --protocol CB18 --backends spbdd:none,spbdd:sift,spbdd:window2
```

CB18 [[17,1,5]] plain,判決全部維持 clean,倍率一律以**同一輪 sweep 內**的 `spbdd:none`
(248–251 s)為基準:

| method | traversal | vs none | 重排次數 | 重排耗時 |
|---|---|---|---|---|
| **none** | **248 s** | **1.00×** | 0 | — |
| sift `@10⁶` / `@10⁷` | 249 / 251 s | 1.01× / 1.00× | 0 | 0 s |
| window2 | 271 s | 0.91× | 8 | 0.4 s |
| window3 | 286 s | 0.87× | — | — |
| symm_sift | 333 s | 0.75× | — | — |
| window4 | 341 s | 0.73× | — | — |
| lazy_sift | 345 s | 0.72× | — | — |
| sift | 347 s | 0.71× | 12 | 20.2 s |
| sift `@10⁵` | 347 s | 0.72× | 4 | 16.8 s |
| window4_conv | 360 s | 0.69× | 11 | 12.8 s |
| random_pivot | 372 s | 0.67× | 9 | 32.8 s |
| group_sift | 390 s | 0.64× | — | — |
| window3_conv | 398 s | 0.62× | 11 | 2.7 s |
| symm_sift_conv | 408 s | 0.61× | 12 | 91.3 s |
| sift_conv | 419 s | 0.59× | 12 | 85.7 s |
| random | 451 s | 0.55× | — | — |
| group_sift_conv | 462 s | 0.54× | 12 | 133.6 s |
| window2_conv | 570 s | 0.44× | 10 | **0.7 s** |
| annealing / genetic | > 900 s | — | — | — |
| exact | `CUDD failed in Cudd_bddAnd` | — | — | — |
| linear / linear_conv | **不能用**,見下 | — | — | — |

三個結論,前兩個推翻了先前寫在這裡的猜測:

**門檻是開關,不是旋鈕。** 把首次觸發門檻從預設的 4004 拉到 10⁵,重排從 12 次降到 4 次,
總時間**一秒都沒省**(347.2 → 346.6);拉到 10⁶ 就一次都不觸發,直接退化成 `none`。中間
沒有甜蜜點。

**代價不在重排程式裡。** `sift` 比 `none` 慢 97 s,其中只有 20 s 花在重排;`window2_conv`
慢 320 s,重排只花 **0.7 s**。真正在動的是**運算次數**:

| | cache lookups | 命中率 | live nodes | peak nodes | traversal |
|---|---|---|---|---|---|
| none | 2.866 G | 95% | 584,951 | 4,884,138 | 255 s |
| sift | 3.575 G(+24.7%) | 95% | 481,626 | 5,074,230 | 347 s |
| window2 | 3.042 G(+6.2%) | 95% | 582,038 | 4,840,192 | 279 s |

命中率三者都是 95%——重排**沒有**讓 cache 變難用。變多的是 lookup 本身,因為 CUDD 每次
重排都要 flush computed table(裡面的結果引用了層級已改變的節點),被 memoise 掉的運算
得重算一遍。扣掉重排時間後,每一次多出來的 lookup 兩種方法代價幾乎一致——sift 107 ns、
window2 134 ns,正好是 240 MB 工作集上隨機存取的 DRAM 延遲量級。**多出來的時間就是多出來
的運算**,不是別的。

而買到的東西是零:live nodes 少 17.7%,但 peak nodes 反而**變多**(重排自己的 swap 要
配置),`memory_in_use` 239 → 246 MB 幾乎沒動。工作集是 peak 和表的大小決定的,不是最終
的 live 數。

**`linear` 與 `linear_conv` 不是慢,是不能用。** 這兩個是 CUDD 唯二會套用**線性變換**的
方法——它們不只重排層級,還會把一個變數換成兩個變數的 XOR。而 SPBDD 整個設計建立在相反
的前提上,`paulispace.hpp` 自己寫得很清楚:「CUDD may reorder levels freely; all code here
is written against variable numbers, which never change」。變數的**意義**一旦被改寫,SPBDD
用變數編號組出來的 quantification cube 就不是它以為的那個 cube,CUDD 會依哪邊先壞掉而
給出兩種死法:

```
linear       cuddGarbageCollect: problem in table 5, dead count != deleted   （abort）
linear_conv  Error: Can only abstract positive cubes
```

backend 因此**直接拒絕**這兩個並說明原因,而不是讓人自己去解讀 CUDD 的錯誤訊息。這值得
往上游報一個 issue:SPBDD 應該在 API 上排除它們,或至少寫進文件。

所以不重排就是這個工作量上的正確設定。**`--backend=spbdd` 現在預設不重排**(也是 SPBDD
自己 `ManagerConfig` 的預設),`spbdd-fixed` 變成它的別名 —— 保留是因為上面的表格用了那個
名字。要重現表格裡開著 sifting 的那幾列,用 `spbdd:sift`。

`dd` 也在 `main` 上做了同一件事(commit `71ebde1`):`bdd_autoreorder(BDD_REORDER_NONE)`
加上 `bdd_init(1<<20, 1<<16)`,對 CB18 是 1.59×、對 LL25 [[19,1,5]] 是 2.25×。SPBDD 那邊
的節點表與 cache 預設本來就是 `1<<20` / `1<<16`,所以兩個 backend 現在**問 BuDDy 要的東西
完全一樣**,剩下的差異才真的是函式庫那一層。

### 下一步的加速:把 record 放進 BDD

剩下的成本是那 12 萬次相異 step 和 5.5 萬次相異 check,而它們仍然源自同一件事:
**在每個被測位元上分裂**。

結構性的解法是不要分裂,而是**為每個被測位元開一個 BDD 變數**,用等式約束綁到被測
qubit 的 x 分量上(`S := S ∧ (m_i ⟺ x_q)`,然後才 reset),分裂只發生在 **guard 真正
區分的地方**——通常是 2 路,不是 2⁴²。

`find_undetectable_logical_pair` **不用改**:它的判準是「同 σ、不同 λ」,靠的是把 σ、λ
當座標而把 stabilizer 分量量化掉;record 變數本來就座標對齊,只是**多出來的 syndrome
座標**,不參與基底變換也不被量化。判準自動變成「同 record、同 σ、不同 λ」,正是 decoder
面對的問題。

這會把 step 從十萬次降到與 DAG 邊數同量級,但代價是變數變多(LL25 約 +210 個)、BDD
可能變大,而且要先確認「同一節點的出邊條件互斥且窮盡」——否則粗粒度路由會把一個 record
送進兩條分支。**還沒做。**

## 還沒做的事

1. **QASM 前端升級**(最大的一塊):自訂 `gate` 內聯、`reset`、`measure`、`bit` 宣告、
   純量 `qubit`、任意暫存器名稱與三個暫存器(qd/qm/qf)、`s`/`sdg` gate。
   `s`/`sdg` 在 phase-free 下是同一個變換 `(x,z) → (x, x⊕z)`,一份實作涵蓋兩者。
2. **傳遞模型改成「`measure` 事件即分裂」**。目前 dd backend 在檔案結尾才分裂,但
   `FSE_b.qasm` 用一顆 ancilla 配合 `reset` 產生 4 個 syndrome bit,那個模型表達不出來。
   順序是**先把測量結果寫進 record,再 reset**。
3. **接上 dd backend**,先用手改成受限方言的 CR17 驗證管線,再換成真正的前端。
