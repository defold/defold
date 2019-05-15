(ns editor.script-api
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.code.resource :as r]
            [editor.code.script-intelligence :as si]
            [editor.code.util :as util]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.lua :as lua]
            [editor.resource :as resource]
            [editor.yamlparser :as yp]
            [editor.workspace :as workspace]
            [editor.code.data :as data]
            [editor.resource-node :as resource-node]
            [internal.graph.error-values :as error-values]
            [schema.core :as s]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defmulti convert
  "Converts YAML documentation input to the internal auto-complete format defined
  in `editor.lua` namespace."
  {:private true}
  #(.toUpperCase (:type %)))

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

(defn- lines->completion-info
  [lines]
  (reduce merge (mapv convert (yp/load (data/lines-reader lines) keyword))))

(g/defnk produce-completions
  [lines]
  (try
    (lines->completion-info lines)
    (catch Exception e
      ;; NOTE: We might want to extract better error information from this
      ;; exception so that it is easier to point to the cause of the error
      ;; without parsing in the build error view?
      (error-values/error-warning (.getMessage e)))))

(g/defnode ScriptApiNode
  (inherits r/CodeEditorResourceNode)
  (output completions si/ScriptCompletions produce-completions))

(defn- load-script-api
  [project self resource]
  (concat (g/connect self :completions (project/script-intelligence project) :lua-completions)
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
                                    :node-type ScriptApiNode
                                    :load-fn load-script-api
                                    :read-fn r/read-fn
                                    :write-fn r/write-fn
                                    :textual? true
                                    :auto-connect-save-data? false))

