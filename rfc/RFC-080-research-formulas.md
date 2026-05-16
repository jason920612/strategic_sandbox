# RFC-080：研究基礎公式框架

- Status: Draft
- Type: Model Notes / Formula RFC
- Scope: 原型公式與長期建模方向
- Version: 0.1

## 1. 重要警告

本文件不是宣稱能準確預測真實政治。這些公式是「有研究基礎的遊戲化近似模型」。

設計目標：

1. 讓公式不是純常識亂猜。
2. 讓變數可解釋。
3. 讓玩家能理解因果。
4. 讓 AI agents 能測試和平衡。
5. 保留可替換性。

## 2. 研究來源方向

可參考的研究框架包括：

- Polity：民主與威權可用多維制度變數衡量，而不是單一標籤。
- V-Dem：民主品質、選舉、自由、參與、審議等可分成多指標。
- Besley & Persson：國家財政能力與法律能力是長期投資結果，並會限制政策選擇。
- Fearon & Laitin：內戰與叛亂風險與國家能力、貧困、人口、地形、政治不穩定、外援等條件相關。
- Collier & Hoeffler：叛亂機會、資源掠奪、經濟條件與 grievances 都會影響衝突。
- Barro：成長與教育、法治、通膨、政府消費、初始收入等因素相關。
- Alesina & Perotti：不平等可提高社會政治不穩定，進而降低投資。

## 3. 稅收公式

### 3.1 v0.1 簡化

```text
Revenue = GDP × LegalTaxBurden × FiscalCapacity × CentralControl × (1 - CorruptionLeakage)
```

### 3.2 變數意義

- `LegalTaxBurden`：法定稅負，不代表實際能收到。
- `FiscalCapacity`：財政能力，代表稅務、登記、審計、徵收能力。
- `CentralControl`：中央對地方的控制。
- `CorruptionLeakage`：腐敗與截留。

### 3.3 科學基礎

Besley & Persson 的國家能力框架指出，財政能力與法律能力是國家透過投資建立的能力，不應假設政府天然能收稅或執行市場規則。

## 4. GDP 成長公式

### 4.1 v0.1 建議

```text
Growth =
BaseGrowth
+ a1 × EducationInvestment
+ a2 × InfrastructureInvestment
+ a3 × RuleOfLaw
+ a4 × IndustryInvestment
- b1 × InflationPressure
- b2 × PoliticalInstability
- b3 × WarDamage
- b4 × Corruption
```

v0.1 若尚未做通膨，可先移除 `InflationPressure`。

### 4.2 科學基礎

Barro 的跨國成長研究常把教育、法治、通膨、政府消費等納入成長因素。這裡不複製真實係數，只採用變數方向。

## 5. 穩定度公式

### 5.1 v0.1 建議

```text
StabilityDelta =
+ c1 × AverageFactionSupport
+ c2 × WelfareSatisfaction
+ c3 × EconomicGrowth
+ c4 × Legitimacy
- d1 × Radicalism
- d2 × InequalityProxy
- d3 × Corruption
- d4 × WarWeariness
- d5 × BudgetCrisis
```

### 5.2 科學基礎

Alesina & Perotti 指出，不平等會透過社會政治不穩定降低投資。遊戲中可把不平等、激進度與不穩定連動，但不要把它設成唯一原因。

## 6. 政變風險公式

```text
CoupRisk =
MilitaryInfluence
× MilitaryDiscontent
× EliteFear
× GovernmentWeakness
× (1 - IntelligenceDetection)
× CrisisMultiplier
```

其中：

```text
GovernmentWeakness = 1 - StateControl
CrisisMultiplier = 1 + WarDefeat + EconomicCrisis + MassProtest
```

### 限制

政變研究非常複雜。v0.1 只作為風險權重，不應直接當機率。

## 7. 內戰／叛亂風險公式

```text
InsurgencyRisk =
LowStateCapacity
× Grievance
× RebelOpportunity
× MobilizationCapacity
× ExternalSupport
```

其中：

```text
LowStateCapacity = 1 - min(FiscalCapacity, AdministrativeCapacity, LocalControl)

Grievance =
EthnicReligiousTension
+ EconomicHardship
+ PoliticalExclusion
+ RepressionBacklash

RebelOpportunity =
RoughTerrain
+ ResourceLootability
+ BorderSanctuary
+ WeakSecurityPresence

MobilizationCapacity =
FactionOrganization
+ Radicalism
+ LocalNetwork
```

### 科學基礎

Fearon & Laitin 強調有利於叛亂的條件，例如貧困、弱國家能力、地形、人口與政治不穩定。Collier & Hoeffler 強調叛亂機會、資源掠奪與經濟條件的重要性。

## 8. 資訊準確度公式

```text
InformationAccuracy =
BaseAccuracy
+ IntelligenceCapacity
+ MediaFreedomSignal
+ BureaucraticProfessionalism
+ AuditCapacity
- Corruption
- FactionCapture
- LeaderIsolation
- LocalAutonomyOpacity
```

報告值可寫成：

```text
ReportedValue = TrueValue + Bias + Noise
```

其中：

```text
Bias = FactionInterestBias + BureaucraticSelfProtection + PropagandaBias
Noise = RandomNormal(0, 1 - InformationAccuracy)
```

## 9. AI 決策效用

```text
Utility(action) =
w_security × SecurityGain
+ w_economy × EconomicGain
+ w_stability × StabilityGain
+ w_faction × FactionSupportGain
+ w_legitimacy × LegitimacyGain
- w_cost × FiscalCost
- w_risk × CrisisRisk
```

AI 不直接選最高效用，而是 softmax 加權選擇：

```text
P(action) = exp(Utility(action) / Temperature) / Σ exp(Utility(i) / Temperature)
```

`Temperature` 越高，AI 越容易做不穩定、誤判或風險行為。

## 10. 外交誤判

```text
Misperception =
(1 - DiplomaticQuality)
+ (1 - IntelligenceAccuracy)
+ LeaderOverconfidence
+ MilitaryOptimismBias
+ DomesticPressure
+ SignalAmbiguity
```

若 `Misperception` 高，AI 可能：

- 高估己方軍力
- 低估盟友背叛
- 低估敵方抵抗
- 誤認對方不會開戰
- 誤判制裁成本

## 11. 參考來源

- Polity Project / Polity IV and Polity5 documentation
- V-Dem Methodology
- Besley, T. & Persson, T. “The Origins of State Capacity: Property Rights, Taxation, and Politics”
- Fearon, J. D. & Laitin, D. D. “Ethnicity, Insurgency, and Civil War”
- Collier, P. & Hoeffler, A. “Greed and Grievance in Civil War”
- Barro, R. “Determinants of Economic Growth: A Cross-Country Empirical Study”
- Alesina, A. & Perotti, R. “Income Distribution, Political Instability, and Investment”
