(ns editor.script-api
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.code.resource :as r]
            [editor.code.util :as util]
            [editor.graph-util :as gu]
            [editor.lua :as lua]
            [editor.resource :as resource]
            [editor.yamlparser :as yp]
            [editor.workspace :as workspace]
            [editor.code.data :as data]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defmulti convert
  "Converts YAML documentation input to the internal auto-complete format defined
  in `editor.lua` namespace."
  {:private true}
  :type)

(defn- name-with-ns
  [ns name]
  (str ns \. name))

(defmethod convert "TABLE"
  [{:keys [name members]}]
  {name (mapv (comp convert #(assoc % :ns name)) members)})

(defmethod convert "ENUM"
  [{:keys [ns name desc]}]
  (let [name (name-with-ns ns name)]
    {:type :variable
     :name name
     :doc desc
     :display-string name
     :insert-string name}))

(defn- param-names
  [params remove-optional?]
  (letfn [(bracketname? [x] (= \[ (first (:name x))))
          (optional? [x] (or (:optional x) (bracketname? x)))]
    (mapv :name
          (if remove-optional?
            (filterv #(not (optional? %)) params)
            (mapv #(if (and (:optional %) (not (bracketname? %)))
                     (assoc % :name (str "[" (:name %) "]"))
                     %)
                  params)))))

(defn- build-param-string
  ([params]
   (build-param-string params false))
  ([params remove-optional?]
   (str "(" (apply str (interpose ", " (param-names params remove-optional?))) ")")))

(defmethod convert "FUNCTION"
  [{:keys [ns name desc parameters]}]
  (let [name (name-with-ns ns name)]
    {:type :function
     :name name
     :doc desc
     :display-string (str name (build-param-string parameters))
     :insert-string (str name (build-param-string parameters true))
     :tab-triggers {:select (param-names parameters true) :exit (when parameters ")")}}))

(defn- load-script-api
  [project self resource]
  (let [content (slurp resource)
        lines (util/split-lines content)]
    (try
      (let [completion-info (reduce merge (mapv convert (yp/load content keyword)))]
        ;; TODO(mags): Put the completion info into the completion-brain-node here!
        ;; Options:
        ;; 1. Put the completion info in a property in this node and connect a cached output of that to the brain.
        ;; 2. Store the completion info in the brain itself. Don't know what to do with updates of files in this case.
        (println "COMPLETION INFO:" completion-info))
      (catch Exception _))
    (when (resource/file-resource? resource)
      (g/connect self :save-data project :save-data))))

(defn register-resource-types
  [workspace]
  (workspace/register-resource-type workspace
                                    :ext "script_api"
                                    :label "Script API"
                                    :icon "icons/32/Icons_29-AT-Unknown.png"
                                    :view-types [:code :default]
                                    :view-opts nil
                                    :node-type r/CodeEditorResourceNode
                                    :load-fn load-script-api
                                    :read-fn r/read-fn
                                    :write-fn r/write-fn
                                    :textual? true
                                    :auto-connect-save-data? false))



