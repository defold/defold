(ns editor.placeholder-resource
  (:require [dynamo.graph :as g]
            [editor.code.resource :as r]
            [editor.resource :as resource]
            [util.text-util :as text-util]
            [editor.workspace :as workspace]))

(g/defnk produce-save-data [_node-id dirty? resource save-value]
  (cond-> {:dirty? dirty? :node-id _node-id :resource resource :value save-value}
          (some? save-value) (assoc :content (r/write-fn save-value))))

(g/defnk produce-build-targets [_node-id resource]
  (g/error-fatal (format "Cannot build resource of type '%s'" (resource/ext resource))))

(g/defnode PlaceholderResourceNode
  (inherits r/CodeEditorResourceNode)
  (output build-targets g/Any produce-build-targets)
  (output save-data g/Any produce-save-data))

(defn load-node [project node-id resource]
  (if (text-util/binary? resource)
    (g/set-property node-id :editable? false)
    (concat
      (g/set-property node-id :editable? true)
      (when (resource/file-resource? resource)
        (g/connect node-id :save-data project :save-data)))))

(defn view-type [workspace]
  (workspace/get-view-type workspace :code))
