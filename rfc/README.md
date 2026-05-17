# 戰略沙盒遊戲 RFC 規格書包

本資料包把企劃拆成多份 RFC 風格 Markdown 文件，避免單一巨大規格書難以維護。

## 建議閱讀順序

1. `RFC-000-overview.md`：總體定位、不可違背設計原則、暫名候選
2. `RFC-001-development-contract.md`：RFC 文件規則與開發拆分方法
3. `RFC-010-prototype-v0_1.md`：第一個可玩原型範圍
4. `RFC-020-politics-internal.md`：內政、派系、法律、玩家權力
5. `RFC-030-economy-budget.md`：經濟、預算、稅制、產業長期方向
6. `RFC-040-diplomacy-war-ai.md`：外交、世界 AI、自動戰爭
7. `RFC-050-events-hidden-truth.md`：事件、隱藏真相、情報失真
8. `RFC-060-technical-architecture.md`：純 C++ 技術架構
9. `RFC-070-data-formats.md`：JSON 資料格式範例
10. `RFC-080-research-formulas.md`：有研究基礎的簡化公式框架
11. `RFC-090-roadmap.md`：分階段開發路線，拆到可執行任務

## 狀態

- 文件版本：Draft v0.1
- 目標平台：PC
- 開發環境：純 C++，無引擎，SVG 優先
- 主要開發者：你 + AI agents
- 規格風格：RFC / GDD / TDD / Roadmap 混合

## 實作進度（與 RFC 對應）

RFC 是設計意圖；實作版本由 milestone PR 推進。RFC 的部分內容已在
M0 / M1 中落地，部分仍是未來工作：

- **M0（已完成）** — 技術骨架。實作 RFC-060（純 C++ 架構）、
  RFC-070 §6 simulation config 載入、RFC-070 §1 country JSON minimal
  shape、RFC-080 §3 fiscal capacity / corruption 變數定義（M1.1 起作為
  state 欄位），以及 RFC-090 §M0 列出的全部 sub-tasks。
  詳見 `../docs/milestone-0-result.md`。
- **M1（已完成）** — 單國內政原型。M1.1 落實 RFC-060 §3
  `CountryState` 主要欄位；M1.2 落實 RFC-060 §3 / RFC-070 §2
  `FactionState`（含 RFC-010 §2.5 列出的派系類型）；M1.3 落實
  RFC-060 §3 / RFC-010 §2.4 `BudgetState`（七類別預算分配，作為巢狀
  欄位嵌在 `CountryState`）；M1.4 落實 RFC-060 §3 / RFC-070 §3
  `PolicyData` 與 `PolicyEffect`（含 RFC-010 §2.6 列出的政策類別與
  十個範例 policy fixture），但**尚未套用效果**。**M1.5（PolicySystem
  apply effects）** 為 M1 第一個真正改變 country / faction 數值的
  PR：`apply_policy_effects` 支援 `country.<field>` /
  `country.budget.<cat>` / `faction:<type>.<field>` 三類 target、
  `add` / `set` 兩種 op，並且在 pre-flight 失敗時保證 state 不變。
  **M1.6（FactionSystem react）** 為 M1 第一個 faction-side 行為：
  兩條 linear-toward-equilibrium reaction rules（loyalty drift toward
  stability at 0.10、support drift toward legitimacy at 0.05），
  明確呼叫，沒有 monthly tick / AI / 事件 / type-specific 行為。
  **M1.7（StabilitySystem tick）** 為 M1 第一個 country-side
  dynamic：依 RFC-080 §5 簡化版公式（`0.5*avg_support +
  0.5*legitimacy - 0.3*corruption - 0.2*avg_radicalism` → target，
  stability drift toward target at 0.10），同樣明確呼叫。
  **M1.8（EconomySystem tick）** 落實 RFC-080 §3 稅收公式
  （`gdp * legal_tax_burden * fiscal_capacity * central_control *
  (1 - corruption)`）與 RFC-080 §4 GDP 成長公式（含 education /
  infrastructure / industry / admin_efficiency 正向項，與 political
  instability / corruption 負向項；exponential compounding），再加
  `budget_balance += (tax_revenue - expenditure)`。明確呼叫，仍
  不接 monthly tick / runner / AI。RFC-080 §4 的 InflationPressure
  / WarDamage 與 RFC-080 §5 的 WelfareSatisfaction / EconomicGrowth /
  InequalityProxy / WarWeariness / BudgetCrisis 仍未實作 —— 待
  monthly pipeline 與後續通膨 / 戰爭 / 福利系統補上。
  **M1.9（MonthlyPipeline）** 為 M1 的第一個 composition
  sub-milestone：將 M1.6 / M1.7 / M1.8 三個明確呼叫 free function
  合成單一 caller。`monthly::tick_country(state, country)` 依固定
  順序執行 `faction::react` → `stability::tick` → `economy::tick`；
  `monthly::tick_all_countries(state)` 依 `state.countries` vector
  順序逐國呼叫並 fail-fast。順序是「可觀察」的（exact-arithmetic
  測試證明任何重新排序會產生不同結果）。對應 RFC-090 §M1 task
  1.15。**M1.9 不做** policy enactment scheduler / active-policy
  container / runner 整合 / log / RNG / save schema 變更 / `last_gdp_growth_rate`
  新欄位 —— 這些保留給 M1.10+。
  **M1.10（Runner monthly wiring）** 把 M1.9 的 pipeline 接到 M0.9
  runner：每次 `TimeSystem.advance_one_day` 回報 `month_changed`，
  runner 在「month rolled over」log 之後呼叫
  `monthly::tick_all_countries(state)`，失敗就立刻 fail。`RunOutcome`
  新增 `monthly_ticks` 計數（並非每國一次，而是每個 month boundary
  一次）。新增公開 `runner::run_state(state, opts)` 給測試手動建
  state 注入。對應 RFC-090 §M1 task 1.16（runner / pipeline 接合）。
  **M1.10 不做** save schema 變更（仍 v5）、country / faction 檔案載入、
  policy scheduler、CSV 新欄位、月度 pipeline log。determinism property
  在空 state 與非空 state 兩種情境下都仍成立。
  **M1.11（Scenario loader）** 為 runner 增加 `--scenario PATH` 旗標
  與 `scenario_loader::load_into_state`，將 M0.7 / M1.1 / M1.2 / M1.4
  parser 組合成 manifest 驅動的載入器。Manifest schema：
  `{ "scenario": { "countries":[…], "factions":[…], "policies":[…] } }`，
  相對路徑相對於 `manifest_path.parent_path().parent_path()` 解析。
  ID 依 vector 順序指派；`faction.country_id_code` 解析為對應的
  `CountryId`。Duplicate id_code、missing country reference、
  pre-populated state 都被 reject。Canonical fixture：
  `data/scenarios/1930_minimal.json`（3 國 + 3 派系 + 10 政策）。
  對應 RFC-090 §M1 task 1.17（scenario manifest / loader）。
  **M1.11 不做** save schema 變更（仍 v5）、policy enactment、
  state 欄位變更、`last_gdp_growth_rate`、RNG、log、partial-load
  rollback、scenario 目錄掃描。
  **M1.12（Economy → Stability coupling）** 在 `CountryState` 新增
  `last_gdp_growth_rate` 欄位：每次 `economy::tick` 在 tick 尾端寫入
  該欄位，下一次 `stability::tick` 把它當作 RFC-080 §5 的
  `EconomicGrowth` 項（`kEconomicGrowthWeight = 2.0`）讀入。
  Monthly pipeline 順序維持 M1.9 canonical（faction → stability →
  economy），所以本月 stability 讀到的是上一月 economy 寫的成長率
  ── 這個一個月 lag 是有意設計，由 monthly_pipeline_test 的
  one-month-lag 與 ordering-regression 兩個測試 pin 住。對應
  RFC-080 §5 EconomicGrowth、RFC-090 §M1 task 1.18（economy ↔
  stability 耦合）。**這是 M1 的第一次 save schema 升版（v5 → v6）**；
  舊 v5 save 與缺少 `last_gdp_growth_rate` 的 v6 save 都被嚴格拒絕。
  **M1.12 不做** policy scheduling、active-policy container、AI、
  事件、戰爭、外交、平衡重調、其他 RFC-080 §5 項（welfare /
  inequality / war weariness / budget crisis）。
  **M1.13（Scenario starting policies）** 為 scenario manifest 新增
  optional `starting_policies` 陣列：每個元素是 `{policy, actor}` 配對的
  id_code，loader 在載入 countries / factions / policies 之後對每個元素
  呼叫一次 `policy::apply_policy_effects`，達成 day-0 政策套用。
  Manifest 沒有 `starting_policies` 仍可載入（保留 M1.11 相容性）。
  Unknown policy / unknown actor 都會 reject 並把 id_code 寫進錯誤訊息。
  新增 fixture `data/scenarios/1930_with_start_policies.json`，在 GER
  上 enact `raise_taxes` + `increase_military_budget`，使 `legal_tax_burden`
  從 0.20 變 0.25、`military_power` 從 0.50 變 0.53。對應 RFC-010 §2.6
  policy fixtures、RFC-090 §M1 task 1.19（scenario 政策初始狀態）。
  **M1.13 不做** save schema 變更（仍 v6）、duration queue / active-
  policy container、monthly policy scheduler、AI、event-triggered
  enactment、平衡重調。Mid-list apply 失敗會留下 partial state（與
  M1.11 文件化的 non-atomic 規則一致）。
  **M1.14（Diagnostics surfaces `last_gdp_growth_rate`）** 為
  `systems::diagnostics` 新增 `CountrySummaryRow` 與
  `country_snapshot(state, country)` 觀察函式，並補上
  `write_country_csv_header` / `write_country_csv_row`；runner 多了
  opt-in 的 `--countries-csv PATH` 旗標，會在每個 snapshot 點
  （start + 每個 `month_changed` + final post-sanity）對每個 country
  各寫一行，欄位為 `date,id_code,gdp,tax_revenue,budget_balance,
  stability,legitimacy,last_gdp_growth_rate`。doubles 用
  `std::scientific` + `setprecision(17)` 以確保 round-trip 精度與 byte-
  identical determinism。原本 `--summary-csv` 的 4 欄格式完全不變，
  保留 M0.10 byte-identical determinism contract。對應 RFC-090 §M1
  task 1.20（per-country diagnostics surface）。**M1.14 不做** save
  schema 變更（仍 v6）、CountryState / FactionState / PolicyData
  shape 變更、faction-level CSV、JSON 變體、streaming I/O、新 sanity
  check、AI / events / war / 平衡重調。
  **M1.15（Policy duration tracking）** 為 `core::entities` 新增
  `ActivePolicy { policy_id_code; expires_on }` 型別，以及
  `CountryState::active_policies` 向量（預設空，只增不減）。每次
  `policy::apply_policy_effects` 成功完成 apply loop 之後，會在 actor
  country 的 `active_policies` 尾端 append 一筆，`expires_on =
  current_date + duration_days`（由 `GameDate::advance_days` 計算，
  DataLoader 已保證 `duration_days >= 0`）。Pre-flight 失敗時
  「不 append」── M1.5 的 atomicity 規則涵蓋這個新 side effect。
  `duration_days == 0` 仍記錄一筆（`expires_on == current_date`），
  原因是 diagnostics 仍想看到 enactment 發生過。`apply_policy_effects`
  並在 actor validation 後加上 runtime 上限
  （`kMaxTrackedPolicyDurationDays = 36500`，~100 年），同時拒絕
  負值 `duration_days`；理由是 `GameDate::advance_days` 是逐日 loop，
  M1.15 開始 `duration_days` 進入 runtime path，否則手寫 `INT_MAX`
  duration 會讓 apply 卡住。DataLoader 不重複這個檢查（避免
  `data_loader -> policy_system` 反向依賴），PolicySystem 是 last
  line of defense。**M1.15 是本 M1
  的第二次 save schema 升版（v6 → v7）**：`country.active_policies`
  是 country object 中必備欄位，v6 save、v7 但 country 缺
  `active_policies`、`active_policies[i]` 缺 `policy_id_code` 或
  `expires_on` 為非法 Gregorian 日期等情況都會 reject 並把
  `countries[N]` / `active_policies[N]` 路徑寫進錯誤訊息。M1.13 的
  scenario day-0 enactment 自動繼承這個 side effect（scenario_loader
  本身沒有 code change）。對應 RFC-010 §2.6 policy lifecycle、
  RFC-090 §M1 task 1.21（policy duration runtime state）。**M1.15
  不做** expiration sweep、effect revert、scheduler / queue、AI 自
  動 enactment、event-triggered enactment、dedup（同一 policy 在
  同一 country 重複 enact 會記兩筆）、新 log line、新 CSV 欄位、
  monthly pipeline 變更、JSON-config / DataLoader shape 變更。
  **M1.16（Faction-level diagnostics CSV）** 為 `systems::diagnostics`
  新增 `FactionSummaryRow` 與 `faction_snapshot(state, faction)`
  觀察函式，並補上 `write_faction_csv_header` /
  `write_faction_csv_row`；runner 多了 opt-in 的 `--factions-csv PATH`
  旗標，會在每個 snapshot 點（start + 每個 `month_changed` + final
  post-sanity）對每個 faction 各寫一行，欄位為
  `date,id_code,country_id_code,type,support,influence,radicalism,
  loyalty,resources` 共 9 欄。`country_id_code` 與 `type` 是
  denormalised 字串欄位，方便外部工具直接以 country 或 faction type
  分組而無需 re-join 其他檔案。doubles 沿用 M1.14 的 `std::scientific`
  + `setprecision(17)` 規則。原本 `--summary-csv` 與 `--countries-csv`
  的 byte format 完全不變，保留 M0.10 與 M1.14 的 byte-identical
  determinism contract。對應 RFC-090 §M1 task 1.22（per-faction
  diagnostics surface）。Drive-by: `main()` 順手補上 per-country /
  per-faction CSV row count 的 stdout 輸出（M1.14 的遺漏）。**M1.16
  不做** save schema 變更（仍 v7）、CountryState / FactionState /
  PolicyData shape 變更、JSON 變體、streaming I/O、新 sanity check、
  per-country 聚合、AI / events / war / monthly pipeline 變更。
  **M1.17（M1 exit / integration tests）** 為 `tests/integration/`
  新增 `m1_end_to_end_test.cpp`，透過 runner 跑完整 M1 pipeline 三
  個 case：(1) 1 年 scenario run（`1930_with_start_policies.json`
  + `--summary-csv` + `--countries-csv` + `--factions-csv`），驗證
  scenario loader 載入 3 國 / 3 派系 / 10 政策、day-0 enactment 在
  GER 留下兩筆 `active_policies`（`raise_taxes` 60 天 →
  `1930-03-02`、`increase_military_budget` 30 天 → `1930-01-31`）、
  monthly_ticks == 12、save round-trip 保留 `active_policies` 與
  `last_gdp_growth_rate`；(2) 10 年 soak run（3652 天 → 1940-01-01，
  120 個 monthly pipeline）對應 RFC-090 §1.17 「跑 10 年單國測試」
  acceptance criterion，驗證 sanity_check 沒有 issue、每國的 gdp /
  stability / legitimacy / last_gdp_growth_rate 都 finite 且 ratio
  欄位仍 clamp 在 `[0, 1]`；(3) 5-artefact byte-identical
  determinism（save / events / summary CSV / countries CSV /
  factions CSV）pin 整個 M1 的決定性 contract。新增
  `docs/milestone-1-result.md` 為 M1 exit report，整理 M1.1–M1.17
  ledger、deferred 項目（expiration sweep、effect revert、scheduler、
  AI / events / war、faction react extension、CSV quoting、多國 /
  外交層、replay）、M2 建議（player-operation prototype per
  RFC-090 §M2）、保留的架構規則。Drive-by：`main()` 過時的
  `Milestone 0.10` label 改成 milestone-neutral 字串。**M1.17 不做**
  save schema 變更（仍 v7）、新 system / flag / CSV、policy
  expiration / revert、AI、events、M2 work。**M1 在此收尾。**
