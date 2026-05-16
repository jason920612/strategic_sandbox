# RFC-001：RFC 文件規則與開發契約

- Status: Draft
- Type: Process / Roadmap Standard
- Scope: 文件、任務拆分、AI agents 協作
- Version: 0.1

## 1. 為什麼採用 RFC

本企劃系統極大，不能用單一 GDD 管理。每個大系統都應該有獨立 RFC，並保持以下欄位：

- Status
- Scope
- Goals
- Non-goals
- Data Model
- Simulation Rules
- UI Requirements
- Test Plan
- Open Questions

## 2. 開發拆分層級

### 2.1 大階段

例如：

- Milestone 0：技術骨架
- Milestone 1：單國內政數值原型
- Milestone 2：多國世界模擬
- Milestone 3：SVG 地圖與 UI
- Milestone 4：事件與隱藏真相
- Milestone 5：派系深化
- Milestone 6：外交與世界 AI
- Milestone 7：自動戰爭
- Milestone 8：崩潰、政變與內戰
- Milestone 9：經濟擴展
- Milestone 10：內容擴充與長期沙盒

### 2.2 子階段

例如 Milestone 1：

- 1.1 Country 資料
- 1.2 Faction 資料
- 1.3 Policy 資料
- 1.4 政策執行
- 1.5 派系反應
- 1.6 穩定度計算
- 1.7 經濟月結
- 1.8 事件 log
- 1.9 自動模擬測試

### 2.3 任務級

任務必須可以在 0.5–2 天內完成，且有明確測試。

例如：

- 定義 `PolicyData`
- 實作 `applyPolicyEffects()`
- 寫 5 個政策資料
- 建立 deterministic RNG
- 輸出 10 年模擬 CSV

## 3. AI agents 工作規則

AI agents 可以協助：

- 寫 C++ 模組
- 產生 JSON 資料
- 產生事件文字
- 寫單元測試
- 跑模擬並分析異常
- 產生平衡報告

但每個 AI 輸出的內容必須通過：

1. 編譯
2. 單元測試
3. deterministic seed 測試
4. log 檢查
5. 人類審查

## 4. Definition of Done

每個功能完成必須有：

- 可編譯
- 可測試
- 有 log
- 可用 seed 重現
- 有最小資料案例
- 有失敗案例測試
- 文件更新

## 5. 非目標

早期不做：

- 完整真實 GIS
- 完整軍事戰線
- 完整金融市場
- 完整角色關係網
- 完整歷史事件劇本
- 完整網路多人
