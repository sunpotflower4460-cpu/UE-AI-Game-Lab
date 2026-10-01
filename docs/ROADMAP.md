# Roadmap — AI が UE + Higgsfield でゲームを完成させるまで

## Phase 0 — Environment Gate

目的: 「ゲーム制作」へ進む前に、環境そのものが成立することを確認する。

### 0.1 環境情報を固定する

記録するもの:

- macOS バージョン
- Mac の CPU / RAM
- Unreal Engine バージョン
- Xcode バージョン
- Claude Code バージョン
- Codex バージョン
- KUROUD の commit SHA
- Git / Git LFS バージョン

この情報は後で `results/environment.md` に残す。

### 0.2 Unreal Engine が通常起動することを確認

最初は空の小規模プロジェクトでよい。

合格:
- Editor が起動する
- Project を保存できる
- PIE (Play In Editor) が動く

### 0.3 Unreal MCP の有無を確認

Unreal 側で以下が利用可能か確認する。

- `ModelContextProtocol`
- `AllToolsets`

利用可能なら有効化する。

MCP server を起動し、read-only な問い合わせを1回成功させる。

合格:
- MCP server が起動
- client から接続
- Level 内 Actor 一覧などを取得できる

ここで失敗した場合はゲーム制作へ進まず、UE バージョン / build / plugin 構成を先に解決する。

### 0.4 Git LFS を準備

Unreal の `.uasset` / `.umap` と、必要に応じて生成動画などの大きなバイナリを Git LFS で扱う。

---

# Phase 1 — 共通基盤を1本通す

目的: Claude / Codex / KUROUD の比較より前に「UE を AI から操作する道」が本当に動くことを証明する。

## 1.1 Baseline UE Project を作成

小さな 3D project を1つ作り、次を確認する。

- PlayerStart
- GameMode
- Character / Pawn
- 入力
- 1 Level
- 最小 HUD

この段階ではゲームらしさは不要。

## 1.2 MCP client config を生成

Unreal の MCP server と各 client を接続できる状態にする。

対象:
- Claude Code
- Codex
- KUROUD 用 adapter / runner

Claude と Codex は同じ Unreal MCP endpoint を使い、できるだけ条件差を減らす。

## 1.3 AI に Editor を読ませる

最初の read-only test:

1. 現在の Level 名を読む
2. Actor 一覧を読む
3. PlayerStart の位置を読む
4. Project 設定の一部を読む

次に write test:

1. Cube を1個生成
2. 座標変更
3. Material を割り当て
4. 保存
5. PIE
6. 削除
7. 保存

これが通れば「AI → MCP → Unreal Editor」は成立。

---

# Phase 2 — Higgsfield Integration

目的: Higgsfield を単なる人間向け素材生成サイトではなく、AI の制作パイプラインへ組み込む。

## 2.1 API wrapper

`tools/higgsfield/` に小さな wrapper を作る。

責務:

```text
prompt
  ↓
Higgsfield API
  ↓
job polling
  ↓
download
  ↓
metadata.json
  ↓
UE import-ready asset
```

秘密情報は環境変数のみで扱う。

```text
HF_API_KEY_ID=
HF_API_KEY_SECRET=
```

キーそのものは Git に絶対コミットしない。

## 2.2 最初の生成物

最初は1ゲームにつき1素材だけでよい。

例:
- Title image
- Loading screen
- Character portrait
- Intro movie
- Background image

目的は素材品質の評価ではなく、

`AI → Higgsfield API → file → UE Import → ゲーム内使用`

の完全な経路を成立させること。

---

# Phase 3 — Claude Code Game

最初の本番実験。

理由:
Epic Games 公式の Claude Code 向け Unreal Engine Skills / MCP 導線があるため、UE 側の問題と agent 側の問題を切り分けやすい。

## 3.1 ゲーム仕様

内容は自由。ただし小さくする。

例:

```text
Collect & Exit

- WASD / Gamepad で移動
- 5個のオブジェクトを集める
- 5個集めると出口が開く
- 出口へ入ると CLEAR
- タイトル画面に Higgsfield 生成素材を使用
```

## 3.2 AI-only production loop

```text
仕様を読む
 ↓
実装計画
 ↓
UE MCP で編集
 ↓
保存
 ↓
PIE
 ↓
ログ / 状態 / Screenshot を確認
 ↓
問題抽出
 ↓
修正
 ↓
再PIE
 ↺
```