- **M2（已完成）** — 玩家操作原型。M2.22 為 close-out PR，
  整體狀態見 `../docs/milestone-2-result.md`。未來 player-operation
  相關工作（CLI script flag / runner-level rejection surface /
  Delayed / Distorted / scheduler / RNG-based resistance /
  attempted-command log / 擴充 authority 欄位 / authority drift /
  faction reactions / multi-country interaction / weighted
  formulas / 等）都移交給 M3+ 或獨立 post-M2 follow-up，
  M2 本身不再新增 sub-milestone。
- **M4（進行中，RFC-090 §M4 SVG map + UI）** — **M4.2（SVG
  exporter skeleton）** 是 M4 第一個 renderer：新 module
  `leviathan::systems::svg_export` 把 `state.provinces` 轉成
  deterministic SVG document（`viewBox="0 0 1000 1000"`，
  每個 node 一個 `<circle>`，`cx = node.x * 1000`、
  `cy = node.y * 1000`、`r=8`、`fill="black"`，外加
  `data-id` / `data-owner` identity attributes；插入順序保留；
  LF line terminator；`std::fixed` + `setprecision(2)` 確保
  跨平台 byte-stable；空 `state.provinces` 產出 header-only
  `<svg>`）。`end_tick` UNCONDITIONALLY 寫 `provinces.svg`，
  成為 **第 9 個 artefact**（沿用 M3.5 / M3.6 unconditional
  pattern；`milestone-3-result.md` §5 規定加第 9 個 artefact
  需要獨立 sub-milestone 並把 contracts 文件化 ── M4.2 就是
  那個 sub-milestone）。`RunnerOptions::provinces_svg_path`
  optional override（**沒有 CLI flag**），預設
  `<output_dir>/provinces.svg`；`RunOutcome::provinces_svg_path`
  記錄解析後的 path。M2.9 pre-`end_tick` no-artefact contract
  自然延伸；M3.6 mid-`end_tick` non-transactional 警語也
  延伸（仍是 deferred）。M1 / M2 / M3 integration byte-identical
  determinism contracts 從 8 個 artefact 延伸到 9 個。
  Branch 名稱帶 `rfc090-` prefix，避免跟 rolled-back 的
  invented-M4.X 工作混淆。12 個新 doctest cases（8 個
  svg_export + 5 個 runner；共 776）。**M4 in progress.**
  **M4.2 不做** HTML viewer / clickable UI / event handler /
  hover state / map colour / per-country palette /
  ownership dynamics / 鄰接 edge / controller-vs-owner /
  terrain / 文字 label / events / AI / command integration /
  新 `PlayerCommandKind` / runner CLI flag / save schema bump
  （仍 v12）/ 新 state field / 新 gameplay / atomic
  `end_tick`。
  **M4.1（SVG map data skeleton）** 開啟 M4。把 M0 死掉的
  `ProvinceState{id, owner}` stub 換成 typed
  `core::ProvinceNode { id_code, name, owner, x, y }`，
  `x` / `y` 是 normalised `[0, 1]` 地圖座標；
  `GameState::provinces` 變成 typed vector，但目前沒有任何
  system 讀它 ── M4.1 是 **純資料** PR，未來 SVG exporter /
  HTML viewer / clickable map 才會消費。**Save format 從
  v11 bump 到 v12**：`provinces` array 在 save 層 REQUIRED
  （empty 允許），每個 entry 全部 validate（id_code / name
  非空、owner 必須索引到 `state.countries`、x / y finite 在
  `[0, 1]`、duplicate id_code 拒絕）；v11 save 直接拒收。
  Scenario loader 新增 OPTIONAL 的 root-level `provinces`
  array，每個元素是指到 per-file province manifest 的路徑
  （`{ "provinces": [ {id, name, owner, x, y}, ... ] }`）；
  pre-M4.1 manifest 不會壞（缺鍵 = 空 vector）；跨檔案的
  id_code 唯一性也在這層強制。新 canonical fixture
  `data/provinces/1930_core_nodes.json` 三個 nodes
  （berlin / paris / tokyo，分別屬於 GER / FRA / JPN），兩
  個 canonical scenario manifest 都接上。`ScenarioLoadOutcome`
  新增 `provinces_loaded`。`diagnostics::compare_states`
  新增 provinces walk（size + per-field paths）。19 個新
  doctest cases（8 save_system + 8 scenario_loader + 3
  diagnostics；共 764）。**M4 in progress.** **M4.1 不做**
  SVG exporter / HTML viewer / clickable UI / 任何 rendering /
  map colour / ownership dynamics / 鄰接 / terrain / 戰爭 /
  events / AI / command integration / 新 `PlayerCommandKind` /
  runner CLI flag / 新 artefact（仍 8）/ provinces CSV /
  M3 公式變更 / M2 command gate 變更 / 外交 / M5 event engine。
