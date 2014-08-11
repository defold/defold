(ns dynamo.scene-editor
  (:require [internal.ui.scene-editor])
  (:import [internal.ui.scene_editor SceneEditor]))

(defn dynamic-scene-editor
  "Create a scene editor. The editor's behavior will depend mostly
   on the node types that have been registered and the nodes in
   the project graph."
  [site file]
  (internal.ui.scene-editor/SceneEditor. (atom {}) file))