人間は Blueprint を手直ししない。

## 3.3 証拠を保存

`results/claude/`:

- prompt.md
- run-log.md
- final-state.md
- screenshots/
- higgsfield-metadata.json
- failures.md

---

# Phase 4 — Codex Game

Claude で確立した Unreal 側の基盤を変更せず、Codex を接続する。

重要:
Claude の完成コードや内部手順をそのまま Codex に渡して「答えを見せる」実験にはしない。

共有してよいもの:
- UE project の共通初期テンプレート
- MCP 接続方法
- Acceptance Criteria
- Higgsfield wrapper

共有しないもの:
- Claude がゲームを作った具体的な実装手順
- Claude 固有の修正履歴

Codex のゲーム内容は別でよい。

例:
- 60秒 survival arena
- 敵 spawn
- HP
- attack
- timer
- win / lose
- Higgsfield 素材を最低1つ利用

結果は `results/codex/` に保存。

---

# Phase 5 — KUROUD Game

ここでは単に「別のLLMを使う」のではなく、KUROUD がゲーム制作タスクを完遂する orchestration layer として成立するかを見る。

## 5.1 KUROUD Unreal Capability

理想:

```text
User
 ↓
KUROUD
 ↓
Task decomposition
 ↓
Unreal capability
 ├─ MCP inspect
 ├─ MCP mutate
 ├─ PIE
 ├─ logs
 ├─ screenshot/state inspection
 └─ retry
 ↓
Higgsfield capability
 ↓
Result verification
```

KUROUD が内部で Claude / Codex 等を worker として使うこと自体は許可する。

ただし最終的に評価するのは、

「ユーザーが KUROUD にゲーム制作を依頼した結果、KUROUD が完了まで管理できたか」

とする。

ゲーム例:
- 3D switch puzzle
- 3つの switch
- 正しい順番で扉が開く
- CLEAR
- Higgsfield 素材を最低1つ利用

結果は `results/kuroud/` に保存。

---

# Phase 6 — Autonomous Repair Loop

3 agent 共通で、単発生成ではなく修正ループまで成立させる。

最低限 AI が観測できるもの:

- UE Output Log
- compile error
- Blueprint compile state
- PIE start / stop
- gameplay state
- screenshot または editor state
- asset existence
- map save state

AI は失敗したら、

```text
Observe
 ↓
Hypothesis
 ↓
Change
 ↓
Run
 ↓
Verify
```

を繰り返す。

「コードを書けた」ではなく「動作確認まで完結した」を合格にする。

---

# Phase 7 — Fresh Clone Reproducibility

完成後に最重要の再現テストを行う。

別ディレクトリへ fresh clone:

```bash
git clone https://github.com/sunpotflower4460-cpu/UE-AI-Game-Lab.git
```

ローカル秘密情報だけ再設定。

その状態から各ゲームを開く。

合格:
- asset missing がない
- project が開く
- compile が通る
- PIE 可能
- clear / win / lose 条件まで動く
- Higgsfield 由来 asset が実際に存在する

---

# Phase 8 — 最終判定

3本それぞれについて Acceptance Criteria を判定する。

最終的に次が3/3で成立すれば、本実験の目的を達成したとみなす。

```text
Natural-language request
        ↓
AI agent
        ↓
Unreal MCP
        ↓
Game creation / modification
        ↓
Higgsfield API
        ↓
Generated asset import
        ↓
PIE verification
        ↓
Autonomous repair
        ↓
Playable completed mini-game
```

# 実行順序

現在地からの順番はこれ。

1. Repo scaffold ← 今ここ
2. Mac / UE / Xcode の実環境確認
3. Unreal MCP plugin の存在確認
4. MCP server 起動
5. Claude Code から read-only 接続
6. Claude Code から Cube 生成テスト
7. Baseline UE project を repo に追加
8. Higgsfield API wrapper 作成
9. Higgsfield → UE import test
10. Claude Game 完成
11. Codex Game 完成
12. KUROUD adapter 実装
13. KUROUD Game 完成
14. fresh clone 再現
15. 最終比較レポート

この順序を崩さない。特に 2〜6 が未完了のまま本ゲーム制作へ進まない。