- **M3（已關閉）** — 內政 / 利益團體反應層。**M3.9（M3
  close-out）** 是純文件 PR：新增 `docs/milestone-3-result.md`
  （M3 exit report：M3.1–M3.8 ledger、final dataflow、8-artefact
  contract、save-format v11 floor、future milestone 必須保留的
  architectural invariants、deferred items、以及中立的 next-
  milestone candidates）、在 `docs/milestone-3-checkpoint.md`
  頂部加歷史備註指向 exit report、把三個 README 翻成「M3
  closed / Latest shipped: M3.9 / Next milestone: TBD」。
  **M3 在此關閉。** **M3.9 不做** 新 system / 新 formula / 新
  artefact（仍 8）/ save schema bump（仍 v11）/ 新 state field /
  新 `InterestGroupKind` / 新 fixture / 新 test /
  `PlayerCommandKind` / events / interest group 直接寫 log /
  AI / UI / REPL / CLI / command-gate change / runner CLI
  flag / atomic `end_tick` writes / M4 / post-M3 governance
  follow-up 字樣。Tests 不變（共 745 doctest cases）；M3.7
  integration tests 與 M3.8 canonical-data-row test 已覆蓋
  loop / 8-artefact / canonical-data-row paths。**未來的下一
  個 milestone 等 reviewer 明確指示**：候選為 RFC-090 M4
  SVG map + UI、RFC-090 M5 event engine、或刻意 scope 在
  RFC-090 之外的 post-M3 governance follow-up；M3.9 完全不
  替任何候選背書，也沒有任何「M4 in progress」字樣。
  **M3.8（canonical scenario interest-group fixtures）** 是
  純資料 PR：在
  `data/scenarios/1930_minimal.json` 與
  `data/scenarios/1930_with_start_policies.json` 各加入一個
  Bureaucracy interest group per canonical country（GER / FRA /
  JPN，每個 `influence=0.55, loyalty=0.50, radicalism=0.10`），
  讓 canonical-scenario run 走出 M3.7 之前 M3 三檔 CSV
  header-only 的路徑：31 天 canonical run 現在
  `interest_groups.csv` 有 9 個 data row、
  `interest_group_country_feedback.csv` 有 3 個、
  `interest_group_authority_pressure.csv` 有 3 個。Bureaucracy
  是唯一新增的 kind，因為 M3.4 `authority_pressure` 只看
  Bureaucracy-kind，所以單一 Bureaucracy group per country
  正好讓三條 reverse-direction systems（M3.2 react / M3.3
  country_feedback / M3.4 authority_pressure）全部跑到 data row
  路徑，又不需要引入任何尚未實作的玩法。canonical scenario
  loader test 多 6 個 assert 釘住 3-group 結構；M1.17 / M2.22
  byte-identical determinism contracts 形狀不變（只有「canonical
  scenarios author zero interest groups」的註解需要 refresh）。
  **M3 仍在 in progress** ── 沒有 `docs/milestone-3-result.md`、
  沒有「M3 closed」字樣、沒有 M4。**M3.8 不做** 新 system /
  新 formula / 新 artefact（仍 8）/ save schema bump（仍 v11）/
  loader semantic change / auto-generation / 新
  `InterestGroupKind` / Military 等其他 kind / military_pressure
  等其他 authority channel / events / command-gate diagnostic /
  command-gate formula change / AI / UI / REPL / CLI / 新
  `PlayerCommandKind` / runner CLI flag / atomic `end_tick`。
  1 個新 doctest case + 6 個新 assert 在既有 case 上（共 745）。
  **M3.7（M3 reaction-loop integration checkpoint）** 把
  M3.1–M3.6 的
  closed loop 用 integration tests 釘住，**不新增任何新系統、
  公式、artefact、save schema、CLI flag、玩法**。新檔
  `tests/integration/m3_end_to_end_test.cpp` 三個 case：
  (1) hand-built one-country / one-Bureaucracy-group state
  跑一次 `monthly::tick_all_countries`，assert M3.2 react /
  M3.3 country_feedback / M3.4 authority_pressure 三條 reverse-
  direction counter 全動、四個 mutable field 全變動、兩個
  trace vector 各拿到一行且 post-mutation 值與 live state
  一致；(2) 同一個 hand-built state 走 `runner::run_state` 31
  天（跨一次 month boundary），assert 八個 artefact 全部產出
  且三個 M3 檔有 data row（補上 canonical scenario integration
  測試僅覆蓋 header-only path 的缺口）；(3) 兩個 byte-identical
  hand-built state 兩次跑出 byte-identical 八 artefact，把
  M1.17 / M2.22 determinism contract 延伸到 M3-mutation path。
  新檔 `docs/milestone-3-checkpoint.md` 記錄目前 dataflow、
  八 artefact、未來 sub-milestone 必須保留的 invariants、
  以及刻意延後的 deferred items（events、AI、UI / REPL / CLI
  surface、atomic `end_tick` writes、M3 close-out 等）。
  **M3 仍在 in progress** ── 沒有 `docs/milestone-3-result.md`、
  沒有「M3 closed」字樣、沒有 M4。**M3.7 不做** 新系統 / 新
  公式 / 新 artefact / save schema bump（仍 v11）/ 新 state
  field / 新 `InterestGroupKind` / 新 `PlayerCommandKind` /
  events / interest group 直接寫 log / AI / UI / REPL / CLI
  surface / command gate formula change / command-gate
  diagnostic surface / runner CLI flag / atomic `end_tick`
  writes。3 個新 integration doctest cases（共 744）。
  **M3.6（InterestGroup feedback outcome diagnostics / CSV
  trace surface）** 是 M3.5 state surface 的 outcome trace
  對應，**不新增玩法、不改公式、不改 save schema、不新增 CLI
  flag**。Runner 每次執行都會無條件
  輸出兩個新檔案：`interest_group_country_feedback.csv`（M3.3
  outcome trace）與 `interest_group_authority_pressure.csv`（M3.4
  outcome trace），各 10 欄。Cadence 是「每次真實 mutation 一行」
  ── 被 skip 的 country（沒 matching groups / `weight_sum <= 0`）
  不輸出 row，preflight failure 不輸出 partial rows。新型別
  `interest_group::CountryFeedbackTraceRow` +
  `AuthorityPressureTraceRow`；`country_feedback` /
  `authority_pressure` 新增 optional
  `std::vector<...>* trace_out = nullptr` 參數（default-null =
  byte-identical 與 M3.3 / M3.4 baseline）。`MonthlyOutcome`
  新增兩個 trace vector，`tick_all_countries` 把指標往下傳，
  `TickController` 在 `step_one_day` 把資料 drain 出來。
  Diagnostics 新增 `write_country_feedback_csv_header / _row`
  + `write_authority_pressure_csv_header / _row` 沿用 M3.5
  `csv_escape` + `std::scientific` + `setprecision(17)`。
  `RunnerOptions` 新增兩個 optional path override（**沒有 CLI
  flag**，預設 `<output_dir>/interest_group_*_*.csv`）。
  `RunOutcome` 新增兩個 path + 兩個 row counter；`main()` 列印
  兩個。**No save schema bump（仍 v11）**；M1.17 / M2.22
  byte-identical determinism contract 從 6 artefacts 擴張到 8
  artefacts；M2.9 pre-`end_tick` no-artefact contract 自然涵蓋
  第 7、8 個檔案（`end_tick` 仍是唯一寫磁碟的地方）。對應 RFC-080
  / RFC-020 §5 reaction-loop 想要的可觀察 outcome 第一片落地。
  **M3.6 不做** save schema 變更、formula change、新欄位、
  新 InterestGroupKind、M3.2 `react` per-mutation trace、
  per-tick state delta CSV、events / AI / UI / REPL、
  新 CLI flag、新 `PlayerCommandKind`、command-gate integration、
  atomic `end_tick` 寫檔、`--target-date` 行為變更、M1 / M2
  system 變更。24 個新 doctest cases。
  **M3.5（InterestGroup
  reaction diagnostics / CSV surface）** 是 M3 第一個 observability
  artefact，**不新增玩法、不改 save schema、不新增 CLI flag**。
  Runner 每次執行都會無條件輸出新檔 `interest_groups.csv`，
  cadence 與既有 CSV 相同（start + 每個 `month_changed` + final
  post-sanity）；canonical scenario 沒 author interest groups 就
  只輸出 header line。固定 9 欄：
  `date,id_code,name,kind,country_id,country_id_code,influence,
  loyalty,radicalism`。`state.interest_groups` vector 順序原樣
  保留（不排序）。新 `diagnostics::InterestGroupSummaryRow` +
  `interest_group_snapshot` + `write_interest_group_csv_header`
  / `write_interest_group_csv_row` + 小型 `csv_escape` (RFC 4180)
  helper（`name` 含 `,` / `"` / `\n` / `\r` 時自動 quote/double-
  quote）；doubles 沿用 M1.14 的 `std::scientific` +
  `setprecision(17)`。Invalid `group.country` 在 snapshot 時就
  loud-fail（不會偷偷輸出空 `country_id_code`）。`RunnerOptions`
  新增 `interest_groups_csv_path`（optional path override，預設
  `<output_dir>/interest_groups.csv`），`RunOutcome` 新增
  `interest_groups_csv_path` + `interest_groups_csv_rows`，
  `TickController` 新增 `interest_group_rows` buffer。Drive-by：
  把 `InterestGroupKind` ↔ string mapping（原本在
  `save_system.cpp` + `scenario_loader.cpp` 各複製一份）抽到
  共用 `core/interest_group_kind.{hpp,cpp}`，save / scenario /
  diagnostics 三個 consumer 都走同一個 source of truth；以後加
  新 variant 只改一個 switch。**No save schema bump（仍 v11）**；
  M1.17 / M2.22 byte-identical determinism contract 從 5
  artefacts 擴張到 6 artefacts；M2.9 pre-`end_tick`
  no-artefact contract 自然涵蓋第 6 個檔（`end_tick` 仍是唯一
  寫磁碟的地方）。對應 RFC-020 §5 / RFC-050 「想看到的內政
  狀態」第一片可觀察落地。**M3.5 不做** save schema 變更、新
  state 欄位、新 InterestGroupKind、formula 變更、formula-trace
  CSV、per-tick delta CSV、event triggers、AI / UI / REPL、
  新 CLI flag、新 `PlayerCommandKind`、command-gate integration、
  weighted aggregate diagnostics、atomic `end_tick` 寫檔、
  `--target-date` 行為變更、M1 / M2 system 變更。24 個新 doctest
  cases。
  **M3.4（InterestGroup-
  derived authority pressure skeleton）** 開啟 M3 反應 loop 的
  第二個反向通道：interest groups 不只推 `country.stability`
  （M3.3），也推 `country.government_authority.bureaucratic_compliance`。
  延續 M3.2 / M3.3 的 `leviathan::systems::interest_group` 模組，
  新增常數 `kInterestGroupAuthorityPressureRate = 0.01`、
  `AuthorityPressureOutcome { int countries_updated }`、與
  `authority_pressure(state)` free function。對每個 country：
  在 `group.country == ci` 且 `g.kind == Bureaucracy` 且
  `influence > 0` 的 groups 上計算 influence-weighted loyalty
  `sum(g.influence * g.loyalty) / sum(g.influence)`，然後 drift
  `country.government_authority.bureaucratic_compliance` 朝該
  target 以 0.01 速率、clamp `[0, 1]`。沒有 Bureaucracy 群組
  或全部 zero-influence 就 skip（不 mutate、不計數、不算失敗）。
  輸出 surface **只動** `bureaucratic_compliance`：其他三個
  authority 子欄位（`military_loyalty` / `intelligence_capability`
  / `media_control`）以及 country `stability` / `legitimacy` /
  `corruption` / `central_control` / `administrative_efficiency`
  全不動。輸入 aggregate **只用** Bureaucracy-kind 的 influence-
  weighted loyalty，不用 radicalism；其他 kind（Military /
  Workers / 等）即使存在也忽略。嚴格 preflight：每個 group
  的 `country` / `influence` / `loyalty`、以及每個 country 的
  `bureaucratic_compliance` 都驗 finite + `[0, 1]`，任何一個
  違規整批不 mutate（避免 NaN 擴散）；`radicalism` 與
  `stability` 因為 M3.4 不讀，故意不重複 preflight。接進
  monthly pipeline 作為 `tick_all_countries` 的**第三個 global
  step**，在 M3.3 `country_feedback` 之後，完成 rate ladder
  (`react` 0.05 → `country_feedback` 0.02 →
  `authority_pressure` 0.01)；越外層 rate 越慢以維持 closed
  loop 阻尼。`MonthlyOutcome` 新增
  `int interest_group_authority_countries_updated`。M2.18
  `EnactPolicy` 阻力閘從此成為 loop 的下游消費者，但 M3.4
  **不**修改閘的公式。**No save schema bump（仍 v11）**；
  canonical scenario 沒 author interest_groups，所以既有
  M1/M2/M3.1/M3.2/M3.3 caller 完全沒有行為變更。對應
  RFC-020 §3 國家掌控力長期想要的 interest-group 反饋的
  第一片落地。**M3.4 不做** save schema 變更、新欄位、新
  InterestGroupKind、`military_loyalty` / `intelligence_capability`
  / `media_control` 的 mutation、`legitimacy` / `corruption` /
  `stability` / `central_control` / `administrative_efficiency`
  的 mutation、第二個 aggregate input（radicalism 不參與本步）、
  per-kind / per-country / per-output rate、weighted multi-input
  formula 超出 influence-weighted Bureaucracy loyalty、RNG /
  機率行為、events / `state.logs` entry、AI / UI / CLI、
  coup / strike / protest / civil war / cross-border、automatic
  group generation、command-gate integration、新 PlayerCommandKind、
  faction reaction 變更、policy preference system、`tick_country`
  變更、M1 / M2 system 變更。
  **M3.3（InterestGroup
  country feedback skeleton）** 把 M3 的反應 loop 收尾：interest
  groups 反推 country state。延續 M3.2 的
  `leviathan::systems::interest_group` 模組，新增常數
  `kInterestGroupCountryFeedbackRate = 0.02`、
  `CountryFeedbackOutcome { int countries_updated }`、與
  `country_feedback(state)` free function。對每個 country：
  計算 influence-weighted radicalism
  `sum(g.influence * g.radicalism) / sum(g.influence)`，只算
  `group.country == ci` 且 `influence > 0` 的 groups；然後
  drift `country.stability` 朝 `1.0 - weighted_radicalism`
  以 0.02 速率、clamp `[0, 1]`。沒有 matching groups 或全部
  zero-influence 就 skip（不 mutate、不計數、不算失敗）。
  輸出 surface **只動** `country.stability`：`legitimacy` /
  `government_authority` / `corruption` / `central_control` /
  `administrative_efficiency` 全不動。輸入 aggregate **只用**
  influence-weighted radicalism，不用 loyalty。嚴格 preflight：
  每個 `group.country` / `influence` / `radicalism` /
  `country.stability` 都驗 finite + `[0, 1]`，任何一個違規
  整批不 mutate（避免 NaN 擴散）。接進 monthly pipeline 作為
  `tick_all_countries` 的**最後一步**、M3.2 `react` 之後，讀的是
  剛 drift 完的 radicalism。`MonthlyOutcome` 新增
  `int interest_group_countries_updated`。較慢的 0.02 速率
  抑制 closed loop 震盪。**No save schema bump（仍 v11）**；
  canonical scenario 沒 author interest_groups，所以既有
  M1/M2/M3.1/M3.2 caller 完全沒有行為變更。對應 RFC-080 §5
  stability 公式長期想要的 interest-group 反饋的第一片落地。
  **M3.3 不做** save schema 變更、新欄位、新 InterestGroupKind、
  `legitimacy` / `government_authority` / `corruption` mutation、
  第二個 aggregate input、per-kind / per-country / per-output rate、
  weighted multi-input formula 超出 influence-weighted radicalism、
  RNG / 機率行為、events / `state.logs` entry、AI / UI / CLI、
  coup / strike / protest / civil war / cross-border、automatic
  group generation、command-gate integration、新 PlayerCommandKind、
  faction reaction 變更、policy preference system、`tick_country`
  變更、M1 / M2 system 變更。
  **M3.2（InterestGroupReactionSystem
  skeleton）** 是 M3 第一個讓 interest_groups 真正 *mutate* 的
  system。新模組 `leviathan::systems::interest_group` 含常數
  `kInterestGroupReactionRate = 0.05`、`ReactionOutcome { int
  groups_updated }` 與 `react(state)` free function。Reaction 是
  linear-toward-equilibrium drift，driver 是 `country.stability`
  單一輸入：`loyalty += (country.stability - loyalty) * 0.05`、
  `radicalism += ((1 - country.stability) - radicalism) * 0.05`，
  兩者都 clamp 到 `[0, 1]`。`influence` / `kind` / `country` /
  `id_code` / `name` 完全不動。`react` 是 pure（不寫 logs、不動
  RNG、不推進日期、不動 country/faction/policy 狀態、不 emit
  events），preflight 驗證每個 `group.country` index 進
  `state.countries`，**任何 entry 無效就整批不 mutate**（atomicity
  across the list）。接進 monthly pipeline，作為
  `tick_all_countries` 的**最後一步**在所有 per-country
  faction/stability/economy tick 完成後才跑一次，讀的是 post-tick
  stability。`MonthlyOutcome` 新增 `int interest_groups_updated`。
  **No save schema bump（仍 v11）**；既有 M1/M2 沒 author
  interest_groups 的 caller 完全沒有行為變更。對應 RFC-020 §5
  長期 faction / interest-group 反應機制的第一片落地。**M3.2 不做**
  influence drift、per-kind formula、weighted formula、第二個
  driver（corruption / authority / legitimacy）、country aggregate
  effect（interest groups 不回推 country state，留給 M3.3+）、
  events / state.logs entry / AI / UI / CLI / scheduler、
  command-gate integration、新 PlayerCommandKind、新 CSV 欄位、
  RNG / 機率行為、strikes / protests / coups / civil war /
  cross-border 行為、M1 / M2 system 變更。
  **M3.1（InterestGroupState
  / political actors skeleton）** 為 M3 開頭，先建立 political
  actor 的資料層：新 `core::InterestGroupKind` enum（10 個 variant：
  Bureaucracy / Military / Workers / Farmers / Religious / Media /
  Students / LocalElites / Business / Technocrats）+ 新
  `core::InterestGroupState` POD（id_code / name / kind / country
  CountryId / influence / loyalty 預設 0.5 / radicalism 預設 0.0）
  + 新 `GameState::interest_groups` root-level vector（每個 entry
  的 `country` 欄位指回所屬 country；root-level 設計為未來跨國
  interaction 預留空間）。**Save format bumped v10 → v11**：save
  層 block 必備（empty array 可），每個 entry 嚴格驗證
  （non-empty id_code + name / known kind 字串 / country index
  resolve 進 `state.countries` / 三個 ratio 在 [0, 1] 內，重複
  id_code 拒絕）。`scenario_loader` 對 scenario JSON 中的
  `interest_groups` block 採 optional 策略，missing → empty
  vector；present-but-malformed 仍嚴格驗證並回報
  `interest_groups[N]` 路徑與欄位名。`diagnostics::compare_states`
  擴張到走訪 array 在 `interest_groups[N].*` 路徑下。**Data only**
  ──完全沒有 M1 / M2 system 讀或寫這些欄位，M1 monthly pipeline
  與 M2 command path 行為 byte-identical。對應 RFC-020 §5
  長期 faction / interest-group 清單第一片落地，為 M3.2+ 的
  reaction / command resistance / event triggers / AI 預留資料 seam。
  **M3.1 不做** monthly reaction、command-resistance integration、
  authority drift、demand / preferred policy / armed strength /
  ideology / foreign links 欄位、automatic generation、coup /
  strike / protest、新 CLI flag、新 `PlayerCommandKind`、新 CSV
  欄位、新 `state.logs` 條目、replay primitive 變更、UI / AI /
  events / scheduler、M1 / M2 system 變更。
  **M2.1（Player country
  selection）** 為 `core::GameState` 新增 `player_country` 欄位
  (`CountryId`，預設 `CountryId::invalid()` = -1)，並在 runner
  新增 `--player COUNTRY_IDCODE` 旗標。Resolution 在
  `run_state` 載入 scenario 後立即執行：linear scan `state.countries`
  尋找 `id_code` match，找不到 / state.countries 為空時 fail 並
  把 id_code 寫進錯誤訊息，整個 fail 動作發生在任何 log / snapshot
  emit 之前。**M2.1 是 M2 第一次 save schema 升版（v7 → v8）**：
  `player_country` 是 root-level 必備整數欄位（-1 為 headless
  sentinel；非負必須 index 進 `countries`），v7 save、missing 欄位、
  non-integer、`< -1`、out-of-range、超出 `INT_MAX` 都會 reject 並
  把實際值寫進錯誤訊息。**M2.1 不做** 任何 system 行為變更（M1 的
  `faction::react` / `stability::tick` / `economy::tick` / monthly
  pipeline / diagnostics 都沒有讀 `player_country`），所以 M1.17
  的 5-artefact byte-identical determinism contract 仍成立。對應
  RFC-090 §M2 task 2.1。**M2.1 不做** pause / resume / step
  控制（M2.2 的事）、player command queue（M2.3）、player command
  log（M2.4）、UI / map、AI / events、multi-player、scenario
  manifest 帶 default player。
  **M2.2（Pause / resume / step primitives）** 把 `run_state` 內的
  day-at-a-time loop 抽出三個 public free function：`begin_tick` /
  `step_one_day` / `end_tick`，搭配新的 `runner::TickController`
  runtime struct（**不**進 `GameState`，不存進 save）。`begin_tick`
  做 `--player` 解析 + 抓 `start_date` + 寫 simulation start log +
  initial snapshot row；`step_one_day` 推進一天，處理 month / year
  boundary log + M1.10 monthly pipeline 呼叫 + per-month snapshot；
  `end_tick` 寫 simulation end log + `sanity_check` + final snapshot
  + 解析 output path + 寫 save / JSONL / CSV，並回傳 `RunOutcome`。
  `run_state` 重寫為三者組合，**M1.17 的 5-artefact byte-identical
  determinism contract 仍成立**（由兩個 equivalence test pin 住：
  `begin/step×N/end == run_state(days=N)`、`begin/step×15/step×16/
  end == run_state(days=31)`）。Misuse path（double begin、step
  before begin、step after end、double end）都返回 specific 錯誤
  訊息。Drive-by：把 PR #29 提到的 bad-`--player` nit 補上兩個
  regression test，驗證錯誤路徑不會留下 `save.json` /
  `events.jsonl` 在 disk。對應 RFC-090 §M2 task 2.2。**M2.2 不做**
  save schema 變更（仍 v8）、新 CLI flag（沒有 `--step` mode）、
  新 log、M1 system 行為變更、command queue（M2.3）、command log
  （M2.4）、UI / map、AI / events、mid-run save/load resume。
  **M2.3（Player command queue）** 為 `core/` 新增
  `core/player_commands.hpp`（含 `PlayerCommandKind` enum 與
  `PlayerCommand{kind, policy_id_code}` POD，本期只支援
  `EnactPolicy`），並新增 `systems/commands.{hpp,cpp}` 模組：
  `CommandQueue` runtime container、`ApplyOutcome` outcome，以及
  `apply_pending(state, queue)` free function。Queue 由 outer
  driver 持有，**不**進 `GameState`、**不**進 `runner::TickController`，
  所以 M2.3 沒有 save schema 變更。`apply_pending` 預先驗證
  `state.player_country` 是 `state.countries` 的合法 index，
  然後 in-order 處理 queue：每個 `EnactPolicy` dispatch 進
  `policy::apply_policy_effects`，沿用 M1.5 per-command atomicity、
  M1.15 active_policies tracking、M1.15 duration cap。**Cross-list
  non-atomic**：首次失敗即停下，已成功的 commands 已 apply 並
  從 queue pop 掉，失敗的 command 留在 head 等候 retry，後面
  剩餘 commands 仍在 queue（與 M1.13 scenario starting_policies
  的 mid-list-failure 規則一致）。對應 RFC-090 §M2 task 2.3 的
  「first-class command queue」精神（RFC 文字較傾向把 task 2.4 預算
  命令、2.5 政策命令、2.6 命令 log 分開列；我們把 queue 基礎設施
  + 第一個 kind 合併為 M2.3，把 log 留給 M2.4）。**M2.3 不做**
  save schema 變更（仍 v8）、新 CLI flag、`apply_pending` 自動
  插進 `step_one_day` 內、其他 `PlayerCommandKind` variants
  （`AdjustBudget` / `ChangeTaxBurden` 等留給 M2.5+）、command
  log（M2.4）、queue 跨 save/load 持久化、multi-player command
  interleaving、新 log line、M1 system 行為變更。
  **M2.4（Player command log）** 為 `core/player_commands.hpp` 新增
  `AppliedPlayerCommand{applied_on, command}` 型別，並在 `GameState`
  新增 `applied_commands` 向量（預設空）。`systems::commands::apply_pending`
  在每個 per-command dispatch 成功後（M1.5 / M1.15 mutation 已 land
  之後）append 一筆 log entry──per-command atomicity 涵蓋這個新
  side effect：失敗 command 停在 queue head，不會 log。`applied_on`
  抓的是 `state.current_date` 在 apply 那一刻的值（不是 submit 時
  的日期），由一個跨日 test pin 住。**M2.4 是 M2 第二次 save schema
  升版（v8 → v9）**：`applied_commands` 是 root-level 必備陣列，
  每個 entry 為 `{applied_on, command: {kind, policy_id_code}}`；
  v8 save、missing `applied_commands`、entry 不是 object、`applied_on`
  非法 Gregorian 日期、`kind` 為未知字串、`command` 缺 `policy_id_code`、
  entry 缺 `command` sub-object 等情況全部 reject 並把
  `applied_commands[N]` 路徑寫進錯誤訊息。對應 RFC-090 §M2 task 2.6
  「命令 log」與 RFC-050 §8「玩家命令需記錄」。這是 deterministic
  replay 的基礎；replay 本身不在 M2.4 範圍。**M2.4 不做** replay
  實作、log compaction / truncation、為失敗 command 寫 log 條目、
  新的 `PlayerCommandKind` variants（仍只支援 `EnactPolicy`，留給
  M2.5）、新 CLI flag、新 `state.logs` 條目、queue 自身的持久化
  （`CommandQueue` 仍 runtime-only）、`apply_pending` 自動進
  `step_one_day` 內、M1 system 行為變更。
  **M2.5（AdjustBudget player command）** 延伸 `PlayerCommandKind`
  增加 `AdjustBudget` variant，並在 `PlayerCommand` POD 加上兩個
  新 payload 欄位（`budget_category` string + `budget_delta`
  double）。`systems::commands::apply_pending` 增加一個 switch arm：
  驗證 `budget_category` 是 7 個 `BudgetState` 欄位之一、
  `budget_delta` 為 finite，再 apply `budget.<category> += delta`
  並 clamp 到 `[0, 1]`（沿用 M1.5 的 ratio-field clamp 規則）。
  M2.3 per-command atomicity 與 M2.4 log-on-success 完整繼承──失敗的
  AdjustBudget 不會 log、不會 mutate state；成功的 log entry 帶完整
  payload。`save_system` 的 kind ↔ string mapping 擴充支援
  `"AdjustBudget"`，per-kind JSON shape 只 emit 對應 payload
  欄位：`EnactPolicy` 帶 `policy_id_code`、`AdjustBudget` 帶
  `budget_category` + `budget_delta`。對應 RFC-090 §M2 task 2.4
  「預算調整命令」。**M2.5 不做** save schema 變更（仍 v9──array
  shape 沒變，只是合法 kind-string 集合擴大；舊 binary 還是會在
  unknown kind 處 reject，已經是嚴格 gate），新 CLI flag、新
  command kind（`ChangeTaxBurden` 等留給 M2.7）、deterministic
  replay（M2.6）、UI、AI、budget 自動 sum-to-1、新 log line、
  M1 system 行為變更。Drive-by：PR #32 reviewer nit──
  `player_command_kind_to_string` 的 fallback 從返回 `"EnactPolicy"`
  改成 `"UnknownPlayerCommandKind"` sentinel，避免未來新增 kind
  卻忘了更新 mapping 時靜默 corrupt save。
  **M2.6（Replay applied command log prototype）** 為
  `systems::commands` 新增 `replay(state, log)` free function 與
  `ReplayOutcome` struct，作為 M2.4 log 的第一個消費者。對每個
  log entry：強制設定 `target_state.current_date = entry.applied_on`、
  建立 1-element `CommandQueue`、呼叫 `apply_pending`──完全沿用
  M2.3 dispatch + M2.4 log-append + M1.5/M1.15 effect machinery，
  沒有複製 logic。**Preconditions**：`target_state.player_country`
  合法 + `target_state.applied_commands` 必須是空（否則 replay log
  會和既有 entries 混在一起，破壞「log mirrors source」保證；caller
  應該對 freshly-loaded scenario 而非 reloaded save 呼叫 replay）。
  **跨 log atomicity** 沿用 M2.3 mid-list-failure pattern：失敗 entry
  以 `replay[N]: ...` 標出，先前 entries 已 apply + log，後續 entries
  跳過。**Prototype 限制**（測試 pin 住）：commands 之間不跑 time
  system（不呼叫 advance_one_day / monthly pipeline）、最終
  `current_date` 停在最後一筆 entry 的 `applied_on` 而非 source 的
  實際 final date、scenario 由 caller 預先載入。對應 RFC-090 §M2
  task 2.8「玩家操作回放 log」的消費端（記錄端在 M2.4 已實作）。
  **M2.6 不做** save schema 變更（仍 v9）、新 CLI flag、新 log line、
  monthly pipeline tick during replay、divergence detection、scenario
  reload、命令 outcome capture（M2.4 deferred nit）、M1 system 行為
  變更。未來 M2.8 候選會把 M2.2 `step_one_day` 整合進來解掉
  prototype 的時間限制。
  **M2.7（Replay with time-system advancement）** 新增
  `systems::commands::replay_with_time(state, opts, ctrl, log)`
  free function，補上 M2.6 prototype 的「不跑 time-system」限制。
  演算法：每個 log entry 先檢查 `entry.applied_on >= current_date`
  (PR #34 reviewer nit 提到的 monotonicity check)，然後 while
  `current_date < entry.applied_on` 呼叫 M2.2 `step_one_day`──
  M1.10 monthly pipeline 因此會在進入 month boundary 時自然觸發──
  最後在日期相等時以 1-element CommandQueue + apply_pending 套用
  指令。Preconditions 在 M2.6 那兩條之上多了 `ctrl.started &&
  !ctrl.ended`（caller 必須先呼叫 `runner::begin_tick`）。Cross-
  log atomicity 沿用 M2.3 mid-list-failure pattern；但因為日期推進
  發生在 apply 之前，失敗時 `state.current_date` 可能停在中途
  日期（M2.6 已被 reviewer 標為文件化但非 transactional rollback
  的限制，M2.7 繼承這個 caveat）。**Killer equivalence test pin
  住行為保持**：source 透過 step + apply 交替推進產生 log，target
  以 replay_with_time 重放，target.current_date / days_stepped /
  monthly_ticks / applied_commands 全部與 source 相等，並且 command
  效果欄位（legal_tax_burden、budget.military）+ monthly pipeline
  影響的欄位（gdp、stability、last_gdp_growth_rate）也都
  byte-identical。M2.6 `replay` 並沒有被取代，兩者共存（M2.6 給
  time-stripped 測試用、M2.7 給 timeline 測試用）。`commands.hpp`
  新增 `runner.hpp` include，但依賴方向仍是 acyclic（runner 不
  include commands）。對應 RFC-090 §M2 task 2.8「玩家操作回放
  log」消費端 + RFC-000 §5 rule 10「deterministic replay」的奠基。
  **M2.7 不做** save schema 變更（仍 v9）、新 CLI flag（M2.8
  候選）、UI、AI、event 整合、divergence report API、transactional
  rollback on mid-list failure、target.current_date 自動延伸到
  source 的最終日期（caller 想要時可以再 step_one_day）、新
  state.logs lifecycle 條目、M1 system 變更。
  **M2.8（Replay CLI harness）** 把 M2.7 `replay_with_time` 接進
  runner CLI：新增 `--replay PATH` 旗標、`RunnerOptions::replay_path`
  欄位、`RunOutcome::replay_commands_replayed` 計數。`run()` 在
  scenario load 之後分支：若 `--replay` 設定，必須同時設定
  `--scenario`（fresh state baseline），載入 save 後若 `--player`
  未設定就從 loaded save 繼承 `player_country`，再執行
  `begin_tick → replay_with_time(loaded.applied_commands) →
  end_tick`，最後把 commands_replayed 計數寫進 outcome。`main()`
  在 stdout summary 加上 `Replay source` + `Commands replayed` 兩行。
  **CLI 不自動比對** replayed state 與 source；使用者自己對 save
  檔做 `diff`──這保持 harness 範圍極小且不過度規範「match」語意。
  對應 RFC-090 §M2 task 2.8「玩家操作回放 log」的使用者介面層。
  **M2.8 不做** save schema 變更（仍 v9）、per-field state-
  comparison API（M2.10 候選）、`--target-date` 旗標、replay 跨
  scenario、multi-save replay chain、新 state.logs lifecycle 條目、
  runner 外的 replay entry point（library primitive `replay_with_time`
  仍可獨立使用）、M1 system 變更。
  **M2.10（State comparison API）** 為 `systems::diagnostics` 新增
  `compare_states(a, b, opts)` free function 與 `StateMismatch` /
  `CompareOptions` 兩個資料型別。Walks 兩個 `GameState`，依固定欄位
  順序逐欄比對，回傳 mismatch list（空 list = match）。
  **Compared**：`current_date`、`player_country`、每個 country 的
  identity 字串 + 13 個 numeric（含 `last_gdp_growth_rate`）+ 7 個
  budget 類別 + `active_policies` 條目；每個 faction 的 identity
  字串 + 5 個 numeric + preferred_policies 計數；每個
  `applied_commands` 條目（applied_on + kind + payload）。
  **Deliberately skipped**（每個都在設計筆記中有 rationale）：
  rng（M2 replay 尚未涉及 RNG）、logs（begin/end_tick boilerplate
  幾乎一定不同）、policies（不會 mutate 的 templates）、provinces /
  events（仍 reserved-empty）、simulation_config（不在 GameState
  裡）。Float-point 比對採絕對 tolerance，預設 `1e-9`（對齊 M0.8
  save round-trip 精度），可由 `CompareOptions` 覆寫。`field_path`
  與 M0.8 / M2.4 save JSON 的定址習慣一致（`countries[0].budget.military`），
  讓同一字串能直接用在 CLI 輸出、測試 assert、錯誤訊息。對應
  RFC-090 §M2「玩家操作回放」family 的程序化等價檢查需求；
  expected consumers 是 replay-equivalence integration tests 與
  未來 M2.11 的 `--verify` CLI flag。**M2.10 不做** save schema
  變更（仍 v9）、CLI 整合（M2.11 候選）、相對 tolerance、log /
  rng / policy 比對、mismatch budget cap、新 state.logs 條目、
  M1 system 變更。
  **M2.11（Replay verify CLI）** 在 runner 新增 `--verify` boolean
  flag（須與 `--replay` 並用），把 M2.10 `compare_states` 接進
  M2.8 replay 流程。`end_tick` 成功後，runner 對「replayed state」
  與「已載入的 source save」呼叫 `compare_states`，把結果寫進新的
  `RunOutcome::verify_mismatches`（vector of `StateMismatch`）。
  `main()` 在 stdout summary 加上 `Verify mismatches: N` 並對每個
  mismatch 印一行 `  - <field_path> : <detail>`。**Informational
  only** — 不論 mismatch 數量，run 都以 exit code 0 結束；artefacts
  （save / JSONL / CSV）仍會寫到 disk 供使用者鑑識。`parse_args`
  在 `--verify` 沒有 `--replay` 時直接 reject 並把兩個 flag 名稱
  寫進錯誤訊息。Source save 重用 M2.8 已經載入的物件（沒有額外
  disk I/O）。對應 RFC-090 §M2 與 RFC-050 §8 的「玩家命令需記錄」
  消費端的最後一塊：自動化等價檢查的 CLI 入口。**M2.11 不做**
  save schema 變更（仍 v9）、strict fail-on-mismatch 模式
  （`--verify-strict` 留給 M2.13 候選）、CLI tolerance 旋鈕、
  `--verify` 在 `--replay` 之外使用、mismatch list 截斷、新
  state.logs 條目、M1 system 行為變更。
  **M2.12（Replay strict mode）** 在 runner 新增 `--verify-strict`
  boolean flag（須與 `--verify` 並用），把 M2.11 的「informational」
  verify 升級為「CI-gateable」。`main()` 在印完所有 mismatch
  bullets 之後，若 `opts.verify_strict && !verify_mismatches.empty()`
  就回傳 `EXIT_FAILURE`。**架構決策**：`run()` 語意不變——
  simulation + replay 完成時 run() 仍回傳 success；strict 是
  `main()`-level 的 exit-code 政策。tradeoff 是 main() 多一行
  policy logic；benefit 是 library / CLI 分離乾淨，其他 consumer
  （tests / 未來 embedders）可以套用自己的政策。`parse_args` 在
  `--verify-strict` 缺 `--verify` 時直接 reject 並把兩個 flag 名稱
  寫進錯誤訊息。Flag-chain：`--verify-strict → --verify →
  --replay`。對應 RFC-090 §M2 replay family 的最後一塊：自動化
  build / replay 回歸 gate。**M2.12 不做** save schema 變更（仍
  v9）、`--verify-tolerance` CLI knob（M2.13 候選）、structured
  diff 輸出、mismatch 數量門檻（strict 是二元，任何 mismatch 都
  失敗）、`run()` 行為變更、新 state.logs 條目、M1 system 變更。
  **M2.13（Verify tolerance CLI）** 在 runner 新增
  `--verify-tolerance FLOAT` 旗標（須與 `--verify` 並用），讓使用者
  覆寫 M2.10 預設的 `1e-9` `CompareOptions::double_tolerance`。
  新的 `parse_nonneg_double` helper 採 exception-free 寫法
  （`std::strtod` + full-consumption + finiteness check），拒絕
  empty / 含 trailing garbage（如 `"1.5x"`）/ NaN / Inf / 負值，
  每種錯誤都把 flag 名稱與不合法值寫進訊息。`run()` 在 replay
  分支建立 `diagnostics::CompareOptions`，若 `opts.verify_tolerance`
  有值就覆寫 `double_tolerance` 再傳給 `compare_states`。`main()`
  在 verify block 多印一行 `Verify tolerance : <value>`，方便 CI
  log 看出當次比對使用的容忍度。**這也讓 M2 replay-CLI family
  進入功能完整狀態**：`--replay` (M2.8) / `--verify` (M2.11) /
  `--verify-strict` (M2.12) / `--verify-tolerance` (M2.13)。
  對應 RFC-090 §M2 replay family 的 CLI ergonomics 收尾。
  **M2.13 不做** save schema 變更（仍 v9）、library 行為變更
  （只是把使用者輸入轉手傳進去）、relative tolerance、per-field
  tolerance、新 gameplay、新 state.logs 條目、M1 system 變更。
  **M2.22（M2 exit / integration tests）** 為 M2 close-out PR：
  新 `tests/integration/m2_end_to_end_test.cpp` 提供 3 個
  end-to-end 測試 pin 住 M2 player-operation surface 與 M3+
  的接縫——
  (1) command script + replay + verify equivalence：`apply_command_script`
      在 source 跑兩條命令、`replay_with_time` 在 target replay 同一份
      log、`diagnostics::compare_states` 確認所有 gameplay 欄位 0
      mismatch；
  (2) order-execution gate atomicity across kinds：低
      bureaucratic_compliance + 高 military_loyalty 下，mixed
      script 中 AdjustBudget(military) 通過、EnactPolicy 被擋、
      trailing AdjustBudget(welfare) 未跑，state / queue /
      applied_commands 全部和 per-command atomicity 一致；
  (3) 5-artefact byte-identical determinism with M2 commands：
      M1.17 5-artefact determinism contract 延伸到 31 天運行 +
      day-0 `apply_command_script`。
  新 `../docs/milestone-2-result.md` exit report 整理 M2.1–
  M2.21 ledger、列出 deferred items（CLI script flag / runner-
  level rejection surface / Delayed / Distorted / scheduler /
  RNG-based resistance / attempted-command log / 擴充 authority
  欄位 / authority drift / faction reactions / multi-country /
  weighted formulas / atomic end_tick writes / relative
  tolerance）、給 M3+ 的推薦項目，以及 M2 加入 M1 之上的架構
  invariants。三個 README + memory 都改成「M2 closed」。**M2 在此
  收尾**。**M2.22 不做** 新 gameplay、save format 變更（仍
  v10）、新 CLI flag、runner / main / replay primitive 變更、新
  PlayerCommandKind、新 CSV 欄位、threshold / formula 調整、
  scheduler / RNG / Delayed / Distorted、AI / events / UI、
  attempted-command log、DataLoader / M1 system 變更。
  **M2.21（Command script driver helper）** 在 M2.20 `try_apply_pending`
  surface 之上新增 library-only convenience function
  `commands::apply_command_script(state, vector<PlayerCommand>)`。
  helper 接受一次性 command script（`std::vector<core::PlayerCommand>`），
  內部建立 local `CommandQueue`、把 script 複製進去、呼叫
  `try_apply_pending`。Outcome shape 直接重用 M2.20 的
  `ApplyWithReportOutcome`，沒有新增 parallel struct。Routing
  完全繼承自 `try_apply_pending`：empty script ⇒ success +
  `rejection == nullopt`；full drain ⇒ success +
  `commands_applied == script.size()`；order-execution gate
  rejection ⇒ success + RejectionRecord；非 execution 錯誤
  （precondition / NaN delta / unknown policy / unknown category）
  ⇒ `Result::failure`。Input vector 完全不 mutate（helper copy
  進 local queue 才 drain）。實作只有三行：建 local queue、copy
  script、call try_apply_pending。**完全沒有 runner / RunOutcome
  / main.cpp / CLI / replay / save schema 變更**——刻意維持
  library-only，runner-level entry point 留給未來 PR（M2.22 候選）
  在實際有 script input 需求時再做。Helper 主要目的：給未來
  REPL / scripted test / agent driver / UI 一個乾淨入口，不要在
  每個 call site 重複 build-queue-then-drain 樣板。對應 RFC-020 §2
  命令阻力 surface 的「程式化讀取入口」需求。**M2.21 不做** runner
  / RunOutcome / main.cpp 變更、`--script PATH` CLI flag、
  remaining-queue surface（rejected 後的 trailing commands 不
  surface）、persistent attempted command log、`state.logs` 條目、
  save format 變更（仍 v10）、DataLoader / policy effect / replay
  primitive 變更、新 `PlayerCommandKind`、新 CSV 欄位、threshold
  / formula 調整、M1 system 變更、AI / events / scheduler。
  **M2.20（Command rejection reporting）** 把 M2.18 / M2.19 的
  order-execution rejection 做成 **可程式化讀取的結構化結果**，
  完全不動 `apply_pending` 既有 semantics。新增 POD
  `commands::RejectionRecord { kind, policy_id_code,
  budget_category, compliance, threshold, resistance }`；新增
  wrapper `commands::ApplyWithReportOutcome { apply, rejection }`；
  新增 free function `commands::try_apply_pending(state, queue)`，
  與 `apply_pending` 同樣的 precondition 與 mid-list-failure
  atomicity，但把 order-execution rejection 從 `Result::failure`
  改成 `Result::success` 攜帶 `rejection` record；non-execution
  錯誤（precondition / NaN delta / unknown policy / unknown
  category）仍維持 `Result::failure`，所以真正的 validation
  錯誤不會被吞掉。實作上抽出 anonymous-namespace 的 `dispatch_one`
  helper 給兩個函式共用，`apply_pending` 的 legacy rejection error
  string 透過 `format_rejection_message` 保持 byte-identical，
  M2.18 / M2.19 既有 test 完全不變。Drive-by：把 `order_execution.cpp`
  裡 PR #46 reviewer flagged 的「Only EnactPolicy is evaluated in
  this PR」過時註解改成同時提 M2.18 EnactPolicy 與 M2.19
  AdjustBudget arm。對應 RFC-020 §2 命令阻力的「讓 UI / forensics
  能讀懂為什麼某條命令被擋下」需求，是未來 UI / REPL / CI driver
  的入口 seam。**M2.20 不做** save format 變更（仍 v10）、
  `apply_pending` signature / 行為變更、persistent attempted-
  command log、新 `state.logs` 條目、`RunOutcome` 拒絕計數 /
  list（M2.21 候選）、DataLoader / replay primitive / runner /
  CLI / M1 system 變更、新 `PlayerCommandKind`、新 CSV 欄位、
  AI / events / UI / scheduler、threshold / formula 調整。
  **M2.19（AdjustBudget execution gate）** 延伸 M2.18 的命令拒絕
  shape 到 `AdjustBudget`，加入一個 category-aware 的單欄位切換：
  `command.budget_category == "military"` 時 gate 看
  `military_loyalty`，其他 6 個 category 仍走 `bureaucratic_compliance`。
  新常數 `kAdjustBudgetComplianceThreshold = 0.3`（與 M2.18 同值，
  維持 canonical default-0.5 scenario Accepted 不變）。`evaluate()`
  的 `AdjustBudget` arm 依 category 選 input → 計算
  `resistance = 1.0 - selected` →
  `status = (selected >= 0.3) ? Accepted : Rejected`。
  `commands::apply_pending` 在 M2.5 的 finite-delta + category
  whitelist 檢查之前先呼叫 `evaluate`：Rejected 時直接回
  `Result::failure`，錯誤訊息包含 `order_execution` /
  `rejected` / `AdjustBudget` / `budget_category` / selected
  compliance 數值 / threshold；rejected command 留在 queue head、
  state 不 mutate、`state.applied_commands` 不 append（沿用
  M2.3 / M2.4 mid-list-failure atomicity）。軍事 category 走
  military_loyalty 的設計反映 RFC-020 §2「軍方陽奉陰違」的核心：
  低忠誠度政權想改變軍隊預算時會被擋下；其他 category 仍由
  官僚層處理，與 EnactPolicy 一致。Replay 相容性：所有現存 v10
  save 在 default 0.5 authority 下都 Accept，replay 不會 regression。
  對應 RFC-020 §2 命令阻力列表的第二個落地工作。**M2.19 不做**
  save format 變更（仍 v10）、`Delayed` / `Distorted` outcome、
  `military` 之外的 per-category routing（例如
  `intelligence` → `intelligence_capability`，留給未來 PR）、
  weighted multi-input formula、RNG / 機率 gate、scheduler、
  rejected 時的 `state.logs` 條目、`RunOutcome` 拒絕計數
  （M2.20 候選）、DataLoader / policy effect / replay primitive /
  runner 變更、新 `PlayerCommandKind`、新 CSV 欄位、M1 system
  變更。
  **M2.18（EnactPolicy execution gate）** 是 M2 第一個會「拒絕」
  玩家命令的 sub-milestone。`order_execution` 新增三件事：
  常數 `kEnactPolicyComplianceThreshold = 0.3`、`ExecutionStatus`
  enum 多一個 `Rejected` variant、`OrderExecutionOutcome` 多一個
  `resistance` double field。`evaluate()` 根據 `command.kind`
  分流：`PlayerCommandKind::EnactPolicy` 計算
  `resistance = 1.0 - bureaucratic_compliance`，並依
  `bureaucratic_compliance >= 0.3` 決定 `Accepted` / `Rejected`；
  `PlayerCommandKind::AdjustBudget` 仍永遠 `Accepted`、
  `resistance` 維持預設 0.0。`commands::apply_pending` 在 M2.3
  policy lookup 之前先呼叫 `order_execution::evaluate`：
  `Rejected` 時直接回 `Result::failure`，錯誤訊息包含
  `order_execution` / `rejected` / `policy_id_code`；rejected
  command 留在 queue head、`state.applied_commands` 不 append、
  state 完全不 mutate（沿用 M2.3 / M2.4 mid-list-failure
  atomicity）。Threshold 0.3 的選擇：canonical scenario 預設
  `bureaucratic_compliance = 0.5`（M2.16 DataLoader 預設值），
  0.3 讓現有測試與 fixture 全部 Accepted，但給 low-compliance
  威權／崩潰政權留出可見的命令阻力空間，呼應 RFC-020 §2
  「政策可能遭遇官僚拖延」的核心。Replay 相容性：M2.7
  `replay_with_time` 對每個 log entry 呼叫 `apply_pending`，因為
  v10 save 中所有 entry 都是在 default 0.5 authority 下被
  Accepted，所以 replay 仍會重新 Accept。對應 RFC-020 §2 命令
  阻力的第一個落地工作。**M2.18 不做** save format 變更（仍
  v10）、`AdjustBudget` gate（留給 M2.19）、`Delayed` /
  `Distorted` outcome、multi-input / weighted formula、
  probabilistic / RNG gate、scheduler / pending-execution queue、
  rejected 時的 `state.logs` 條目、`RunOutcome` 增加 rejected
  counter、DataLoader / policy effect / replay primitive 變更、
  新 `PlayerCommandKind`、新 CSV 欄位、M1 system 變更。
  **M2.17（OrderExecutionSystem skeleton）** 為 M2 引入第一個讀
  M2.16 `government_authority` 的 system。新模組
  `leviathan::systems::order_execution` 包含三個型別與一個 free
  function：`OrderExecutionInputs`（actor country 的 4 個 authority
  ratio snapshot，預設 0.5）、`ExecutionStatus` enum（M2.17 只 ship
  `Accepted`，但 `Rejected` / `Delayed` / `Distorted` 在 header
  以註解方式保留 RFC-020 §2 對應的未來 variant）、
  `OrderExecutionOutcome { status, inputs }`，以及
  `evaluate(state, command) → Result<OrderExecutionOutcome>`。
  `evaluate` 沿用 M2.3 `apply_pending` 的 precondition shape
  （`state.player_country` 須有效且 index 進 countries），把
  actor 的 government_authority 整段 snapshot 到 outcome，然後一律
  回 `Accepted`。函式是 pure read：不 mutate state、不寫 log、不
  動 RNG、不 inspect `command.kind`（只是讓 signature 為 M2.18+
  保留 API shape）。**沒有 caller wire 進來** —
  `commands::apply_pending` 行為與 M2.5 / M2.16 byte-identical，
  M1.10 monthly pipeline 也不變。**故意不 ship `resistance`
  欄位**：避免假裝公式 shape 已定；M2.18+ 在引入公式時一起 ship
  resistance representation。新 `order_execution.cpp` 接進
  `leviathan_systems`，新 `order_execution_test.cpp` 接進
  `leviathan_tests`。10 個新 doctest cover preconditions、success
  path、inputs mirror authority、non-mutation、kind-independence、
  determinism、default outcome。對應 RFC-020 §2-§3 的「命令不
  保證成功」未來工作的第一個資料 seam。**M2.17 不做** save format
  變更（仍 v10）、`commands::apply_pending` 行為變更、`resistance`
  欄位 / 公式、`Rejected` / `Delayed` / `Distorted` 行為、replay
  變更、policy effect 變更、DataLoader 變更、新 `PlayerCommandKind`、
  新 CSV 欄位、新 `state.logs` 條目、AI / events / UI / scheduler。
  **M2.16（GovernmentAuthorityState）** 為 `core::CountryState` 新增
  `government_authority` 子結構，是 M2 第一個真正的 gameplay state
  擴張，對應 RFC-020 §3「國家掌控力」的 stripped-down 子集。新型別
  `core::GovernmentAuthorityState` 內含 4 個 `[0, 1]` ratio 欄位、
  預設皆為 `0.5`：`bureaucratic_compliance`（官僚服從度）、
  `military_loyalty`（軍方忠誠）、`intelligence_capability`（情報
  能力）、`media_control`（媒體控制）。`corruption` 與
  `administrative_efficiency` 已存在於 M1.1 不重複；`local_control`、
  `legal_mandate`、`leader_prestige`、`party_organization` 暫時不
  做、文件化為延期項目。**Save format bumped v9 → v10**：save 層
  block REQUIRED（每個 sub-field 經 `require_ratio` 嚴格驗證，
  finite + `[0, 1]`），DataLoader 對 country JSON 保持 OPTIONAL
  策略（缺 block → 全部 default 0.5；present block → 嚴格驗證，
  partial block 與 out-of-range 都會 reject 並把欄位名寫進錯誤
  訊息）。`diagnostics::compare_states` 擴張到走訪 4 個 sub-field，
  field_path 與 save JSON 結構一致
  （`countries[0].government_authority.bureaucratic_compliance`
  等）。**Data-only**：完全沒有 M1 system 讀或寫這幾個 field，
  M1 monthly pipeline 與 M2 command path 行為與 byte-identical
  determinism contract 完全不受影響。對應 RFC-090 §M2 task 2.16
  「政府權威 state 基礎」。**M2.16 不做** OrderExecutionSystem /
  命令執行阻力（M2.17 候選）、policy effect 對 government_authority
  target dispatch、新 `PlayerCommandKind` variant、新 CSV 欄位、
  scenario / country fixture 變更、新 `state.logs` 條目、M1 system
  變更。
  **M2.14（Replay target-date CLI）** 在 runner 新增
  `--target-date YYYY-MM-DD` 旗標（須與 `--replay` 並用），把 M2.8
  的 replay flow 限縮到指定日期。一個 flag 兩個作用：log 截斷
  （loaded.applied_commands 中 `applied_on > target_date` 的條目
  直接略過，因為 M2.7 monotonic non-decreasing 保證，所以單向掃過
  一遍就夠）+ replay 後時間延伸（`step_one_day` loop 直到
  `state.current_date == target_date`，過程中遇到月份邊界
  自然觸發 M1.10 monthly pipeline）。`--target-date` 經
  `core::GameDate::parse` 在 parse 時就拒絕格式錯誤；scenario-start
  precondition（target 不得早於 scenario 起始日）在 run() 進到
  begin_tick 之前驗證，因此 bad target 落在 M2.9 pre-`end_tick`
  no-artefact contract 內。`RunnerOptions` 新增
  `std::optional<core::GameDate> target_date`；`main()` 在 replay
  block 多印 `Target date: <value>` 一行。`replay_with_time` /
  `step_one_day` 語意都沒變 —— M2.14 純粹是 runner 端的接合。
  M2.9 contract 對應補上兩條 pre-`end_tick` 條目（target-date
  precondition、post-replay step loop 的 monthly pipeline failure）。
  新增 dated-log test helper `build_source_with_dated_log` 手動拼出
  具備非起點日期的 `applied_commands` log，方便測試截斷情境（一般
  apply_pending 跑出來的 log 全部都在 scenario 起始日）。對應
  RFC-090 §M2 replay family 的「截斷重播窗口」需求。**M2.14 不做**
  save schema 變更（仍 v9）、`--target-date` 在 `--replay` 之外
  使用、sub-day 解析度、與 `--verify` 的特別整合（mismatch 結果
  仍對 source save 比較，用戶自行解讀）、新 gameplay、新
  `state.logs` 條目、M1 system 變更。
  **M2.9（Replay CLI error-path hardening，補位版）** 是補回 M2
  sprint 中跳過的編號：M2.8 引入 `--replay` 之後，當時把編號讓給
  了 M2.10 → M2.13 一連串 verify-family，把 `--replay` 自身的
  failure-path artefact semantics 留成隱含契約。M2.9 為這條契約
  補上明確文件與 regression test，但**只覆蓋「pre-`end_tick`」**
  那一塊：當 `--replay` 在進到 `end_tick` 之前失敗
  （`save_system::load` 失敗、缺 `--scenario`、`begin_tick` reject、
  `replay_with_time` 失敗──含 out-of-order log / unknown policy
  id_code / malformed budget command / monthly pipeline 失敗），
  runner 不會寫出任何 output artefact（save.json / events.jsonl /
  summary.csv / countries.csv / factions.csv）。原因是 file write
  全部集中在 `end_tick`，而那幾條失敗路徑都不會走到 `end_tick`。
  **`end_tick` 自身的 I/O failure 明確不在這份 contract 範圍內**：
  `end_tick` 依序寫 save → log → summary CSV → countries CSV →
  factions CSV，是非 transactional 的；如果其中一個 write 在前面
  已經成功之後失敗，disk 上會留下 partial artefact。把 `end_tick`
  改成 atomic temp-file + rename 是未來 PR 的工作，M2.9 故意不
  納入。`runner::run` doc comment 新增 "M2.9 contract" 段落明確列出
  pre-`end_tick` failure 條件與「零 artefact」保證，並附 NOTE 說明
  end_tick 自身 failure 不被涵蓋；新增 3 個 doctest test 對三條代表性
  pre-end_tick failure path 做 regression：source 檔案不存在、log
  順序倒轉、unknown policy id_code，每個 test 都把五個 artefact
  path 都接上並驗證最終 disk 上一個都沒有。對應 RFC-090 §M2 task
  2.7「玩家操作回放錯誤路徑收尾」與 RFC-001 §「測試先寫」精神。
  **M2.9 不做** library 行為變更、save schema 變更（仍 v9）、新
  flag、新 gameplay、failure 時的新 state.logs 條目、retry /
  rollback machinery、replay-mode 的 partial-progress reporting、
  atomic `end_tick` writes、M1 system 變更。
- 未落地：RFC-020 完整政治、RFC-030 完整經濟、RFC-040 外交與戰爭、
  RFC-050 事件與隱藏真相、RFC-080 §6 §7 §10 政變 / 內戰 / 誤判公式。

實作 PR 對應的 design notes 全部在 `../docs/`，索引見
`../docs/README.md`。
