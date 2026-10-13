# Lumi Stack

Claude Code が Unreal Engine 5 向けに作ったワンタップの積み上げゲームです。

- **遊び方**: 左右からスライドしてくるブロックをタップで落とし、塔を積みます。はみ出した部分は切り落とされて物理演算で落ち、ブロックはどんどん小さくなります。
- **PERFECT**: ぴったり重ねると光のリングが出て、コンボ数に応じて音階が上がります。3コンボ目以降はブロックが少しずつ大きく戻ります。
- **ステージ**: 全5ステージ（DAWN → LAGOON → EMBER → AURORA → STARLIGHT）。ステージをクリアすると空の色が変わり、ブロックが元のサイズに戻り、速度が上がります。
- **勝敗**: 100階に到達すると **SUMMIT REACHED**（クリア）です。ブロックが完全に外れると **GAME OVER** になります。
- **その他**: 最高スコアはセーブされ、iOS では触覚フィードバックがあります。タイトル画面では自動プレイのデモタワーが積まれ続けます。

ブラウザで遊べるプレビュー（同じ C++ コアを WebAssembly 化したもの）は `WebPreview/index.html` にあります。

## 特徴: アセットゼロ、全部コード

`.uasset` / `.umap` は1つもありません。すべて C++ と設定ファイルで構成しています。

| 要素 | 実装 |
| --- | --- |
| レベル | エンジン同梱の空マップ `/Engine/Maps/Entry` を起動し、GameMode が `ALumiStackDirector` を生成 |
| ブロック | `/Engine/BasicShapes/Cube` と `BasicShapeMaterial` の Dynamic Material Instance |
| 背景グラデーション | 実行時に生成した `UTexture2D` を ProceduralMesh の板に貼る（unlit） |
| PERFECT リング | 実行時生成テクスチャの半透明 unlit 板 |
| ライティング | Movable の Directional Light（影あり）と、カメラに追従する Fill の Point Light、マニュアル露出 |
| 効果音 | すべてシンセサイズ（`USoundWaveProcedural`）。音声ファイルなし |
| UI | Slate を C++ で記述（`SLumiStackHUD`）。iPhone のノッチ対応に SafeZone を使用 |
| アイコン / 起動画面 | `Tools/make_ios_art.py` で手続き的に描画 |

これにより、fresh clone した状態から missing asset が起きず、差分もすべてテキストでレビューできます。

## 構成

```text
LumiStack/
├── LumiStack.uproject
├── Config/                     DefaultEngine/Game/Input/Editor.ini（iOS 設定込み）
├── Source/LumiStack/
│   ├── GameCore/LumiCore.*     ★ UE 非依存のゲームルール・配色・効果音シンセ
│   ├── LumiStackDirector.*     UE 側の演出（メッシュ・カメラ・光・物理・音・セーブ）
│   ├── SLumiStackHUD.*         Slate UI
│   ├── LumiStackGameMode.*     Director を spawn、Pawn なし
│   ├── LumiStackPlayerController.*  タッチ / クリック / Space / ゲームパッド A → タップ
│   ├── LumiStackSaveGame.*     ベストスコア
│   └── LumiStackTests.cpp      UE Automation テスト
├── CoreTests/                  ネイティブ単体テスト（UE 不要）
├── WebPreview/                 同じコアを WASM 化したブラウザ版
├── Build/IOS/Resources/        App アイコン（Assets.xcassets）・起動画像
└── Tools/
    ├── mac_verify.sh           Mac でビルドから自動プレイまでを一括検証
    └── make_ios_art.py         アイコン生成
```

ゲームルール（`GameCore`）を UE から切り離しているので、エディタなしでテストできます。UE 版、テスト、ブラウザ版の3つはまったく同じルールのコードから作られます。

## Mac で動かす

前提: Unreal Engine 5.5 以降（Epic Games Launcher でインストール）と Xcode。

```bash
cd games/claude/LumiStack

# 1. 一括検証（単体テスト → C++ ビルド → Automation テスト → 実ゲームを自動プレイで100階まで）
UE_ROOT="/Users/Shared/Epic Games/UE_5.5" ./Tools/mac_verify.sh

# 2. エディタで開く（初回はモジュールのビルドを聞かれるので Yes）
open LumiStack.uproject
#    → ツールバーの Play（PIE）。DefaultEditor.ini で iPhone 比率の縦長ウィンドウになります
```

UE 5.6 / 5.7 を使う場合は、`LumiStack.uproject` を右クリックして「Switch Unreal Engine version」を選ぶか、`EngineAssociation` を書き換えてください。

### デバッグ用コンソール変数

| コマンド | 効果 |
| --- | --- |
| `lumi.AutoPlay 1` | 現在のランを自動で完璧にプレイ |
| `lumi.AutoPlay 2` | 開始とリトライも自動（放置で検証できる） |

ゲームの状態は Output Log の `LogLumiStack` に出ます。終了時には `LUMI_RESULT phase=Won floor=100 score=...` のように1行出力されるので、AI がログを読んで合否を判定できます。

### 調整パラメータ

`ALumiStackDirector` の `EditAnywhere` プロパティで、カメラ距離と角度、光の強さ、露出、ブルーム、背景の明るさ、音量を調整できます。ゲームバランス（速度、PERFECT 判定幅、ステージ構成）は `GameCore/LumiCore.h` の `Tuning` と `LumiCore.cpp` の `kStages` にまとまっています。

## iPhone 実機 / App Store

1. Project Settings → Platforms → iOS で **Team ID** を設定します（`Config/DefaultEngine.ini` の `IOSTeamID=` でも可）。Bundle ID は `com.sunpotflower.lumistack` です。必要なら自分のものに変更してください。
2. パッケージを作成します。

```bash
"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
  -project="$PWD/LumiStack.uproject" -platform=IOS -clientconfig=Shipping \
  -build -cook -stage -pak -iostore -package -archive -archivedirectory="$PWD/Builds" -distribution
```

3. 生成された `.ipa` を Xcode の Organizer または Transporter で App Store Connect にアップロードします。

App Store 向けの設定は済ませてあります: 縦画面固定、60fps、ステータスバー非表示、暗号化の輸出申告（`ITSAppUsesNonExemptEncryption=false`）、1024px アイコン、仮想ジョイスティックの無効化、Safe Area 対応。

残りは人間の作業です: Apple Developer 登録、App Store Connect でのアプリ作成、スクリーンショット、プライバシー申告（データ収集なし）、審査提出。

## 検証状況

| 項目 | 状態 |
| --- | --- |
| ゲームコア単体テスト（g++ / clang++ `-Werror`、ASan+UBSan） | ✅ 45,000+ チェック合格。完璧プレイのボットが100階クリア、1,500回のランダムプレイで構造の不変条件を確認 |
| ブラウザ版（同一コアの WASM）を headless Chromium で起動 | ✅ タイトル、プレイ、ステージクリア、ゲームオーバーを画面で確認 |
| UE C++ のコンパイル / PIE / iOS ビルド | ⚠️ **未実施**。制作したクラウド環境（Linux）に UE がないため。`Tools/mac_verify.sh` で Mac 上で確認してください |

UE 側のコードは別のレビューエージェントが API の正しさを机上でチェックしています。それでも、実際のコンパイルは Mac で初めて行うことになります。エラーが出たら、ローカルの Claude Code でログを渡して修正させるのが最短です（`mac_verify.sh` の build.log）。
