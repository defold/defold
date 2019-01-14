(ns dev
  (:require [dynamo.graph :as g]
            [editor.asset-browser :as asset-browser]
            [editor.changes-view :as changes-view]
            [editor.console :as console]
            [editor.outline-view :as outline-view]
            [editor.prefs :as prefs]
            [editor.properties-view :as properties-view]))

(set! *warn-on-reflection* true)

(def workspace 0)

(defn project []
  (ffirst (g/targets-of workspace :resource-types)))

(defn app-view []
  (ffirst (g/targets-of (project) :_node-id)))

(defn active-resource []
  (->> (g/node-value (project) :selected-node-ids-by-resource-node)
       (keep (fn [[resource-node _selected-nodes]]
               (let [targets (g/targets-of resource-node :node-outline)]
                 (when (some (fn [[_target-node target-label]]
                               (= :active-outline target-label)) targets)
                   resource-node))))
       first))

(defn active-view []
  (some-> (app-view)
          (g/node-value :active-view)))

(defn selection []
  (->> (g/node-value (project) :selected-node-ids-by-resource-node)
       (keep (fn [[resource-node selected-nodes]]
               (let [targets (g/targets-of resource-node :node-outline)]
                 (when (some (fn [[_target-node target-label]]
                               (= :active-outline target-label)) targets)
                   selected-nodes))))
       first))

(defn prefs []
  (prefs/make-prefs "defold"))

(def select-keys-deep #'internal.util/select-keys-deep)

(defn views-of-type [node-type]
  (keep (fn [node-id]
          (when (g/node-instance? node-type node-id)
            node-id))
        (g/node-ids (g/graph (g/node-id->graph-id (app-view))))))

(defn view-of-type [node-type]
  (first (views-of-type node-type)))

(def assets-view (partial view-of-type asset-browser/AssetBrowser))
(def changed-files-view (partial view-of-type changes-view/ChangesView))
(def outline-view (partial view-of-type outline-view/OutlineView))
(def properties-view (partial view-of-type properties-view/PropertiesView))

(defn console-view []
  (-> (view-of-type console/ConsoleNode) (g/targets-of :lines) ffirst))
