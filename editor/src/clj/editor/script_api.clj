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

(defn- ns-name
  [ns name]
  (str ns \. name))

(defmethod convert "TABLE"
  [{:keys [name members]}]
  {name (mapv (comp convert #(assoc % :ns name)) members)})

(defmethod convert "ENUM"
  [{:keys [ns name desc]}]
  (let [name (ns-name ns name)]
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
  (let [name (ns-name ns name)]
    {:type :function
     :name name
     :doc desc
     :display-string (str name (build-param-string parameters))
     :insert-string (str name (build-param-string parameters true))
     :tab-triggers {:select (param-names parameters true) :exit (when parameters ")")}}))

(defn invalid-yaml-error [node-id resource]
  (g/->error node-id nil :fatal nil
             (format "The file '%s' contains invalid YAML data." (resource/proj-path resource))
             {:type :invalid-content :resource resource}))

(g/defnode ScriptApi
  (inherits r/CodeEditorResourceNode)
  (property content-transform g/Any (dynamic visible (g/constantly false)))
  (input consumer-passthrough g/Any)
  (output consumer-passthrough g/Any (gu/passthrough consumer-passthrough))
  (output content g/Any (g/fnk [_node-id content-transform lines resource]
                          (try
                            (content-transform (convert (yp/load (data/lines-reader lines) keyword)))
                            (catch Exception _
                              (invalid-yaml-error _node-id resource))))))

(defn load-script-api
  [project self resource]
  (let [content (slurp resource)
        lines (util/split-lines content)]
    (concat
      (g/set-property self :content-transform identity :editable? true :modified-indent-type (r/guess-indent-type lines))
      (when (resource/file-resource? resource)
        (g/connect self :save-data project :save-data)))))

(defn register-resource-types
  [workspace]
  (workspace/register-resource-type workspace
                                    :ext "script_api"
                                    :label "Script API"
                                    :icon "icons/32/Icons_29-AT-Unknown.png"
                                    :view-types [:code :default]
                                    :view-opts nil ;; TODO
                                    :node-type ScriptApi
                                    :load-fn load-script-api
                                    :read-fn r/read-fn
                                    :write-fn r/write-fn
                                    :textual? true
                                    :auto-connect-save-data? false))


