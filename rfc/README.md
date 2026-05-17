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
- **M2（進行中）** — 玩家操作原型。**M2.1（Player country
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
- 未落地：RFC-020 完整政治、RFC-030 完整經濟、RFC-040 外交與戰爭、
  RFC-050 事件與隱藏真相、RFC-080 §6 §7 §10 政變 / 內戰 / 誤判公式。

實作 PR 對應的 design notes 全部在 `../docs/`，索引見
`../docs/README.md`。
