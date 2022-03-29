# my-shell
シェルをc言語で実装  
* ビルトインコマンド（自作コマンド）は外部コマンドを使用しない  
* その他のコマンドは外部コマンドとして実行する

# ビルトインコマンド（自作コマンド）
詳しい仕様は[pdfファイル](https://github.com/oni-97/my-shell/blob/master/specification-builtin-command.pdf)を参照

- ディレクトリの管理機能
  - cd
  - pushd
  - dirs
  - popd
- ヒストリー機能
  - history
  - !!
  - !string &nbsp;&nbsp;(\*stringは１文字以上の任意の文字列)
  - !n &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;(\*nは数字)
  - !-n &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;(\*nは数字)
- ワイルドカード機能
  - \* 
- プロンプト機能
  - prompt
- スクリプト機能
- エイリアス機能
  - alias
  - unalias
- その他機能
  - grep
  - cat
