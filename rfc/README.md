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
- **M1（進行中）** — 單國內政原型。M1.1 落實 RFC-060 §3
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
- 未落地：RFC-020 完整政治、RFC-030 完整經濟、RFC-040 外交與戰爭、
  RFC-050 事件與隱藏真相、RFC-080 §6 §7 §10 政變 / 內戰 / 誤判公式。

實作 PR 對應的 design notes 全部在 `../docs/`，索引見
`../docs/README.md`。
