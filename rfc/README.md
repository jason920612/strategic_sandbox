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
- **M5（進行中，RFC-090 §M5 event engine）** — **M5.3
  （EventMatch actor-binding skeleton）** 是 M5 的
  第三個 sub-milestone。擴充 M5.2 `event_evaluator`
  module 的回傳形狀，讓未來的 firer / effects-applicator
  知道效果該套到 **哪個 entity**。新增
  `enum class TriggerActorKind { Country, InterestGroup }`、
  `struct TriggerActor { TriggerActorKind kind;
  std::string id_code; core::CountryId country;
  std::size_t index; }`、`struct TriggerEvaluation {
  std::size_t trigger_index; TriggerActor actor; }`。
  原 M5.2 `TriggerMatch` **改名為 `EventMatch`** 並新增
  `std::vector<TriggerEvaluation> triggers`（每條
  `def.triggers` 一筆，僅 ALL match 時 populated）；
  `event_index` 與 `event_id_code` field 名一字不改，
  讓 M5.2 樣的 read site 仍能直接 compile。新增兩支
  free function：`std::optional<TriggerActor>
  trigger_actor(state, EventTrigger)` 回傳首個滿足
  trigger 的 entity（nullopt = 沒有滿足者 / 未知
  target / op / 非有限值）；`std::optional<EventMatch>
  evaluate_match(state, EventDefinition)` 回傳整個
  per-trigger actor binding（nullopt = 任一 trigger
  fail）。`match_events` 名字不變，回傳型別由
  `vector<TriggerMatch>` 寬化為 `vector<EventMatch>`，
  攜帶完整 per-match actor binding。**Actor selection
  policy = "first in vector order"** ── deterministic，
  因為 `state.countries` / `state.interest_groups` 是
  canonical scenario-load 順序（save layer
  byte-stably 保留）。把每個 trigger 所有滿足者都收
  下來（而非只首個）deferred 到未來 M5.x；first-match
  選擇與 M5.2 bool predicate short-circuit 一致，所以
  gate 語意 0 變動，M5.3 只暴露 M5.2 predicate 隱含
  選的 first-match。原兩支 bool predicate
  （`trigger_matches`、`evaluate`）保留作為便宜的
  「是否 match」probe。對 kind `InterestGroup` 的 actor，
  `country` field 是其 **owning** country（M3.1+ 起
  IG 必有 owning country）；對 kind `Country` 則為
  該 actor 自己的 id。這讓「IG-trigger 觸發時把效果
  套到 IG 所屬國家」這條未來路徑不必再 lookup。
  **16 個新 doctest case（965 total，62006 assertions；
  per `feedback_ctest_masks_doctest` 規則直接跑
  `leviathan_tests.exe` 驗證**）：per-target actor
  binding / nullopt 邊界 / 重複 id_code 釐清（index
  是 canonical handle）/ `evaluate_match` nullopt vs
  populated / cross-scope binding / 空 triggers
  vacuous true 但 actors 為空 / 共享 actor case（多條
  trigger 同打一國皆綁該國，indices 按 def 順序）/
  `match_events` 攜帶 binding 並按 canonical 順序 /
  canonical no-fire regression 仍成立（M5.3 不改
  gate）/ no-mutate regression / evaluator 仍不
  consult effects / `match_events` 仍不 append 到
  `state.logs` / `state.applied_commands`。新
  `docs/m5-3-event-match-actor-binding-skeleton.md`
  design note。**沒有 event firing / log entry on
  match / `events.jsonl` 變動 / history append /
  effects application / 「每個 trigger 收集所有滿足
  entity」（M5.3 只收 first）/ selection policy
  options（hard-coded "first"；無 random / weighted
  / `for_country:GER`）/ runner 或 monthly 整合 /
  auto-evaluation cadence / save format bump（仍 v13）
  / 新 artefact（仍 10）/ 新 `RunnerOptions` field /
  CLI flag / 新 `PlayerCommandKind` / 新 state field
  / 更廣 trigger op / target / logical operator /
  event author tooling / UI surface / balance pass /
  對 M5.1 schema 或 M5.2 gate 語意的變動 / 對
  M1/M2/M3/M4 system 的變動 / 對 `scenario_loader`
  / `save_system` / `diagnostics` 的變動 /
  `docs/milestone-5-checkpoint.md`（仍 deferred ──
  在 M5.3 開 checkpoint 仍是 premature framing）**。
  M5 remains in progress。
- **M5（歷史進行中）** — **M5.2
  （trigger evaluator skeleton）** 是 M5 的第二個
  sub-milestone。新增 `leviathan::systems::event_evaluator`
  module（header `include/leviathan/systems/event_evaluator.hpp`
  + impl `src/leviathan/systems/event_evaluator.cpp`）。
  Public API：`struct TriggerMatch { std::size_t
  event_index; std::string event_id_code; }`、
  `bool trigger_matches(state, EventTrigger)`、
  `bool evaluate(state, EventDefinition)`、
  `std::vector<TriggerMatch> match_events(state)`。
  **三個 function 全部 pure read** ── 不改 GameState、
  不寫 log、不 fire event、不動 time / RNG。
  **語意**：per-op numeric compare（M5.1 op
  allowlist `lt`/`lte`/`gt`/`gte`）；per-target
  dispatch（M5.1 target allowlist：`country.stability`
  / `country.legitimacy` /
  `country.government_authority.bureaucratic_compliance`
  路由到 `state.countries`；`interest_group.radicalism`
  / `interest_group.loyalty` 路由到
  `state.interest_groups`）；**ANY-entity-satisfies
  aggregation**（country 範圍 trigger 只要有任一
  country 滿足就 match；entity list 空 → false，
  existential quantifier over empty）；**AND across
  `def.triggers`**（每條 trigger 都要 match；空
  triggers vector 是 vacuously true ── M5.1 loader
  本來就 reject 空 triggers，這個語意只為手寫 def 與
  defensive reader 釘住）。Unknown target / op、
  非有限 trigger value、非有限 state value 一律
  evaluate 成 **false**（defensive ── M5.1 loader
  是 gate，evaluator 不重複 allowlist 訊息）。
  `match_events` 依 `state.events` vector 順序走，
  回傳 canonical 順序。**28 個新 doctest case
  （949 total，61924 assertions；per
  `feedback_ctest_masks_doctest` 規則直接跑
  `leviathan_tests.exe` 驗證**）涵蓋：每個 target
  的每個 op / per-target dispatch / ANY-entity edge
  cases（含空 entity list = false）/ cross-scope
  AND / 空 triggers vacuous truth / unknown
  target / unknown op / 非有限值處理 / canonical-
  event-shape vs canonical-state-shape regression
  （在 evaluator 層級釘住 M5.1 fixture「不 fire」
  屬性）/ no-mutate regression（countries / IG /
  events / logs / applied_commands）/
  evaluator-is-trigger-only（effects vector 內容
  不被 consult；`match_events` 不會 append 到
  `state.logs` / `state.applied_commands`）。新
  `docs/m5-2-trigger-evaluator-skeleton.md` design
  note。**沒有 event firing / log entry on match /
  `events.jsonl` 變動 / history append / effects
  application / per-actor selection（trigger 來自
  哪個 country / IG 不記錄 ── 等 effects-application
  那 sub-milestone 才加）/ runner 或 monthly
  整合 / auto-evaluation cadence / save format bump
  （仍 v13）/ 新 artefact（仍 10）/ 新
  `RunnerOptions` field / CLI flag / 新
  `PlayerCommandKind` / 新 state field / 更廣
  trigger op（`eq` / `ne` / `between` / `in`）/
  更廣 trigger target / trigger logical operator
  （`all_of` / `any_of` / `not` /
  `for_country:GER` per-trigger）/ event author
  tooling / UI surface（events 仍不在 `map.html`
  / `provinces.svg` / 任何 CSV）/ balance pass /
  對 M5.1 schema 的變動 / 對 M1/M2/M3/M4 system
  的變動 / 對 `scenario_loader` / `save_system` /
  `diagnostics` 的變動（它們仍 own `state.events`；
  M5.2 只 read 它）/ `docs/milestone-5-checkpoint.md`
  （仍 deferred ── 在 M5.2 開 checkpoint 仍是
  premature framing）**。M5 remains in progress。
- **M5（歷史進行中）** — **M5.1
  （EventDefinition trigger/effect schema foundation）**
  是 M5 的第一個 sub-milestone。把 M0 的
  `EventDefinition { EventId id; std::string name; }` stub
  原地升級成 typed `{ id_code, name, description,
  triggers[], effects[] }` shape；新增
  `core::EventTrigger { target, op, value }`。
  `EventDefinition::effects` 直接重用 M1.4 的
  `core::PolicyEffect`，使未來 M5.x evaluator 可以透過
  既有的 `policy::apply_policy_effects` 派發 event
  effect，不需要平行 effect type。Trigger `op` allowlist：
  `lt` / `lte` / `gt` / `gte`。Trigger `target` allowlist：
  `country.stability`、`country.legitimacy`、
  `country.government_authority.bureaucratic_compliance`、
  `interest_group.radicalism`、`interest_group.loyalty`。
  Trigger `value` 必須是 finite double。Effect 的
  target/op 在 load time 只要求 non-empty string + finite
  value（mirror `data_loader::parse_policy` 的
  no-allowlist-at-load 規則；effect target/op 的 allowlist
  住在 M1.5 `policy::apply_policy_effects`，未來 M5.x
  evaluator 直接 inherit）。`triggers` 必須非空；
  `effects` 可以為空（「warning-only event」 class）。新增
  `scenario_loader::parse_event_file` per-file parser；
  `ScenarioManifest` 多一個 optional `events[]` 檔案路徑
  陣列，shape mirror M4.1 `provinces[]`；
  `ScenarioLoadOutcome` 多一個 `events_loaded`。Event
  `id_code` 跨檔唯一性在 load 與 save 兩層都檢查。
  **Save format 從 v12 → v13**：events array 在 save 層
  required，每筆 entry 都驗證 id_code/name/description/
  triggers/effects；v12 save 在 load 時以
  `supports 13` 直接 reject。`diagnostics::compare_states`
  在 `state.provinces` 之後 walk `state.events`，field path
  `events[N].id_code` / `events[N].triggers[M].target` 等。
  新 canonical fixture `data/events/1930_core_events.json`
  含兩個 event ── `low_stability_unrest`（trigger
  `country.stability lt 0.30`，effect
  `country.stability add -0.02`）與
  `radical_interest_group_warning`（trigger
  `interest_group.radicalism gt 0.75`，effect
  `country.legitimacy add -0.01`）；值刻意挑成
  **在 canonical scenario 上都不會 fire**（GER stability
  是 0.55 > 0.30；canonical interest-group radicalism 是
  0.10 < 0.75）。兩個 canonical manifest
  （`1930_minimal.json` + `1930_with_start_policies.json`）
  都引用此檔。約 25 個新 doctest case（8 個 save_system
  + 13 個 scenario_loader + 4 個 diagnostics + 2 個
  runner regression；**921 total，61862 assertions；
  per `feedback_ctest_masks_doctest` 規則，直接跑
  `leviathan_tests.exe` 驗證**）。新
  `docs/m5-1-event-definition-schema-foundation.md`
  design note。**沒有 `docs/milestone-5-checkpoint.md`** ──
  該 checkpoint 故意延後到 M5 多個 surface 落地後再開
  （M3 / M4 各自在 M3.7 / M4.9 才開 checkpoint，
  並非 .1）；在 M5.1 開 checkpoint 會是 premature
  milestone framing。**沒有 trigger evaluator / 事件 firing /
  effects 套用 / monthly 整合 / `events.jsonl` 語意變更 /
  runner CLI flag / 新 artefact（仍 10）/ 新
  `PlayerCommandKind` / cooldown / weight / exclusivity /
  chained events / choices / RNG-driven outcome branches /
  historical-once gating / log-on-fire / 更廣 trigger
  op（`eq` / `ne` / `between` / `in`）/ 更廣 trigger
  target / per-effect actor / per-effect duration / event
  categories / event ordering / save-schema migration shim /
  UI surface / balance pass / 對 M1/M2/M3/M4 system 的
  任何變動 / 對 `PolicyEffect` shape 的任何變動**。M5
  remains in progress。
