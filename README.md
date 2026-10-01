# UE-AI-Game-Lab

Claude Code / Codex / KUROUD が、Unreal Engine と Higgsfield API を使い、実際に遊べる小規模ゲームをそれぞれ完成させられるかを検証する実験リポジトリです。

## 最終目的

3つの独立した実験を通して、次を実証します。

1. AI が Unreal Editor に接続できる
2. AI が Level / Actor / Blueprint or C++ / Material / UI / GameMode などを編集できる
3. AI が Higgsfield API を呼び、生成した画像または動画をゲームへ取り込める
4. AI が Play In Editor を実行し、ログや画面を見て問題を発見できる
5. AI が問題を修正し、再テストできる
6. 人間がゲーム内容を手作業で組み立てなくても、最後まで遊べる状態へ到達できる
7. fresh clone から再現できる

対象エージェント:

- Claude Code
- Codex
- KUROUD

ゲーム内容は3本で異なって構いません。ただし、合格条件は共通にします。

## 基本方針

最初から3本を同時に作りません。

まず Unreal MCP の経路そのものを Claude Code で1本通し、次に Codex、最後に KUROUD へ広げます。Claude Code は Epic Games 公式の Unreal Engine Skills / MCP 導線が用意されているため、最初の基準経路にします。

```text
Phase 0  Environment Gate
   ↓
Phase 1  Unreal MCP Baseline
   ↓
Phase 2  Claude Code Game
   ↓
Phase 3  Codex Game
   ↓
Phase 4  KUROUD Game
   ↓
Phase 5  3-Agent Reproducibility Test
   ↓
DONE: AI + UE + Higgsfield でゲーム制作可能と実証
```

詳細は [docs/ROADMAP.md](docs/ROADMAP.md) を参照してください。

合格条件は [docs/ACCEPTANCE_CRITERIA.md](docs/ACCEPTANCE_CRITERIA.md) に固定します。

## 想定ディレクトリ

```text
UE-AI-Game-Lab/
├── docs/
│   ├── ROADMAP.md
│   └── ACCEPTANCE_CRITERIA.md
├── games/
│   ├── claude/
│   ├── codex/
│   └── kuroud/
├── tools/
│   └── higgsfield/
├── results/
│   ├── claude/
│   ├── codex/
│   └── kuroud/
├── .env.example
├── .gitattributes
└── .gitignore
```

Unreal プロジェクト本体は Phase 0 の環境確認後に生成します。UE バージョンを先に固定しないのは、使用する Mac / Xcode / Unreal MCP プラグインの実動作を確認してから最適な組み合わせを決めるためです。

## 人間が担当してよい範囲

Phase 1 以降は、人間の手作業を可能な限り次に限定します。

- Unreal Engine / Xcode 等のインストール
- MCP プラグインの初回有効化
- API キーなど秘密情報のローカル設定
- OS 権限ダイアログへの応答
- AI が物理的に操作できない初回接続作業

ゲーム内容の配置・Blueprint修正・素材Import・バグ修正などは、原則として AI 側に行わせます。
