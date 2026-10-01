# Shared Acceptance Criteria

Claude Code / Codex / KUROUD の3実験すべてに同じ最低条件を適用する。

## A. Connection

- [ ] Unreal Editor が起動している
- [ ] MCP server が起動する
- [ ] agent が Unreal MCP に接続できる
- [ ] agent が現在の Level / Actor を読み取れる

## B. Editor Mutation

- [ ] agent が Actor を作成できる
- [ ] agent が Actor を変更できる
- [ ] agent が asset / Blueprint / C++ のいずれかを作成・編集できる
- [ ] agent が Level を保存できる

## C. Gameplay

- [ ] Player が操作できる
- [ ] 明確な game rule がある
- [ ] game state が変化する
- [ ] CLEAR / WIN / LOSE のいずれかの終了条件がある
- [ ] PIE で終了条件まで到達できる

## D. Higgsfield

- [ ] agent が Higgsfield API 呼び出しを実行する
- [ ] credentials が Git に含まれていない
- [ ] 生成物がローカルへ保存される
- [ ] 生成物が Unreal asset として import される
- [ ] 実際のゲーム画面内で利用される
- [ ] request / model / output 等の metadata が記録される

## E. Verification / Repair

- [ ] agent が PIE を開始できる
- [ ] agent がログまたは editor state を観測できる
- [ ] 意図的または自然発生した不具合を最低1回検出する
- [ ] agent 自身が修正する
- [ ] 修正後に再テストする
- [ ] 最終状態を pass と確認する

## F. Human Intervention

許可:
- 初回インストール
- secret 入力
- OS / macOS の権限操作
- AI から操作不能な初回 MCP 有効化

原則不許可:
- ゲーム用 Blueprint の人間による手修正
- Level の人間による配置調整
- バグを人間が直接直す
- Higgsfield asset を人間が手動 import して成功扱いする

人間の介入が必要になった場合は失敗にせず、必ず `human-interventions.md` に記録する。

## G. Reproducibility

- [ ] clean commit が存在する
- [ ] fresh clone できる
- [ ] 必要手順が README にある
- [ ] secret を再設定すれば project が開く
- [ ] missing asset がない
- [ ] gameplay が再現する

## 判定

各 agent:

- PASS: A〜G の必須項目を満たす
- PARTIAL: playable だが一部で人間介入が必要
- FAIL: playable completion まで到達しない

この実験では「面白さ」や「グラフィック品質」は第一判定に含めない。

第一判定は一貫して、

**AI が UE + Higgsfield を使い、観測・修正・検証まで含めて遊べるゲームを完成できるか**

とする。