- **M4（已關閉，RFC-090 §M4 SVG map + UI）** — **M4.23
  （M4 exit / close-out）** 是 docs-only 的 M4 出口 PR，
  形式對應 M1.17 / M2.22 / M3.9。發布
  `docs/milestone-4-result.md`（M4 exit report，七個
  section：M4.1–M4.23 ledger / final dataflow /
  10-artefact contract / save-format v12 floor /
  architectural invariants 每個未來 milestone 都必須保留
  / deferred items 分 Category A、B、C / 中性 next-
  milestone candidates）。把 `docs/milestone-4-
  checkpoint.md` 註記為 historical（檔頭加 note 指向
  exit report 為新的 authoritative record，本體保留供
  archaeology）。把三份 README（root + docs/ + rfc/）翻
  到「M4 closed」。**沒有 code / formula / fixture /
  test 變動**（892 doctest cases / 61742 assertions 與
  M4.22 相同；`provinces.svg` 與 `map.html` bytes 與
  M4.22 完全相同）。2026-05-17 force-reset 的教訓（見
  `milestone-3-result.md` §7）是「不要在 milestone 真正
  退出之前寫 exit report」的記錄理由 ── 之前在 M3.7+
  曾經 drift 成 premature close-out + invented M4.X
  numbers + 9th artefact 而被 force-reset；恢復用的
  pattern 是「每個 milestone 都有一個 dedicated 最終
  close-out PR，只做這件事」。M1.17 / M2.22 / M3.9 都
  follow 這個 pattern；M4.23 對 M4 follow 同樣的 pattern。
  **M4 closes here.** **沒有「M5 in progress」字樣** 出現
  在這個 PR；M5 會在 reviewer 給綠燈時，由 M5 自己的
  第一個 sub-milestone PR 開始；M4.23 對「下一個
  milestone 是哪一個」不做任何宣告。
  **M4.23 不做** 新 system / 新 formula / 新 artefact
  （仍 10）/ save schema bump（仍 v12）/ 新 state field
  / 新 fixture / 新 test（close-out PR 是 docs-only ──
  M4.22 已經加了 integration test G 作為「M4 viewer
  contract complete」的 consolidated pin）/ 新
  `InterestGroupKind` / `PlayerCommandKind` / renderer
  行為變更 / rename 任何 data-* attribute / 動
  `provinces.svg` 或 `map.html` bytes / runner CLI
  flag / atomic `end_tick` write / 「M5 in progress」
  字樣 / 對下一個 milestone 是哪一個的任何主張。
  **M4 closes here.**
