(ns editor.script-api
  (:require [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.code.script-intelligence :as si]
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [editor.yamlparser :as yp]
            [internal.graph.error-values :as error-values]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defmulti convert
  "Converts YAML documentation input to the internal auto-complete format defined
  in `editor.lua` namespace."
  {:private true}
  #(.toUpperCase ^String (:type %)))

(defn- name-with-ns
  [ns name]
  (if (nil? ns)
    name
    (str ns \. name)))

(defmethod convert "TABLE"
  [{:keys [ns name members desc]}]
  (let [name (name-with-ns ns name)]
    (into [{:type :namespace
            :name name
            :doc desc
            :display-string name
            :insert-string name}]
          (mapv (comp convert #(assoc % :ns name)) members))))

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

(defn- convert-lines
  [lines]
  (mapv convert (yp/load (data/lines-reader lines) keyword)))

(defn- combine-conversions
  "This function combines the individual hierarchical conversions into a map where
  all the namespaces are keys at the top level mapping to a vector of their
  respective contents. A global namespace is also added with the empty string as
  a name, which contains a vector of namespace entries to enable auto completion
  of namespace names."
  [conversions]
  (first (reduce
           (fn [[m ns] x]
             (cond
               (vector? x)
               ;; Recurse into sublevels and merge the current map with the
               ;; result. Any key collisions will have vector values so we
               ;; concatenate and turn back into a vector.
               [(merge-with (comp (partial into []) concat) m (combine-conversions x)) ns]

               :else
               (if (= :namespace (:type x))
                 [(let [m (assoc m (:name x) [])
                        m (if-not (= "" ns)
                            (update m ns conj x)
                            m)]
                    ;; Always add namespaces as members of the global namespace.
                    (update m "" conj x))
                  (:name x)]
                 [(update m ns conj x) ns])))
           [{"" []} ""]
           conversions)))

(defn- lines->completion-info
  [lines]
  (combine-conversions (convert-lines lines)))

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

