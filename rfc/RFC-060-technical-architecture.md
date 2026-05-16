# RFC-060：純 C++ 技術架構

- Status: Draft
- Type: TDD
- Scope: 程式架構
- Version: 0.1

## 1. 技術限制

- 純 C++
- 無引擎
- PC
- SVG 優先
- JSON 資料
- deterministic seed
- 可 headless 模擬
- 可輸出 log / CSV / SVG

## 2. 推薦架構

採用「資料集中、系統分離」架構。

```text
GameState
├── Countries
├── Provinces
├── Factions
├── Characters
├── Policies
├── Events
├── DiplomaticRelations
├── Wars
├── Logs
└── RandomState

Systems
├── TimeSystem
├── PolicySystem
├── FactionSystem
├── EconomySystem
├── EventSystem
├── AISystem
├── DiplomacySystem
├── WarSystem
├── IntelligenceSystem
├── MapSystem
├── SaveSystem
└── LoggingSystem
```

## 3. 核心 C++ 類別草案

```cpp
struct GameDate {
    int year;
    int month;
    int day;
};

struct RandomState {
    uint64_t seed;
    uint64_t counter;
};

struct CountryId { int value; };
struct ProvinceId { int value; };
struct FactionId { int value; };
struct PolicyId { int value; };

struct CountryState {
    CountryId id;
    std::string name;
    double gdp;
    double legalTaxBurden;
    double fiscalCapacity;
    double centralControl;
    double corruption;
    double stability;
    double militaryPower;
    double threatPerception;
    std::vector<FactionId> factions;
};

struct FactionState {
    FactionId id;
    CountryId country;
    std::string type;
    double support;
    double influence;
    double radicalism;
};

struct BudgetState {
    double administration;
    double military;
    double education;
    double welfare;
    double intelligence;
    double infrastructure;
    double industry;
};

struct PolicyData {
    PolicyId id;
    std::string name;
    std::vector<std::string> tags;
    int durationDays;
    double adminCost;
};

struct EventLogEntry {
    GameDate date;
    std::string type;
    std::string title;
    std::string publicText;
    std::string debugTruth;
};
```

## 4. Tick 分層

```text
每日：
- 日期推進
- 政策進度
- 事件檢查
- 戰爭簡化更新

每週：
- 派系態度
- 抗議與地方動盪
- 外交壓力

每月：
- GDP
- 稅收
- 預算
- AI 大決策
- 穩定度統計

每年：
- 人口
- 長期產業
- 世界統計
```

## 5. 輸出

必須支援：

- `simulation.log`
- `events.jsonl`
- `country_stats.csv`
- `world_map.svg`
- `savegame.json`

## 6. 測試策略

- deterministic seed 測試
- 10 年短跑
- 70 年長跑
- 100 次 Monte Carlo 平衡測試
- 極端國家狀態測試
- 資料檔 schema 測試

## 7. 非目標

早期不做：

- 複雜 ECS
- GPU 地圖渲染
- 多人連線
- 完整存檔 replay
- 完整 GUI 框架