- **M4（進行中歷史，現已關閉）** — **M4.22
  （close-out readiness checkpoint）** 對應 M3.7 在 M3
  反應 loop 的角色：docs + 1 個 integration test，
  **零 renderer 行為變更**。M4.21 之後 M4 viewer stack
  已經結構完整，M4.22 正式評估準備度、用一個 consolidated
  end-to-end integration test（test G "M4 viewer contract
  complete"）鎖住目前的 contract、修掉 PR #87 reviewer
  flag 出來的 1040px 數學寫法（實作一直都用 1040 =
  1000 + 2×20，只有 doc 文字 drift 成「1000 + 20×2 +
  1×2 = 1042」；現在跨 svg_export.cpp / svg_export.hpp
  / 三份 READMEs / m4-21 design note 全部修正），並
  把 M4 exit report（`docs/milestone-4-result.md`）
  **故意延後** 等 reviewer 給「do M4 close-out」綠燈
  才開新 PR 寫。`docs/milestone-4-checkpoint.md` 多
  一個新的 section 9「Close-out readiness assessment」
  含四個 subsection：(9.1) M4 ship 的一句話總結 ──
  deterministic 10-artefact viewer stack，包含 5 個
  data-* identity surface、click+keydown+hover
  listeners、transient `.selected` / `:focus-visible`
  / `:hover` CSS rings、`role="button"` + `aria-label`、
  hover-status text bar、viewport meta + 1 條
  responsive `@media`；(9.2) 仍 deferred items 分三類
  ── Category A（defer-to-M5+ gameplay-domain：鄰接
  edge、terrain、ownership dynamics、event integration）、
  Category B（recommended post-M4 follow-up polish：
  broader ARIA `aria-selected` / `aria-current` /
  `aria-pressed` / `aria-live` / `aria-describedby`、
  pair-hover、position-aware tooltip、selection 持久化、
  keyboard polish、mobile-only layouts、dark-mode、CSS
  animations）、Category C（not-needed-for-close
  nice-to-haves：container queries、`@supports`、
  responsive font sizing、JS responsive surface、hover
  delay）；(9.3) verdict ── **M4 結構上已準備好 close-out**；
  (9.4) PR #87 數學寫法修正。Integration test G 在 canonical
  scenario 上一次性檢查所有 M4.x surface marker（viewBox、
  circle、text、5 個 data-* 屬性、tabindex、role、
  aria-label、details panel、legend、click+keydown+
  mouseover+mouseout listeners、hover-status、`.selected`
  CSS、`:focus-visible`、`:hover`、fields-array labels、
  viewport meta、`@media` block）並釘住 `provinces.svg`
  **完全不帶** HTML-wrapper surface、所有 7 個
  unconditional artefact 都存在、`save_version: 12`。
  **reviewer 接下來的決策有三條路**：(1) **M4 close-out
  PR**（publish `docs/milestone-4-result.md`，鏡像
  `milestone-3-result.md` shape；翻三份 README 為「M4
  closed」）；(2) **再多一個 polish PR**（從 checkpoint
  section 9.2 Category B 挑一項）；(3) **停 M4，前往 M5**。
  M4.22 不替 reviewer 做選擇。**M4 在這個 PR 結束時仍
  in progress** ── 沒有寫 `docs/milestone-4-result.md`，
  沒有「M4 closed」字樣。renderer bytes 與 M4.21 完全
  相同 ── svg_export.cpp 的改動是純註解。**Artefact
  數量不變（仍 10）；save 格式不變（仍 v12）**；M1.17 /
  M2.22 / M3.7 byte-identical determinism contract 仍 by
  construction 通過。1 個新 doctest case（共 892、61742
  assertions；依照 `feedback_ctest_masks_doctest` 規則
  **用直接 run `leviathan_tests.exe` 驗證**）。
  **M4 in progress.**
  **M4.22 不做** M4 close-out / `docs/milestone-4-result.md`
  / 「M4 closed」字樣 / 新 feature surface（M4.22 只是
  docs + 1 個 integration test + 1 個 math wording fix）
  / 新 system / 新 formula / 新 artefact / 新 state field
  / 新 fixture / save schema bump / 新 `InterestGroupKind`
  / `PlayerCommandKind` / rename 任何 data-* attribute /
  renderer 行為變更 / broader ARIA / pair-hover /
  position-aware tooltip / keyboard polish / selection
  持久化 / hover delay / dark-mode（全部按 checkpoint
  section 9.2 deferred）/ 動 `provinces.svg` 或
  `map.html` bytes。
  **M4.21（responsive viewport skeleton）** 讓 `map.html` 在窄
  螢幕 / 行動裝置上能正常顯示。兩個小改動：(a) `<head>`
  裡新增 `<meta name="viewport"
  content="width=device-width, initial-scale=1">`（位置
  在 `<meta charset>` 之後、`<title>` 之前），讓行動瀏覽器
  以裝置實際寬度排版（而不是預設約 980px 的桌面模擬
  viewport），並用 `initial-scale=1` 取消初次載入的
  auto-zoom-out；(b) `<style>` block 結尾新增一條
  `@media (max-width: 1040px)` 規則：`svg { width: 100%;
  max-width: 100%; height: auto; }`，讓 SVG 在窄螢幕上
  填滿欄寬而不會出現水平捲動。1040px 的 threshold = 1000
  （SVG 寬度）+ 2 × 20px（body padding）= 1040px
  （SVG 的 1px border 在 padded column 內部，不會額外
  增加 layout 寬度）。
  threshold 以上 M4.6 桌面 rule（`margin: 0 auto`）會贏；
  threshold 以下 M4.21 的 @media 規則會贏。既有的
  `viewBox` 讓百分比寬度下 SVG aspect ratio 自然保留。
  legend / details panel / hover-status bar 都靠既有的
  `max-width: 1000px; margin: 0 auto` 自然繼承欄寬，所以
  不需要為它們寫額外的 mobile rule。**narrowly reverses
  了 M4.5–M4.20「no `<meta viewport>`、no media queries」
  的非目標** ── 只 ship 一個 viewport meta + 一個 @media
  block。更廣的 responsive surface（mobile-only layout、
  breakpoint cascade、`@container` container queries、
  `prefers-color-scheme` / `prefers-reduced-motion`、
  responsive font sizing 用 `clamp()` / `vw` / `vh`、JS
  responsive 用 `matchMedia` / `ResizeObserver` /
  `window.innerWidth` / `"resize"` listener）全部仍然
  deferred。**純 CSS** ── 沒有 JS responsive 邏輯。
  M4.10/M4.11/M4.12/M4.13/M4.15/M4.16/M4.17/M4.19/M4.20
  invariant 全部沿用，完全 additive。依照
  `feedback_checkpoint_drift` 規則，
  `docs/milestone-4-checkpoint.md` 在這個 PR 裡 **inline
  刷新**（`<head>` 加 viewport meta；`<style>` 加 @media
  rule；selector-count 措辭從「20 selectors」改成「20
  plain selectors plus one @media block」；VISUAL POLISH
  deferred bucket 改寫成「viewport + 1 @media 已 ship；
  broader responsive 仍 deferred」；invariants 區段把
  「no <meta viewport>, no media queries」改寫成「only
  one viewport meta + one @media block」；"Refreshed at"
  stamp 延伸到 M4.21）。**M4 仍在 in progress** ── 沒有
  寫 `docs/milestone-4-result.md`，M4.21 只是再多一個
  skeleton sub-milestone，不是 exit。**save 格式仍
  v12**。`provinces.svg` bytes 與 M4.20 完全相同
  （viewport meta + @media 只在 `render_map_html`）；
  `map.html` bytes 有變（多 1 個 meta tag、1 條
  @media block）。**Artefact 數量不變（仍 10）；save
  格式不變（仍 v12）**；M1.17 / M2.22 / M3.7
  byte-identical determinism contract 仍 by construction
  通過。5 個新 doctest cases（共 891、61678 assertions；
  依照 `feedback_ctest_masks_doctest` 規則 **用直接 run
  `leviathan_tests.exe` 驗證**）。
  **M4 in progress.**
  **M4.21 不做** 第二個 viewport meta / @media block /
  `@container` / `@supports` / `min-width:` queries
  （只用 `max-width`）/ `orientation:` /
  `prefers-color-scheme` / `prefers-reduced-motion` /
  responsive font sizing (`clamp()` / `vw` / `vh`) / JS
  responsive surface（`matchMedia` / `ResizeObserver`
  / `window.innerWidth` / `window.innerHeight` /
  `"resize"` listener / `.onresize`）/ mobile-only
  layout rule（legend / details / hover-status 都靠既有
  max-width 自然 wrap）/ fluid font / CSS animation /
  transition / `@import` / `@font-face` / `<link>` /
  外部 CSS / font / inline event attribute / per-element
  inline `style="..."` / save schema bump / 新 state
  field / 新 artefact / 新 fixture / 新
  `InterestGroupKind` / `PlayerCommandKind` / rename
  M4.8 / M4.13 data-* key / 第二個 `<script>` /
  `<script src=>` / `<script type=>` / `fetch` /
  `XMLHttpRequest` / `localStorage` / `sessionStorage`
  / `history.pushState` / `window.location` /
  `navigator` / `innerHTML` / `outerHTML` /
  `document.write` / `eval` / `Function` /
  `insertAdjacentHTML` / 更廣的 ARIA（仍 deferred）/
  state mutation / commands / AI / events / selection
  持久化 / 鄰接 edge / terrain / overlay / runner CLI
  flag / 動 `provinces.svg` bytes / M4 close-out /
  `docs/milestone-4-result.md` / 「M4 closed」字樣。
  **M4.20（hover tooltip skeleton）** 在 `map.html` 加入一個小型
  hover-status 文字條。在 inline SVG 與 M4.10 details
  panel 之間 emit `<p id="hover-status"
  class="hover-status">&nbsp;</p>`；inline `<script>` 在
  既有的 per-element loop 裡（與 M4.10 click、M4.15
  keydown listener 同一個 loop）多註冊 `mouseover` 與
  `mouseout` 兩個 listener：mouseover 用 `getAttribute`
  讀 `data-name` 與 `data-owner-name`（M4.8 / M4.13 已
  有的屬性 ── 不新增任何 data-* 屬性），組合成
  `"<name> (<owner-name>)"`（owner-name 是空字串時
  fallback 成 `<name>` 而不加尾括號 ── 與 M4.17
  aria-label 的 fallback 同形），並用 `textContent`
  寫進 hover-status 條；mouseout 把 `textContent` 清成
  `""`。**寫入只用 `textContent` ── 永遠不用
  `innerHTML`**，M4.10 的 XSS-safe DOM API contract
  延續。新增一條 `.hover-status` CSS rule，提供置中、
  斜體、灰字、`min-height: 1em` 防止 layout 跳動。
  **tooltip 設計決策**：選 status-bar 而非 SVG
  `<title>` child（會與 M4.17 aria-label 競爭 accessible
  name，reviewer 已明確點出）、也不選 position-aware
  浮動 tooltip（超過 skeleton 範圍）。bar 在 document
  flow 裡固定位於 details panel 上方，遵循瀏覽器
  status-bar 慣例。**narrowly reverses 了 M4.19「no JS
  hover handler」這條非目標** ── 只有
  `mouseover` + `mouseout` 透過 `addEventListener` 上線；
  `mouseenter` / `mouseleave` / `.onmouseover` /
  `.onmouseout` / inline `onmouseover=` / `onmouseout=`
  全部仍然 absent。**hover 與 click 是兩個獨立的
  surface** ── M4.20 mouseover handler 完全不呼叫
  `showDetails` / `selectProvince`、不碰 `details.`、
  也不在 hovered element 上動 `.classList`。整份文件
  完全沒有 SVG `<title>` child；M4.5 head `<title>
  Leviathan Map</title>` 仍是文件裡唯一的 `<title>`。
  M4.10 的 XSS-safe DOM API、no-network discipline、
  「`map.html` 有且只有一段 inline `<script>`；
  `provinces.svg` 完全沒有 script」這條非對稱
  invariant、M4.12 transient `.selected`、M4.13 五個
  data-* DOM contract、M4.15 `tabindex` + keydown、
  M4.16 `:focus-visible`、M4.17 `role="button"` +
  `aria-label`、M4.19 `:hover` CSS 全部沿用，完全
  additive。依照 `feedback_checkpoint_drift` 規則，
  `docs/milestone-4-checkpoint.md` 在這個 PR 裡
  **inline 刷新**（selector 從 19 條增加到 20 條；
  HTML body shape 加上 item 2 `<p id="hover-status">`；
  script section 加上 M4.20 的 listener block；
  Interactivity surface section 加上 hover-status 條目；
  HOVER+TOOLTIPS deferred bucket 改寫為「hover-status
  bar 已 ship 在 M4.20；pair-hover / position-aware
  tooltip / SVG `<title>` / `mouseenter`+`mouseleave`
  仍 deferred」）。**M4 仍在 in progress** ── 沒有寫
  `docs/milestone-4-result.md`，M4.20 只是再多一個
  skeleton sub-milestone，不是 exit。**save 格式仍
  v12** ── hover-status 從 render-time 屬性讀取，不
  新增任何 persistent state field。`provinces.svg`
  bytes 與 M4.19 完全相同（hover-status 只在 HTML
  wrapper）；`map.html` bytes 有變（多 1 個元素、1
  條 CSS rule、2 種 listener 類型）。**Artefact 數量
  不變（仍 10）；save 格式不變（仍 v12）**；M1.17 /
  M2.22 / M3.7 byte-identical determinism contract 仍
  by construction 通過。7 個新 + 1 個 retune 的 doctest
  cases（共 886、61651 assertions；**用直接 run
  `leviathan_tests.exe` 驗證**，依照
  `feedback_ctest_masks_doctest` 規則 ── ctest 可能
  silently mask doctest CHECK failures）。
  **M4 in progress.**
  **M4.20 不做** SVG `<title>` child / position-aware
  浮動 tooltip / pair-hover / `mouseenter` /
  `mouseleave` / hover delay / hover-driven panel
  preview / `aria-live` on the bar / animation /
  transition / 更廣的 ARIA / state mutation / commands
  / AI / 點擊以外的 event / selection 持久化 / panel
  快捷鍵 / save schema bump / 新 state field / 新
  artefact / 新 fixture / 新 `InterestGroupKind` /
  `PlayerCommandKind` / rename M4.8 / M4.13 data-*
  key / 第二個 `<script>` / `<script src=>` /
  `<script type=>` / `<link>` / 外部 CSS / font /
  `<iframe>` / `<img>` / `fetch` / `XMLHttpRequest` /
  `localStorage` / `sessionStorage` /
  `history.pushState` / `window.location` /
  `navigator` / `innerHTML` / `outerHTML` /
  `document.write` / `eval` / `Function` /
  `insertAdjacentHTML` / inline event attribute /
  per-element inline `style="..."` / `<meta
  name="viewport">` / CSS animation / transition /
  media query / `@import` / `@font-face` / 鄰接
  edge / terrain / overlay / runner CLI flag / 動
  `provinces.svg` bytes / M4 close-out /
  `docs/milestone-4-result.md` / 「M4 closed」字樣。
  **M4.19（hover affordance skeleton）** 在 `map.html` 加入
  滑鼠 hover 的視覺提示，讓使用者在按下去前就看到
  「這個 marker 會回應我」。**純 CSS** ── 沒有改
  JavaScript，也沒有改 markup 結構（只動 `<style>`
  block）。M4.6 `<style>` block 新增兩條規則：
  `svg circle:hover { stroke: #666666; stroke-width:
  2; }` 與 `svg text:hover { text-decoration:
  underline; }`。位置放在 M4.10 `cursor: pointer` 之後、
  M4.12 `.selected` / M4.16 `:focus-visible` 之前 ──
  CSS specificity 三者相等，靠 source order 決定誰
  win，所以 `.selected` 與 `:focus-visible`（之後宣告）
  會在同一個元素上 override hover。三種狀態的顏色刻意
  分開：hover 灰色（`#666666`、2px）/ selected 黑色
  （`#000000`、3px）/ focused 藍色（`#1976d2`、4px）；
  狀態疊加時較粗的 stroke 自然贏。text 用 underline
  （與 M4.12 的 `font-weight: bold` 和 M4.16 的
  `outline` 機制不同），所以多重狀態疊加時讀起來仍然
  分明。**純 CSS ── 沒有 JS hover handler、沒有
  `mouseover` / `mouseout` listener、沒有 tooltip、
  沒有 SVG `<title>` 子元素**（`<title>` 子元素會與
  M4.17 `aria-label` 競爭 accessible name 的優先權）。
  Pair-hover（hover circle 時同時讓對應的 text 變色）
  deferred ── 那會需要 JS。**Checkpoint doc 在這個 PR
  裡 inline 刷新**（按新的 feedback rule，每個改 surface
  的 sub-milestone 都應該順手刷新
  `docs/milestone-4-checkpoint.md`，不要再讓它 drift 到
  需要專門的 refresh PR）：`<style>` selector 從 17 條
  增加到 19 條；HTML wrapper shape 區塊把兩條新 hover
  rule 列出來；interactivity surface 區塊加一個
  `:hover` CSS bullet；deferred items 的 HOVER+TOOLTIPS
  bucket 改寫成「basic :hover CSS 已 ship 在 M4.19；
  richer hover behaviour 仍 deferred」。M4.10 的
  XSS-safe DOM API、no-network discipline、「`map.html`
  有且只有一段 inline `<script>`；`provinces.svg` 完全
  沒有 script」這條非對稱 invariant、M4.12 transient
  `.selected` 機制、M4.13 五個 data-* DOM contract、
  M4.15 `tabindex` + keydown handler、M4.16
  `:focus-visible` rings、M4.17 `role="button"` +
  `aria-label` 全部沿用，完全 additive。**M4 仍在
  in progress** ── 沒有寫
  `docs/milestone-4-result.md`，M4.19 只是再多一個
  skeleton sub-milestone，不是 exit。**save 格式仍
  v12**。`provinces.svg` bytes 與 M4.17 完全相同
  （hover CSS 只在 HTML wrapper）；`map.html` bytes
  有變（多了兩條 CSS rule）。**Artefact 數量不變
  （仍 10）；save 格式不變（仍 v12）**；M1.17 / M2.22
  / M3.7 byte-identical determinism contract 仍 by
  construction 通過。5 個新 doctest cases（共 885）。
  **M4 in progress.**
  **M4.19 不做** JS hover handler / pair-hover /
  tooltip / SVG `<title>` 子元素 / hover-driven
  detail-panel preview / hover delay / animation /
  transition / 更廣的 ARIA（M4.17 之後仍 deferred）/
  M4.15 之外的 keyboard polish / selection 持久化 /
  save schema bump / 新 state field / 新 artefact /
  新 fixture / 新 `InterestGroupKind` /
  `PlayerCommandKind` / rename M4.8 / M4.13 data-*
  key / 第二個 `<script>` / `<script src=>` /
  `<script type=>` / `<link>` / 外部 CSS / font /
  `<iframe>` / `<img>` / `fetch` / `XMLHttpRequest`
  / `localStorage` / `sessionStorage` /
  `history.pushState` / `window.location` /
  `navigator` / `innerHTML` / `outerHTML` /
  `document.write` / `eval` / `Function` / inline
  event attribute / per-element inline `style="..."`
  / `<meta name="viewport">` / CSS animation /
  transition / media query / `@import` / `@font-face`
  / 鄰接 edge / terrain / overlay / runner CLI flag
  / 動 `provinces.svg` bytes / M4 close-out /
  `docs/milestone-4-result.md` / 「M4 closed」字樣。
  **M4.18（accessibility checkpoint refresh）** 對應 M4.14 在
  當時的角色：**零新行為**，只刷新狀態快照 + 補一個
  小型 integration assertion。
  `docs/milestone-4-checkpoint.md` 上一次刷新是 M4.14
  （涵蓋 M4.2–M4.13），M4.18 把它擴充到覆蓋 M4.15–M4.17
  三個 a11y surface：M4.15（keyboard focus skeleton ──
  `tabindex="0"` + keydown Enter/Space listener）、
  M4.16（focus-visible CSS skeleton ── `#1976d2` 藍色
  `:focus-visible` rings 與 M4.12 黑色 `.selected`
  stroke 視覺上分明）、M4.17（ARIA labels skeleton ──
  `role="button"` + `aria-label="<name>, <owner_name>"`
  ── narrowly reverses M4.15/M4.16 的「no ARIA」非目標）。
  刷新後的 checkpoint 列出 `<style>` block 裡 17 條
  selector（M4.14 時是 13 條；M4.16 多了 4 條
  `:focus`/`:focus-visible` rule）、一個新的
  「Accessibility surface (M4.15–M4.17)」區塊枚舉
  tabindex / keydown / focus-visible / role=button /
  aria-label / decorative-legend-swatch invariant、
  並把 deferred items 重新分類：KEYBOARD+FOCUS surface
  已 SHIPPED；更廣的 BROADER ARIA（`aria-selected` /
  `aria-current` / `aria-pressed` / `aria-live` /
  `aria-describedby` / `aria-labelledby`）仍 deferred；
  KEYBOARD POLISH（arrow-key nav / Escape-to-clear /
  Tab-within-panel）仍 deferred。
  在 `tests/integration/m4_dom_contract_test.cpp` 加
  一個新的 integration assertion（test F）：canonical
  scenario 在 `provinces.svg` 與 `map.html` 兩個
  artefact 都有 6 個 `role="button"`（3 個 province ×
  circle+text）、每個 province 在每個 artefact 都有
  2 個 composed `aria-label`（"Berlin, Germany" /
  "Paris, France" / "Tokyo, Japan"）；3 個 legend
  swatch 都不帶 `role` / `aria-label` / `tabindex`；
  M4.16 `:focus-visible` + `#1976d2` CSS 只出現在
  `map.html`、`provinces.svg` 完全沒有；以及仍 deferred
  的 ARIA surface 在兩個 artefact 都絕對沒出現。
  這條 test 是 M4.15/M4.16/M4.17 svg_export_test
  單元測試的 end-to-end 對應，透過實際 runner /
  canonical fixture 路徑驗證。**M4 仍在 in progress**
  ── 沒有寫 `docs/milestone-4-result.md`，M4.18 是
  checkpoint refresh 不是 exit。**renderer bytes 與
  M4.17 完全相同** ── 這次只加 docs + 1 個
  integration test。**Artefact 數量不變（仍 10）；
  save 格式不變（仍 v12）**；M1.17 / M2.22 / M3.7
  byte-identical determinism contract 仍 by construction
  通過。2 個新 doctest cases（共 880）。
  **M4 in progress.**
  **M4.18 不做** 新 system / 新 formula / 新 artefact
  / save schema bump / 新 state field / 新 fixture
  / 新 `InterestGroupKind` / `PlayerCommandKind` / 新
  feature surface（M4.18 只動 docs + 1 個 integration
  test）/ rename 任何 data-* attribute / 動
  render_svg_root / render_map_html bytes / 更廣的
  ARIA / M4.15 之外的 keyboard polish / `<meta
  name="viewport">` / CSS animation / transition /
  media query / 鄰接 edge / terrain / overlay /
  events / AI / commands / hover / tooltip /
  selection 持久化 / runner CLI flag / atomic
  `end_tick` write / M4 close-out /
  `docs/milestone-4-result.md` / 「M4 closed」字樣 /
  動 `provinces.svg` 或 `map.html` bytes。
  **M4.17（ARIA labels skeleton）** 讓 M4.15-focusable /
  M4.10-clickable 的 province markers **可以被 screen
  reader 讀出來**。`render_svg_root` 現在會在每個
  `<circle>` 與每個 `<text>` 上 emit 兩個新屬性：
  `role="button"`（告訴 assistive tech「這是一個可
  互動的 button」── 與 click + Enter/Space 的觸發模型
  一致）+ `aria-label="<name>, <owner_name>"`（給沒有
  文字內容的 `<circle>` 一個可讀名稱；給 `<text>` 一個
  一致名稱，所以兩個 sibling 不管哪一個 focus 都會聽到
  同樣的播報）。aria-label 在 render time 組合：
  owner 解析得到時用 `<name>, <owner_name>`、owner
  無效時 fallback 成 `<name>`（不加尾逗號 ── 避免螢幕
  閱讀器讀成「Ghost, 逗號」）；組合後的整個字串以單一
  值走 M4.2 `xml_attr_escape`，所以名稱裡有
  `& < > " '` 不會破壞屬性語法。fallback 的判定與
  `data-owner-code` / `data-owner-name` 共用同一個
  bounds check，三者不可能對「owner index 是否有效」
  產生不一致的看法。兩個屬性都同時掛在 circle 與 text
  上，沿用 M4.8 / M4.13 uniform identity surface 模式。
  屬性都在 `render_svg_root` 裡 emit，所以 standalone
  `provinces.svg` 也會帶；`map.html` 裡 legend swatch
  `<circle>` 元素是另外 emit 的（在 `render_map_html`
  裡），**刻意** 不帶 `role` 也不帶 `aria-label`，
  維持單純裝飾元素的角色。**M4.17 narrowly REVERSES
  了 M4.15/M4.16 的「no ARIA」非目標** ── 只動
  `role="button"` 與 `aria-label`；更廣的 ARIA surface
  （`aria-selected` / `aria-current` / `aria-pressed`
  / `aria-live` / `aria-describedby` /
  `aria-labelledby`）仍然 deferred，留給未來專門的
  A11Y sub-milestone。為配合這個變化，M4.15/M4.16 的
  「NO ARIA polish」單元測試 retune ── 原本廣域的
  `role=` / `aria-label=` 缺席斷言改成只斷言上面那組
  更窄的「仍然 deferred」ARIA 屬性缺席。`role="button"`
  的選擇有特定推理：`role="link"` 拒絕（handler 不
  navigate）；`role="option"` 拒絕（會需要 `role=
  "listbox"` 加 arrow-key 導覽加 `aria-activedescendant`，
  超出 scope）；不加 role 拒絕（focus 時會被讀成
  純圖形）。aria-label 的值與 M4.10/M4.11 details
  panel 裡 `Province Name` / `Owner Name` 兩列渲染的
  內容一致，所以看得見的使用者與聽得見的使用者
  獲得同一個節點 identity。M4.10 的 XSS-safe DOM API、
  no-network discipline、「`map.html` 有且只有一段
  inline `<script>`；`provinces.svg` 完全沒有 script」
  這條非對稱 invariant、M4.12 transient `.selected`
  機制、M4.13 五個 data-* DOM contract、M4.15
  `tabindex` + keydown handler、M4.16 `:focus-visible`
  rings 全部沿用，完全 additive。**M4 仍在 in
  progress** ── 沒有寫 `docs/milestone-4-result.md`，
  M4.17 只是再多一個 skeleton sub-milestone，不是
  exit。**save 格式仍 v12** ── aria-label 從既有的
  `ProvinceNode` 與 `state.countries` 欄位 derive，
  不是新的 persistent state field。`provinces.svg`
  bytes 有變（每個 `<circle>` + `<text>` 多兩個屬性
  ── additive only）；`map.html` bytes 有變（同一份
  SVG body）。**Artefact 數量不變（仍 10）；save
  格式不變（仍 v12）**；M1.17 / M2.22 / M3.7
  byte-identical determinism contract 仍 by construction
  通過。7 個新 doctest cases（共 878）。
  **M4 in progress.**
  **M4.17 不做** `aria-selected` / `aria-current` /
  `aria-pressed` / `aria-live` / `aria-describedby` /
  `aria-labelledby` / 任何其他 role 值（只允許
  `"button"`）/ `<title>` / `<desc>` 子元素 / state
  mutation / commands / AI / events / selection
  持久化 / tooltip / hover / animation / panel 快捷鍵
  / save schema bump / 新 state field / 新 artefact /
  新 fixture / 新 `InterestGroupKind` /
  `PlayerCommandKind` / rename M4.8 / M4.13 data-* key
  / 第二個 `<script>` / `<script src=>` /
  `<script type=>` / `<link>` / 外部 CSS / font /
  `<iframe>` / `<img>` / `fetch` / `XMLHttpRequest` /
  `localStorage` / `sessionStorage` /
  `history.pushState` / `window.location` /
  `navigator` / `innerHTML` / `outerHTML` /
  `document.write` / `eval` / `Function` / inline
  event attribute / per-element inline `style="..."` /
  `<meta name="viewport">` / CSS animation /
  transition / media query / `@import` / `@font-face`
  / 鄰接 edge / terrain / overlay / runner CLI flag /
  M4 close-out / `docs/milestone-4-result.md` /
  「M4 closed」字樣。
  **M4.16（focus-visible styling skeleton）** 讓 M4.15 的鍵盤
  focus 看得見。**純 CSS** ── 沒有改 JavaScript，也沒有
  改 markup 結構（只動 `<style>` block）。M4.6 `<style>`
  block 多四條規則：
  `svg circle:focus { outline: none; }`、
  `svg circle:focus-visible { outline: none; stroke:
  #1976d2; stroke-width: 4; }`、
  `svg text:focus { outline: none; }`、
  `svg text:focus-visible { outline: 2px solid #1976d2;
  outline-offset: 2px; }`。兩條 bare `:focus { outline:
  none; }` 是用來壓掉瀏覽器預設的 focus outline，讓
  M4.16 的樣式能夠勝出；兩條 `:focus-visible` 才是真正
  的視覺指示。**選 `:focus-visible` 而不是 bare
  `:focus`** 是關鍵 ── `:focus-visible` 只在「瀏覽器
  認為這個 focus 應該被視覺化」時觸發（典型情況：鍵盤
  Tab）；滑鼠點擊獲得的 focus **不會** 觸發，所以
  滑鼠點擊只會顯示 M4.12 的 `.selected` 黑色 stroke，
  鍵盤 Tab 只會顯示 M4.16 的 `#1976d2` 藍色 ring，
  鍵盤 Enter/Space 觸發 activate 則兩者同時顯示
  （正確：「這個被選中了」+「這個目前還有 focus」）。
  顏色 `#1976d2`（Material Blue 700）刻意挑來與 M4.3
  owner palette 以及 M4.12 `.selected` 黑色 stroke
  做區隔。circle 用 stroke 做 ring（沿形狀外緣）；
  text 用 CSS `outline` + `outline-offset` 做矩形 ring。
  M4.10 的 XSS-safe DOM API、no-network discipline、
  「`map.html` 有且只有一段 inline `<script>`；
  `provinces.svg` 完全沒有 script」這條非對稱 invariant、
  M4.12 transient `.selected` 機制、M4.13 五個 data-*
  DOM contract、M4.15 `tabindex` + keydown handler 全部
  沿用，完全 additive。**仍然不做 ARIA 大改** ──
  `role=` / `aria-label=` / `aria-selected=` /
  `aria-current=` / `aria-pressed=` 全都不出現
  （tests 釘住這點）。這留給未來專門的 A11Y
  sub-milestone。**M4 仍在 in progress** ── 沒有寫
  `docs/milestone-4-result.md`，M4.16 只是再多一個
  skeleton sub-milestone，不是 exit。`provinces.svg`
  bytes 與 M4.15 完全相同 ── focus CSS 只在 HTML
  wrapper 裡；`map.html` bytes 有變（多了四條 CSS rule）。
  **Artefact 數量不變（仍 10）；save 格式不變（仍 v12）**；
  M1.17 / M2.22 / M3.7 byte-identical determinism contract
  仍 by construction 通過。5 個新 doctest cases（共 871）。
  **M4 in progress.**
  **M4.16 不做** state mutation / commands / AI / events /
  selection 持久化 / tooltip / hover / animation /
  transition / `:focus-visible` polyfill / save schema
  bump / 新 state field / 新 artefact / 新 fixture / 新
  `InterestGroupKind` / `PlayerCommandKind` / rename
  M4.8 / M4.13 data-* key / 第二個 `<script>` /
  `<script src=>` / `<script type=>` / `<link>` / 外部
  CSS / font / `<iframe>` / `<img>` / `fetch` /
  `XMLHttpRequest` / `localStorage` / `sessionStorage` /
  `history.pushState` / `window.location` / `navigator` /
  `innerHTML` / `outerHTML` / `document.write` / `eval`
  / `Function` / inline event attribute / per-element
  inline `style="..."` / `<meta name="viewport">` / 鄰接
  edge / terrain / overlay / runner CLI flag / 動
  `provinces.svg` bytes / M4 close-out /
  `docs/milestone-4-result.md` / 「M4 closed」字樣。
  **M4.15（keyboard focus accessibility skeleton）** 是 M4 viewer
  的**第一個鍵盤輸入面**。`render_svg_root` 現在會在每個
  `<circle>` 與 `<text>` 上 emit `tabindex="0"`，所以
  province marker 進入正常 tab order，standalone
  `provinces.svg` 也會跟著拿到這個屬性；`map.html` 內 inline
  `<script>` 在既有 `click` listener 旁邊再多註冊一個
  `keydown` listener ── 當 `event.key === "Enter"` 或
  `event.key === " "` 時，先呼叫 `event.preventDefault()`
  （抑制 Space 預設往下捲動），然後執行與 click 完全相同
  的 `selectProvince + showDetails` 配對。click 與 keydown
  共用每個元素的 `activate()` closure，所以兩種輸入模式不
  可能在效果上飄移。M4.7 legend swatch `<circle>` 元素是在
  `render_map_html` 裡 emit（不在 `render_svg_root`），沒有
  `tabindex`，所以維持不在 tab order；既有的 M4.10 selector
  `svg circle[data-id], svg text[data-id]` 也早就把它們排
  除掉了。**明確 non-goal：完全不做 ARIA 大改** ── 沒有
  `role=` / `aria-label=` / `aria-selected=` / `aria-current=`
  / `aria-pressed=`、沒有 `:focus` / `:focus-visible` CSS
  （用瀏覽器預設 focus ring）、`tabindex` 只允許 `"0"`（不
  做 programmatic-only focus、不做 manual 排序）、沒有 panel
  快捷鍵、沒有 skip-link、沒有跨 render 的 focus 復原。這些
  全部留給未來專門的 A11Y sub-milestone。M4.10 的 XSS-safe
  DOM API（`createElement` + `textContent` only；沒有
  `innerHTML` / `outerHTML` / `document.write` / `eval` /
  `Function`）、no-network discipline（沒有 `fetch` /
  `XMLHttpRequest`）、「`map.html` 有且只有一段 inline
  `<script>`；`provinces.svg` 完全沒有 script」這條非對稱
  invariant、M4.12 的 transient `.selected` 機制、以及
  M4.8 + M4.13 的五個 data-* DOM contract 全部沿用，
  完全 additive。**M4 仍在 in progress** ── 沒有寫
  `docs/milestone-4-result.md`，M4.15 只是再多一個 skeleton
  sub-milestone，不是 exit。**save 格式仍 v12** ── 
  `tabindex` 是 render-time only，不是 `ProvinceNode` 上
  的新欄位。`provinces.svg` bytes 有變（每個 `<circle>` +
  `<text>` 多了一個 `tabindex="0"` ── additive only）；
  `map.html` bytes 有變（同一份 SVG body + 重構過的
  listener loop + 新的 keydown 線路）。**Artefact 數量
  不變（仍 10）；save 格式不變（仍 v12）**；M1.17 / M2.22
  / M3.7 byte-identical determinism contract 仍 by
  construction 通過。9 個新 doctest cases（共 866：6 個
  svg_export + 1 個 integration test E + 2 個 standalone-SVG
  / cross-check）。**M4 in progress.**
  **M4.15 不做** state mutation / commands / AI / 鍵盤
  emit event / selection 持久化 / 多選 / shift-Enter /
  右鍵 / hover / tooltip / animation / save schema bump
  / 新 state field / 新 artefact / 新 fixture / 新
  `InterestGroupKind` / `PlayerCommandKind` / rename
  M4.8 / M4.13 data-* key / 第二個 `<script>` /
  `<script src=>` / `<script type=>` / `<link>` / 外部
  CSS / font / `<iframe>` / `<img>` / `fetch` /
  `XMLHttpRequest` / `localStorage` / `sessionStorage` /
  `history.pushState` / `window.location` / `navigator`
  / `innerHTML` / `outerHTML` / `document.write` /
  `eval` / `Function` / inline event attribute（用
  `addEventListener` 取代）/ per-element inline
  `style="..."` / `<meta name="viewport">` / CSS
  animation / transition / media query / `@import` /
  `@font-face` / 鄰接 edge / terrain / overlay /
  runner CLI flag / M4 close-out /
  `docs/milestone-4-result.md` / 「M4 closed」字樣。
  **M4.14（DOM contract checkpoint refresh）** 對應 M4.9 在當時
  的角色：**零新行為**，只刷新狀態快照 + 補一個小型
  integration assertion。`docs/milestone-4-checkpoint.md`
  原本是 M4.9 寫的、釘住 M4.2–M4.8 的 SVG / HTML DOM
  contract；M4.14 把它擴充到覆蓋 M4.10（`map.html` 第一段
  inline `<script>`；非對稱 JS 邊界 ── `provinces.svg`
  仍完全沒有 script）、M4.11（details panel 的 `<dt>` 標籤
  從 raw `data-*` key 解耦成 `Province ID` / `Owner Index`
  / `Owner Code` / `Owner Name`（M4.13）/ `Province Name`
  共五個 human-readable label）、M4.12（transient
  `.selected` class + `circle.selected` / `text.selected`
  CSS + `selectProvince(el)` helper ── 純粹 DOM 層級，
  不持久化），以及 M4.13（每個 `<circle>` 與 `<text>`
  多帶第五個 `data-owner-name` 屬性，內容從
  `state.countries[owner].name` 解析）。刷新後的 checkpoint
  doc 列出 `<style>` block 裡 13 條 selector（M4.9 時是
  6 條）、一個新的 interactivity-surface 區塊枚舉
  `<div id="details">` / `.selected` / `selectProvince`
  / `showDetails` / 5 列 `fields` 陣列，以及把 deferred
  items 依 HOVER+TOOLTIPS / KEYBOARD+A11Y / PERSISTENT
  SELECTION / DOM EXTENSIONS / VISUAL POLISH /
  INFRASTRUCTURE 六大類重新整理。在
  `tests/integration/m4_dom_contract_test.cpp` 加一個
  新的 integration assertion（test D）：canonical
  `map.html` 的 inline `<script>` 必須帶 M4.13 之後的
  五列 `fields` 陣列 ── 五個 raw `data-*` 屬性名稱與
  五個 human-readable label 都必須在 script body 裡出現；
  `provinces.svg` 則完全沒有任何 JS literal 形式。這條
  test 是 M4.11/M4.13 svg_export_test 單元測試的
  end-to-end 對應，透過實際 runner / canonical fixture
  路徑驗證。**M4 仍在 in progress** ── 沒有寫
  `docs/milestone-4-result.md`，M4.14 是 checkpoint
  refresh 不是 exit。**renderer bytes 與 M4.13 完全相同**
  ── 這次只加 docs + 1 個 integration test。
  **Artefact 數量不變（仍 10）；save 格式不變（仍 v12）**；
  M1.17 / M2.22 / M3.7 byte-identical determinism contract
  仍 by construction 通過。1 個新 doctest case（共 857）。
  **M4 in progress.**
  **M4.14 不做** 新 system / 新 formula / 新 artefact /
  save schema bump / 新 state field / 新 fixture / 新
  `InterestGroupKind` / `PlayerCommandKind` / 新 feature
  surface（M4.14 是 docs + 1 integration test 而已）/
  rename 任何 data-* attribute / 動 click handler /
  details panel / `.selected` CSS / `fields` array 的
  bytes / `<meta name="viewport">` / CSS animation /
  transition / media query / 鄰接 edge / terrain /
  overlay / events / AI / commands / hover / tooltip /
  keyboard nav / `aria-*` polish / selection 跨 reload
  持久化 / runner CLI flag / atomic `end_tick` write /
  M4 close-out / `docs/milestone-4-result.md` /
  「M4 closed」字樣 / 動 `provinces.svg` 或 `map.html`
  bytes。
  **M4.13（details panel owner-name polish）** 把 M4.8 的 identity
  surface 再加寬一個屬性，並把 M4.11 details panel 的
  `fields` 陣列再加一列。`render_svg_root` emit 出來的每個
  `<circle>` 與每個 `<text>` 現在都會多帶
  `data-owner-name`，內容從
  `state.countries[owner.value()].name` 解析；當 owner
  index 無效或越界時 emit 空字串（沿用 M4.8
  `data-owner-code` 的防禦性 fallback）。同一個 bounds
  check 同時覆蓋 `data-owner-code` 與 `data-owner-name`
  兩個查詢，這兩個值的「validity 認知」絕不會發散。新
  attribute 值透過 M4.2 `xml_attr_escape` 走完整 XML
  attribute escape（country name 含 `& < > " '` 時不會
  破壞屬性語法）。M4.11 click handler 內的 `fields`
  陣列從四列長到五列，新增
  `{ attr: "data-owner-name", label: "Owner Name" }`，
  details panel 因此 render 出 5 對 dt/dd（`Province
  ID` / `Owner Index` / `Owner Code` / `Owner Name` /
  `Province Name`）。**save 格式仍是 v12** ── 
  `data-owner-name` 是 render 時從 `state.countries`
  **衍生** 出來的，不是 `ProvinceNode` 上的新欄位，所以
  save schema 完全不變。被拒絕的替代路徑是「click
  handler DOM-walk 進 legend，從 `<li>` body 撈出
  country name」── 那會：(1) 把 details panel 與
  legend DOM 結構耦合在一起；(2) 需要解析 `"<id_code>
  &mdash; <name>"` 字串。新增 `data-owner-name` 與
  M4.8 同樣 additive，保證未來 hover / tooltip /
  clickable-UI 子里程碑可以用一行
  `getAttribute("data-owner-name")` 拿到國家名稱，
  future DOM surface 保持一致。M4.10 的 XSS-safe DOM
  API（`createElement` + `textContent` only；沒有
  `innerHTML` / `outerHTML` / `document.write` /
  `eval` / `Function`）、no-network discipline、「`map.html`
  有且只有一段 inline `<script>`；`provinces.svg` 完全沒有
  script」這條非對稱 invariant、以及 M4.12 的 transient
  `.selected` 機制全部沿用；M4.8 既有的四個 data-* key
  **沒有** rename（只 additive 加第五個）。**M4 仍在
  in progress** ── 沒有寫
  `docs/milestone-4-result.md`，M4.13 只是再多一個
  additive 的 surface 擴張，不是 exit。`provinces.svg`
  bytes 有變（每個 `<circle>` + `<text>` 多出一個屬性
  ── additive only；沒有刪除舊屬性，也沒有像素位移）；
  `map.html` bytes 有變（同一份 SVG body + 新的第五個
  `fields` 條目）。**Artefact 數量不變（仍 10）；save
  格式不變（仍 v12）**；M1.17 / M2.22 / M3.7
  byte-identical determinism contract 仍 by construction
  通過。8 個新 doctest cases（共 856）。
  **M4 in progress.**
  **M4.13 不做** 新增 `ProvinceNode` 欄位 / save schema
  bump / 新 state field / 新 artefact / 新 fixture / 新
  `InterestGroupKind` / `PlayerCommandKind` / rename
  M4.8 的 data-* DOM contract key / state mutation /
  commands / AI / 點擊 emit event / selection persistence
  / 多選 / 右鍵 / hover / tooltip / keyboard navigation
  / focus ring / `aria-*` polish / animation / 第二個
  `<script>` / `<script src=>` / `<script type=>` /
  `<link>` / 外部 CSS / font / `<iframe>` / `<img>` /
  `fetch` / `XMLHttpRequest` / `localStorage` /
  `sessionStorage` / `history.pushState` /
  `window.location` / `navigator` / `innerHTML` /
  `outerHTML` / `document.write` / `eval` / `Function` /
  inline event attribute / per-element inline
  `style="..."` / `<meta name="viewport">` / CSS
  animation / transition / media query / `@import` /
  `@font-face` / 鄰接 edge / terrain / overlay /
  runner CLI flag / M4 close-out /
  `docs/milestone-4-result.md` / 「M4 closed」字樣。
  **M4.12（clickable UI selected-state CSS skeleton）** 在 M4.10
  click handler / M4.11 details labels 上再疊一層 transient
  selection highlight。M4.6 `<style>` block 新增兩條 CSS：
  `svg circle.selected { stroke: #000000; stroke-width:
  3; }` 與 `svg text.selected { font-weight: bold; }`。
  click handler 多呼叫一個 `selectProvince(el)` helper：
  先用 `classList.remove("selected")` 清掉前一個 `.selected`
  節點，再用 `classList.add("selected")` 加到所有與被點擊
  元素同 `data-id` 的節點上（點 `<circle>` 或 `<text>` 都會
  把該 province 的整對 element 一起亮起來 ── 正好兌現 M4.8
  「未來 clickable UI 可以對 circle / text 一視同仁地查詢」
  的設計意圖）。實作走預先收集的 `nodes` NodeList、用字串
  比對 `data-id`，**不**讓 attribute value 重新進入
  CSS-selector parser，避免任何 selector escape 風險。初始
  render 完全沒有 `class="selected"`；class 只在 click 時
  才出現。**selection 純粹是 DOM 層級的狀態**：不寫進
  `GameState`、不持久化跨 reload、沒有 `localStorage` /
  `sessionStorage` / cookie / URL fragment ── 重新整理頁面
  一定從「沒有選擇」開始。M4.10 的 XSS-safe DOM API
  （`createElement` + `textContent` only；沒有 `innerHTML`
  / `outerHTML` / `document.write` / `eval` / `Function`）、
  no-network discipline（沒有 `fetch` / `XMLHttpRequest`）、
  以及「`map.html` 有且只有一段 inline `<script>`；
  `provinces.svg` 完全沒有 script」這條非對稱 invariant
  全部沿用，不需要新增或修改既有的 integration test。
  M4.8 `<circle>` / `<text>` 上的 `data-*` DOM contract
  key **沒有** rename。**M4 仍在 in progress** ── 沒有
  寫 `docs/milestone-4-result.md`，M4.12 只是 selection
  surface 的 skeleton，不是 exit。`provinces.svg` bytes
  與 M4.8 完全相同；`map.html` bytes 有變（多了兩條 CSS
  rule + 新的 helper + 延伸的 listener）。**Artefact 數量
  不變（仍 10）；save 格式不變（仍 v12）**；M1.17 /
  M2.22 / M3.7 byte-identical determinism contract 仍 by
  construction 通過。5 個新 doctest cases（共 848）。
  **M4 in progress.**
  **M4.12 不做** state mutation / commands / AI / 點擊
  emit event / selection persistence / 多選 / 右鍵 /
  hover / tooltip / keyboard navigation / focus ring /
  `aria-*` polish / animation / save schema bump / 新
  state field / 新 artefact / 新 fixture / 新
  `InterestGroupKind` / `PlayerCommandKind` / rename
  M4.8 的 data-* DOM contract key / 第二個 `<script>` /
  `<script src=>` / `<script type=>` / `<link>` / 外部
  CSS / font / `<iframe>` / `<img>` / `fetch` /
  `XMLHttpRequest` / `localStorage` / `sessionStorage` /
  `history.pushState` / `window.location` / `navigator`
  / `innerHTML` / `outerHTML` / `document.write` /
  `eval` / `Function` / `className` 字串拼接 /
  `setAttribute("class", ...)` / inline event attribute
  / per-element inline `style="..."` / `<meta
  name="viewport">` / CSS animation / transition / media
  query / `@import` / `@font-face` / 鄰接 edge /
  terrain / overlay / runner CLI flag / 動
  `provinces.svg` bytes / M4 close-out /
  `docs/milestone-4-result.md` / 「M4 closed」字樣。
  **M4.11（clickable UI details labels polish）** 是 M4.10 點擊
  handler 的純 UX 打磨。`<div id="details">` panel 裡每個
  `<dt>` 渲染的標籤，從原本「直接顯示 raw `data-*` 屬性
  名」改成「固定的 human-readable label」，但 `getAttribute`
  仍然查 M4.8 DOM contract 的四個原始 key
  （`data-id` / `data-owner` / `data-owner-code` /
  `data-name` ── **沒有 rename**；`<circle>` / `<text>` 上
  的屬性面與 M4.10 完全 byte-identical），只有 `<dt>`
  body 文字改成 `Province ID` / `Owner Index` /
  `Owner Code` / `Province Name`。renderer 內部只動一段 JS
  literal：`var keys = [...]` → `var fields = [{attr,
  label}, ...]`，並把 `dt.textContent` 從 `keys[i]` 改成
  `f.label`，`getAttribute(f.attr)` 維持原狀。M4.10 的
  XSS-safe DOM API（`createElement` + `textContent` only；
  沒有 `innerHTML` / `outerHTML` / `document.write` /
  `eval` / `Function`）、no-storage / no-network discipline
  （沒有 `fetch` / `XMLHttpRequest` / `localStorage` /
  `sessionStorage` / `history.pushState` /
  `window.location` / `navigator`）、以及「`map.html` 有且
  只有一段 inline `<script>`；`provinces.svg` 完全沒有
  script」這條非對稱 invariant 全部沿用，不需要新增或修改
  既有的 integration test。**M4 仍在 in progress** ── 沒有
  寫 `docs/milestone-4-result.md`，M4.11 只是 UX 打磨，
  不是 exit。`provinces.svg` bytes 與 M4.8 完全相同；
  `map.html` bytes 有變（多了四個 label 字串 + 新的
  `fields` 陣列結構）。**Artefact 數量不變（仍 10）；save
  格式不變（仍 v12）**；M1.17 / M2.22 / M3.7 byte-identical
  determinism contract 仍 by construction 通過。4 個新
  doctest cases（共 843）。**M4 in progress.**
  **M4.11 不做** rename M4.8 的 data-* DOM contract key /
  state mutation / commands / AI / 點擊 emit event /
  selection persistence / 多選 / 右鍵 / hover / tooltip /
  keyboard navigation / focus ring / `aria-*` polish /
  animation / save schema bump / 新 state field / 新
  artefact / 新 fixture / 新 `InterestGroupKind` /
  `PlayerCommandKind` / 第二個 `<script>` / `<script src=>`
  / `<script type=>` / `<link>` / 外部 CSS / font /
  `<iframe>` / `<img>` / `fetch` / `XMLHttpRequest` /
  `localStorage` / `sessionStorage` / `history.pushState`
  / `window.location` / `navigator` / `innerHTML` /
  `outerHTML` / `document.write` / `eval` / `Function` /
  inline event attribute / per-element inline
  `style="..."` / `<meta name="viewport">` / CSS
  animation / transition / media query / `@import` /
  `@font-face` / 鄰接 edge / terrain / overlay /
  runner CLI flag / 動 `provinces.svg` bytes / M4
  close-out / `docs/milestone-4-result.md` / 「M4
  closed」字樣。
  **M4.10（HTML clickable UI skeleton）** 是 `map.html` 第一段
  JavaScript。在 `<body>` 結尾放唯一一個 inline `<script>`
  IIFE，對每個 `svg circle[data-id], svg text[data-id]`
  元素掛一個 `click` listener；listener 透過
  `getAttribute` 讀出 M4.8 的四個 `data-*` 屬性，並用
  `createElement` + `textContent` 把它們渲染成 `<dl>`，
  注入到一個新增的 `<div id="details">` placeholder 裡
  （這個 placeholder 位於 inline SVG 與 M4.7 legend 之間，
  初始內容是一個 `<p class="details-empty">Click a
  province to see its details.</p>`）。第一次點擊會覆蓋
  placeholder，之後每次點擊都會覆蓋前一個 `<dl>`。Handler
  本身 **無狀態 + XSS-safe**：完全沒有 `innerHTML` /
  `outerHTML` / `document.write` / `eval` / `Function`，也
  完全沒有 `fetch` / `XMLHttpRequest` / `localStorage` /
  `sessionStorage` / `history.pushState` /
  `window.location` / `navigator`。selector 故意排除 M4.7
  legend swatch `<circle>` 元素（它們沒帶 `data-id`），
  legend 維持不可點。新增四條 CSS rule（`.details` +
  dl/dt/dd + `.details-empty` + `svg circle[data-id], svg
  text[data-id] { cursor: pointer; }`）寫進 M4.6 的
  `<style>` block，**不在任何元素上加 inline
  `style="..."`** ── M4.6 single-CSS-surface 契約照舊。
  JavaScript 邊界自 M4.10 起變 **不對稱**：
  `provinces.svg` 仍然完全沒有 script、完全 inert；
  `map.html` 只能有 EXACTLY ONE inline script（沒 `src=`、
  沒 `type=`）。M4.9 integration test C 拆成不對稱的
  per-artefact 斷言來保護這條新 invariant；M4.5/M4.6
  「no `<script>`」單元測試重新校準。**M4 仍在 in
  progress** ── 沒有寫 `docs/milestone-4-result.md`，
  M4.10 只是再多一個 skeleton sub-milestone，不是 exit。
  `provinces.svg` bytes 與 M4.8 完全相同；`map.html`
  bytes 有變（新 CSS + placeholder + script）。
  **Artefact 數量不變（仍 10）；save 格式不變（仍 v12）**；
  M1.17 / M2.22 / M3.7 byte-identical determinism contract
  仍 by construction 通過。8 個新 doctest cases（7
  svg_export + 1 runner；共 839）。**M4 in progress.**
  **M4.10 不做** state mutation / commands / AI / 點擊
  emit 任何 event / selection persistence / 多選 /
  右鍵 / hover / tooltip / keyboard navigation / focus
  ring / `aria-*` polish / 任何 animation / save schema
  bump / 新 state field / 新 artefact / 新 fixture / 新
  `InterestGroupKind` / `PlayerCommandKind` / 第二個
  `<script>` / `<script src=>` / `<script type=>` /
  `<link>` / 外部 CSS / font / `<iframe>` / `<img>` /
  `fetch` / `XMLHttpRequest` / `localStorage` /
  `sessionStorage` / `history.pushState` /
  `window.location` / `navigator` / `innerHTML` /
  `outerHTML` / `document.write` / `eval` / `Function` /
  inline event attribute / per-element inline
  `style="..."` / `<meta name="viewport">` / CSS
  animation / transition / media query / `@import` /
  `@font-face` / 鄰接 edge / terrain / overlay /
  runner CLI flag / 動 `provinces.svg` bytes / M4
  close-out / `docs/milestone-4-result.md` / 「M4
  closed」字樣。
  **M4.9（HTML DOM contract checkpoint）** 對應 M3.7 在 M3 反應 loop
  的角色：**零新行為**，只新增三個 integration tests
  （`tests/integration/m4_dom_contract_test.cpp`）和一份
  單頁 snapshot（`docs/milestone-4-checkpoint.md`），正式
  釘住 M4.2–M4.8 累積出來的 SVG / HTML DOM contract，
  作為未來 clickable UI sub-milestone 之前的 checkpoint。
  三條 end-to-end test：(1) 每個 canonical province 在
  `provinces.svg` 與 `map.html` 兩個 artefact 裡的
  `<circle>` 與 `<text>` 上都帶齊四個 data-* 屬性；
  (2) `map.html` legend 與 `state.countries` 一對一對應，
  每行 `<li data-owner="N">` body 內含該國的 id_code；
  (3) no-interactivity invariant ── 兩個 artefact 都沒有
  `<script>` / `<link>` / inline event attribute /
  per-element `style="..."`，而且 `provinces.svg` 額外
  保證沒有 `<style>` block 與 `font-family`。snapshot
  doc 把 artefact 集合、SVG body shape、HTML wrapper
  shape、identity-surface DOM lookups、未來 M4.x 必須
  保留的 invariants、deferred items 全部寫在一個地方，
  讓未來 clickable UI sub-milestone 可以一次看到「現在的
  contract 長什麼樣」，不必拼湊八份 per-sub-milestone
  note。**M4 仍在 in progress** ── 沒有寫
  `docs/milestone-4-result.md`，M4.9 是 checkpoint 不是
  exit report。renderer bytes 與 M4.8 完全相同 ── 這次
  只新增 tests + docs。**Artefact 數量不變（仍 10）；
  save 格式不變（仍 v12）**；M1.17 / M2.22 / M3.7
  byte-identical determinism contract 仍 by construction
  通過。3 個新 doctest cases（共 830）。**M4 in progress.**
  **M4.9 不做** 新 system / 新 formula / 新 artefact /
  save schema bump / 新 state field / 新 fixture /
  JavaScript / `<script>` / `<link>` / inline event
  attribute / per-element inline `style="..."` /
  `<meta name="viewport">` / CSS animation / transition /
  media query / `@import` / `@font-face` / click handler /
  clickable UI / hover / tooltip / state mutation / 鄰接
  edge / terrain / events / AI / command integration /
  runner CLI flag / atomic `end_tick` / M4 close-out /
  `docs/milestone-4-result.md` / 「M4 closed」字樣 / 動
  `provinces.svg` 或 `map.html` bytes。
  **M4.8（HTML static province data attributes skeleton）**
  把 SVG body 裡的 identity surface 加寬：每個 `<circle>` 和每個
  `<text>` 都會帶相同四個 read-only `data-*` 屬性
  （`data-id`、`data-owner`、`data-owner-code`、
  `data-name`），未來 clickable UI / DOM script 可以對兩個
  元素使用一致的查詢方式，不必走兄弟節點。
  `data-owner-code` 從 `state.countries[owner.value()]
  .id_code` 解析，索引無效時回空字串（hand-built state 的
  防禦性 fallback；save / scenario layer 會在 load 時就拒絕
  invalid owner）。`data-name` 把 `<text>` body 的內容也以
  attribute 形式暴露，方便用 `getAttribute` 統一存取。四個
  data-* 值全部走 M4.2 的 `xml_attr_escape`。**這是 M4.4
  之後第一次刻意修改 standalone SVG body 的 sub-milestone**
  ── 改動完全 additive（沒移除舊屬性，沒有任何像素位移）；
  SVG-to-PNG pipeline、vector tool 看到的視覺結果與 M4.7
  完全相同。`provinces.svg` 與 `map.html` 共用
  `render_svg_root`，所以新屬性同時出現在兩個 artefact 裡。
  所有 M4.5 / M4.6 / M4.7 既有的 not 都保留：沒 JavaScript /
  沒 `<script>` / 沒 `<link>` / 沒 inline event attribute /
  沒個別元素的 inline `style="..."` / 沒
  `<meta name="viewport">` / 沒 CSS animation /
  transition / media query / `@import` / `@font-face` /
  沒 click handler / 沒 clickable UI / 沒 hover / 沒
  tooltip / 沒 viewer 寫回 state。**Artefact 數量不變
  （仍 10）；save 格式不變（仍 v12）**；M1.17 / M2.22 /
  M3.7 byte-identical determinism contract 因 same state →
  same attribute values → same bytes 自然繼續成立。8 個新
  doctest cases（7 svg_export + 1 runner；M4.4 empty-name
  test 改錨點對齊 M4.8 新 layout；共 827）。**M4 in
  progress.** **M4.8 不做** JavaScript / click handler /
  event handler / hover / tooltip / state mutation /
  `<script>` / `<link>` / inline event attribute / inline
  `style="..."` / `<meta name="viewport">` / CSS animation
  / transition / media query / `@import` / `@font-face` /
  新 artefact / save schema bump / 新 state field / 新
  `InterestGroupKind` / `PlayerCommandKind` / runner CLI
  flag / events / AI / command integration / ownership
  dynamics / 鄰接 edge / terrain / 新 gameplay / atomic
  `end_tick` / 個別 province 的顏色 override / 新增任何
  `<circle>` / `<text>` 的 presentation attribute（M4.8
  只新增 data-* 屬性，cx / cy / r / fill / x / y /
  text-anchor 都不變）。
  **M4.7（HTML legend skeleton）** 在 `map.html` 內聯 SVG
  之後加入靜態 `<ul class="legend">`，讓觀看者能解讀哪個 palette 顏色
  對應哪個國家。每個 `<li data-owner="N">` 對應
  `state.countries[i]`（vector order），內容是一個 16×16 的
  inline SVG 色塊（顏色取自
  `color_for_owner(CountryId{i})`）加上 `"id_code &mdash;
  name"` 文字。色塊用 inline SVG（而非帶 `background-color`
  的 HTML 元素）保留了 M4.6「個別元素不得有 inline
  `style="..."`」這條 invariant ── `<circle>` 上的 `fill`
  是 SVG presentation attribute，不是 HTML inline style。
  M4.6 既有的三條 CSS rule 加上 M4.7 新增的三條
  （`.legend`、`.legend li`、`.legend .swatch`），只做版面
  排版（去掉預設 bullet、`max-width: 1000px` 置中、flex
  排版讓色塊與文字並排、固定色塊大小）。Legend 文字透過 M4.4
  的 `xml_text_escape` helper 跑 XML-text escape。空的
  `state.countries` 仍輸出空 `<ul>`（沿用 always-present-file
  契約）。**`provinces.svg` bytes 不變** ── legend 只存在
  HTML wrapper 裡，standalone-SVG 路徑保持 CSS-free、
  legend-free，給下游消費者（SVG-to-PNG pipeline、vector
  tool）。所有 M4.5 / M4.6 的 not 都保留：沒有 JavaScript /
  沒有 `<script>` / 沒有 `<link>` / 沒有 inline event
  attribute / 沒有個別元素的 inline `style="..."` / 沒有
  `<meta name="viewport">` / 沒有 CSS animation /
  transition / media query / `@import` / `@font-face`。M4.4
  `<text>` 自己不帶 font-family / font-size 的契約也保留。
  **Artefact 數量不變（仍 10）；save 格式不變（仍 v12）**；
  M1.17 / M2.22 / M3.7 byte-identical determinism contract
  因 same state → same legend bytes → same map.html bytes
  自然繼續成立。9 個新 doctest cases（8 svg_export + 1
  runner；共 819）。**M4 in progress.** **M4.7 不做**
  JavaScript / `<script>` / external stylesheet `<link>` /
  inline event attribute / inline `style="..."` /
  `<meta name="viewport">` / CSS animation / transition /
  media query / `@import` / `@font-face` / click handler /
  clickable UI / hover / tooltip / viewer 寫回 state /
  `<text>` 自己的 font-family / font-size / ownership
  dynamics / 鄰接 edge / terrain / events / AI / command
  integration / 新 `PlayerCommandKind` / runner CLI flag /
  save schema bump / 新 state field / 新 gameplay / atomic
  `end_tick` / 動 `provinces.svg` bytes。
  **M4.6（HTML viewer minimal CSS skeleton）** 在 M4.5 的
  HTML wrapper 裡加入最小可能的 inline `<style>` block，
  三條 CSS selector：`body { margin: 0; padding: 20px; background-color:
  #f0f0f0; }`（中性灰背景 + 少量 padding，讓白色 SVG 卡
  片浮起來）、`svg { display: block; margin: 0 auto;
  border: 1px solid #888; background-color: #ffffff; }`
  （置中 + 卡片式邊框）、`svg text { font-family:
  sans-serif; }`（修掉瀏覽器對 SVG `<text>` 預設 serif 字型，
  讓小 label 更易讀）。`<style>` 在 `<head>` 內以 2-space
  indent 與 `<meta>` / `<title>` 並列。**`provinces.svg`
  bytes 不變** ── CSS 只在 HTML wrapper 裡；standalone-SVG
  路徑保持 CSS-free，讓下游消費者（SVG-to-PNG pipeline、
  vector tool）看到的 bytes 與 M4.5 完全相同。所有 M4.5 的
  not 都保留：沒有 JavaScript、沒有 `<script>`、沒有
  `<link>`、沒有 inline event attribute、沒有
  `<meta name="viewport">`、沒有個別元素的 inline
  `style="..."`。M4.4 `<text>` 自己不帶 font-family /
  font-size 的契約也保留 ── 只有 CSS selector `svg text`
  設字型，而且只影響 HTML viewer。**Artefact 數量不變（仍 10）；
  save 格式不變（仍 v12）**；M1.17 / M2.22 / M3.7
  byte-identical determinism contract 因 same state → same
  CSS bytes → same map.html bytes 自然繼續成立。6 個新
  doctest cases（5 svg_export + 1 runner；M4.5 的 "no
  `<style>`" test 改名並調掉 `<style>` assertion、再加上
  inline-`style=` 檢查；共 810）。**M4 in progress.**
  **M4.6 不做** JavaScript / `<script>` / external
  stylesheet `<link>` / inline event attribute / inline
  `style="..."` / `<meta name="viewport">` / 個別元素的
  CSS override / CSS animation / transition / media
  query / `@import` / `@font-face` / click handler /
  clickable UI / hover / tooltip / viewer 寫回 state /
  legend / `<text>` 元素自己的 font-family / font-size /
  ownership dynamics / 鄰接 edge / terrain / events / AI /
  command integration / 新 `PlayerCommandKind` / runner
  CLI flag / save schema bump / 新 state field / 新
  gameplay / atomic `end_tick` / 動 `provinces.svg` bytes。
  **M4.5（HTML viewer skeleton）** 把 M4.2–M4.4 的 SVG
  body 包進一個最小 HTML5 document（`map.html`），讓地圖
  在瀏覽器打開時不再被當成 raw XML。新公開函式
  `render_map_html(state) → std::string` 與
  `write_map_html(state, path) → Result<bool>`。內部 refactor
  抽出 `render_svg_root` helper，讓 `render_provinces` 保持
  byte-identical（既有 M4.x tests 無需修改就繼續綠燈）。
  HTML 形狀：`<!DOCTYPE html>` + `<html lang="en">` + 最小
  `<head>`（`<meta charset="UTF-8">` + `<title>Leviathan
  Map</title>`）+ `<body>` 內聯 `<svg>` body（不含 XML
  prolog ── 那行在 HTML 裡是 invalid）。沒有 CSS / 沒有
  JavaScript / 沒有 `<style>` / 沒有 `<script>` / 沒有
  `<link>` / 沒有 inline event handler ── M4.5 只做最小
  wrapper。Inline embed（非 external reference），檔案自成
  一格，沒有 `file://` vs `http://` CORS 雷區。`end_tick`
  無條件寫 `map.html`，成為 **第 10 個 artefact**（沿用
  M3.5 / M3.6 / M4.2 unconditional pattern；第二次滿足
  `milestone-3-result.md` §5「增加 artefact 需要獨立
  sub-milestone」規則）。`RunnerOptions::map_html_path`
  optional override（**沒有 CLI flag**），預設
  `<output_dir>/map.html`；`RunOutcome::map_html_path` 記錄
  解析後的 path。M2.9 pre-`end_tick` no-artefact contract
  自然延伸；M3.6 mid-`end_tick` non-transactional 警語也
  延伸（仍是 deferred）。M1 / M2 / M3 integration
  byte-identical determinism contracts 從 9 個 artefact
  延伸到 10 個。**Artefact 數量現在 10；save 格式不變（仍
  v12）；`provinces.svg` bytes 與 M4.4 完全相同。**
  12 個新 doctest cases（7 svg_export + 5 runner；共 804）。
  **M4 in progress.** **M4.5 不做** click handler / clickable
  UI / event handler / hover / tooltip / viewer 寫回 state /
  legend / CSS / `<style>` / JavaScript / `<script>` /
  `<link>` / inline event attribute / `<meta name="viewport">`
  / `<text>` 的 font-family / font-size / ownership
  dynamics / 鄰接 edge / terrain / events / AI / command
  integration / 新 `PlayerCommandKind` / runner CLI flag /
  save schema bump / 新 state field / 新 gameplay / atomic
  `end_tick`。
  **M4.4（SVG labels skeleton）** 在 `provinces.svg` 為每個 `<circle>`
  加上一個 `<text>` label。Label 位置為
  `(cx, cy + kLabelYOffset)`（新公開常數，22.0），
  `text-anchor="middle"`，內容為 XML-text-escape 過的
  `ProvinceNode::name`。新 helper `xml_text_escape`（依
  XML 1.0 §2.4 text-content 規則只 escape `& < >`，跟 M4.2
  針對 attribute context 也 escape `" '` 的
  `xml_attr_escape` 並排）。每個 node 的 `<circle>` 和
  `<text>` 在 byte stream 裡相鄰（per-node interleaved，依
  `state.provinces` 順序）。`<text>` 不指定 `font-family` /
  `font-size` / `fill` ── SVG consumer default 即可，
  typography 留給未來 presentation sub-milestone。空 `name`
  仍會輸出空 body 的 `<text>`，讓 renderer 對 hand-built
  state total（save / scenario layer 本來就拒絕空 name）。
  其他 SVG byte（viewBox、circle 屬性、owner-driven palette、
  `data-id` (XML-escape) / `data-owner` identity、insertion
  order、fixed-precision coord、LF terminator、empty-state
  header-only）與 M4.3 byte 相同。**Artefact 數量不變（仍 9）；
  save 格式不變（仍 v12）**；M1.17 / M2.22 / M3.7
  byte-identical determinism contract 因 same state → same
  label → same byte 自然繼續成立。8 個新 doctest cases
  （7 svg_export + 1 runner；共 792）。**M4 in progress.**
  **M4.4 不做** HTML viewer / clickable UI / event handler /
  hover / tooltip / legend / `font-family` / `font-size` /
  `<text>` fill / label collision detection / per-province
  label override / rich text / 多行 label / ownership
  dynamics / 鄰接 edge / terrain / events / AI / command
  integration / 新 `PlayerCommandKind` / runner CLI flag /
  新 artefact / save schema bump / 新 state field / 新
  gameplay / atomic `end_tick`。
  **M4.3（SVG owner-color skeleton）** 把 M4.2 寫死的
  `fill="black"` 換成
  deterministic 的 per-owner palette lookup。新公開符號
  在 `leviathan::systems::svg_export`：`kOwnerPalette`
  （10 個 hex-RGB string 的 `constexpr std::array<string_view,
  10>`）、`kOwnerPaletteSize`、`kOwnerFallbackFill`（`#888888`）
  以及 `color_for_owner(CountryId) → string_view`。Palette
  以 `owner.value() % kOwnerPaletteSize` 索引（modulo wrap；
  未來只能 append 不能改既有條目，否則會破壞 owner→colour
  對應）；負 owner 走 fallback，讓 renderer 對 hand-built
  state 仍 total（save / scenario layer 本來就拒絕 invalid
  owner）。Canonical GER / FRA / JPN 分別對應 entries
  0 / 1 / 2（steel blue / indian red / goldenrod）。其它
  SVG 屬性 ── viewBox、circle radius、`data-id`（仍 XML-escape）
  、`data-owner`、insertion order、fixed-precision coord、
  LF terminator、empty-state header-only ── 與 M4.2 byte
  完全相同。**Artefact 數量不變（仍 9）；save 格式不變
  （仍 v12）**；M1.17 / M2.22 / M3.7 byte-identical
  determinism contract 因 same state → same colour → same
  byte 自然繼續成立。7 個新 doctest cases（6 svg_export +
  1 runner；共 784）。**M4 in progress.** **M4.3 不做**
  HTML viewer / clickable UI / event handler / hover / label /
  legend / per-province colour override / ownership
  dynamics / 鄰接 edge / terrain / events / AI / command
  integration / 新 `PlayerCommandKind` / runner CLI flag /
  新 artefact / save schema bump / 新 state field / 新
  gameplay / atomic `end_tick`。
  **M4.2（SVG exporter skeleton）** 是 M4 第一個 renderer：新 module
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
