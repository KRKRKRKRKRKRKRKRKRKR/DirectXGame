#pragma once
#include "SceneBase.h"

// ユーザーがエディタの「新規シーン作成」から名前だけ指定して作る、空のシーン。
// OnInitialize/HandleSceneTransitionInputを一切オーバーライドしない、SceneBaseそのまま
// （Unityの新規シーンと同じ：カメラ・光源・GameObjectの配置は全て起動後にエディタ上で行う）。
// TitleScene/PlaySceneのように「1つのC++クラス＝1つの固定シーン名」ではなく、この1つの
// クラスを何個でも異なる名前でSceneRegistryへ動的登録できる（SceneRegistry::RegisterGeneric
// 参照）。Resources/{名前}/scene.jsonが唯一の実体で、このクラス自身は状態を持たない
class GenericScene : public SceneBase {
};
