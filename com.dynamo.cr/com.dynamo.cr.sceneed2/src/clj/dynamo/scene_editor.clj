(ns dynamo.scene-editor
  (:require [dynamo.ui :as ui]
            [dynamo.project :as p]
            [dynamo.file :as f]
            [internal.ui.handlers :as h]
            [internal.ui.scene-editor :as se :refer [->SceneEditor]]))

(set! *warn-on-reflection* true)

(defn find-or-load-node
  [project-state file]
  (let [f (f/project-path project-state file)]
    (if-let [node (first (p/query project-state [[:filename f]]))]
      node
      (do
        (p/load-resource project-state f)
        (first (p/query project-state [[:filename f]]))))))

(defn dynamic-scene-editor
  "Create a scene editor. The editor's behavior will depend mostly
   on the node types that have been registered and the nodes in
   the project graph."
  [project-state site file]
  (let [scene-node   (find-or-load-node project-state file)
        editor-state (atom {:project-state project-state})]
    (->SceneEditor editor-state scene-node)))

