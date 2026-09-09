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
                                                         │  dd(BDD,尚未接上)    │
                                                         │  未來其他解法         │
                                                         └─────────────────────┘
```

## 檔案結構

```
CMakeLists.txt
cmake/              BuDDy 的取得與建置
docs/               FPDL 與 parser 的說明,samples/ 是文件裡指令的產出範例
protocols/          協定測資,一個目錄一個協定(.fpdl + 它引用的 .qasm)
include/fpdl/       前端:協定解析、路徑圖
include/ftec/       驗證核心:trie、backend 介面、走訪
src/fpdl/  src/ftec/
tools/              各個執行檔的進入點
tests/
backends/dd/        decision-diagram backend(自成一套,含自己的文件與測試)
```

## 建置

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build
```

不需要 autotools。BuDDy(decision-diagram backend 的依賴)由 `cmake/BuDDy.cmake` 直接當
一般 CMake target 編;已有 checkout 可用 `-DFTEC_BUDDY_SOURCE_DIR=<path>` 指過去省下下載。

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
| `--backend=NAME` | `mock` | 用哪個解法。`dd` 是真正的驗證,`mock` 只走結構(見下)。 |
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
