# RFC-070：JSON 資料格式範例

- Status: Draft
- Type: Data Format
- Scope: 原型資料
- Version: 0.1

## 1. 國家資料

```json
{
  "id": "GER",
  "name": "Germany",
  "start_year": 1930,
  "gdp": 900.0,
  "legal_tax_burden": 0.18,
  "fiscal_capacity": 0.72,
  "central_control": 0.78,
  "corruption": 0.18,
  "stability": 0.55,
  "military_power": 0.60,
  "threat_perception": 0.45,
  "factions": [
    "GER_military",
    "GER_bureaucracy",
    "GER_workers",
    "GER_local_elites",
    "GER_media",
    "GER_intelligence"
  ]
}
```

## 2. 派系資料

```json
{
  "id": "GER_military",
  "country": "GER",
  "type": "military",
  "support": 0.50,
  "influence": 0.70,
  "radicalism": 0.25,
  "preferred_budget": {
    "military": 0.35,
    "intelligence": 0.10
  },
  "policy_preferences": {
    "increase_military_budget": 0.8,
    "cut_military_budget": -0.9,
    "press_censorship": 0.2
  }
}
```

## 3. 政策資料

```json
{
  "id": "increase_military_budget",
  "name": "Increase Military Budget",
  "category": "budget",
  "duration_days": 30,
  "admin_cost": 0.1,
  "effects": [
    { "target": "country.military_power", "op": "add", "value": 0.03 },
    { "target": "faction:military.support", "op": "add", "value": 0.08 },
    { "target": "country.threat_perception_export", "op": "add", "value": 0.05 },
    { "target": "faction:workers.support", "op": "add", "value": -0.03 }
  ]
}
```

## 4. 事件資料

```json
{
  "id": "labor_strike",
  "name": "Labor Strike",
  "category": "domestic_unrest",
  "source_type": "newspaper",
  "trigger_conditions": [
    { "field": "country.stability", "op": "<", "value": 0.55 },
    { "field": "faction:workers.support", "op": "<", "value": 0.45 }
  ],
  "weight_modifiers": [
    { "field": "faction:workers.radicalism", "scale": 2.0 },
    { "field": "country.unemployment", "scale": 1.5 }
  ],
  "visible_report": "Rail workers have announced a nationwide strike.",
  "true_cause": "Workers are angry, but local elites may also be funding the strike to resist tax reform.",
  "options": [
    {
      "id": "negotiate",
      "text": "Open negotiations with union leaders.",
      "effects": [
        { "target": "faction:workers.support", "op": "add", "value": 0.05 },
        { "target": "country.stability", "op": "add", "value": 0.02 }
      ]
    },
    {
      "id": "suppress",
      "text": "Authorize police suppression.",
      "effects": [
        { "target": "faction:workers.radicalism", "op": "add", "value": 0.08 },
        { "target": "faction:military.support", "op": "add", "value": 0.03 },
        { "target": "country.stability", "op": "add", "value": -0.04 }
      ]
    }
  ]
}
```

## 5. 省份節點資料

```json
{
  "id": "GER_BERLIN",
  "name": "Berlin",
  "owner": "GER",
  "controller": "GER",
  "population": 4200000,
  "gdp": 80.0,
  "terrain": "urban",
  "resources": [],
  "central_control": 0.85,
  "local_power": 0.20,
  "neighbors": ["GER_BRANDENBURG", "GER_SAXONY"],
  "svg": {
    "type": "circle",
    "cx": 420,
    "cy": 220,
    "r": 8
  }
}
```

## 6. 設定檔範例

```json
{
  "simulation": {
    "start_date": "1930-01-01",
    "end_date": "2000-12-31",
    "daily_tick": true,
    "seed": 19300101
  },
  "debug": {
    "show_hidden_truth": false,
    "write_csv": true,
    "write_event_log": true,
    "export_svg_every_days": 30
  }
}
```
