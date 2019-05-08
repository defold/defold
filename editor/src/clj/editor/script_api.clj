(ns editor.script-api
  (:require [clojure.java.io :as io]
            [editor.lua :as lua]
            [editor.yamlparser :as yp]))

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
  [{:keys [ns name]}]
  (let [name (ns-name ns name)]
    {:type :variable
     :name name
     :display-string name
     :insert-string name}))

(defn- param-names
  [params remove-optional?]
  (mapv :name
        (if remove-optional?
          (filterv #(not= \[ (first (:name %))) params)
          params)))

(defn- build-param-string
  ([params]
   (build-param-string params false))
  ([params remove-optional?]
   (str "(" (apply str (interpose ", " (param-names params remove-optional?))) ")")))

(defmethod convert "FUNCTION"
  [{:keys [ns name parameters]}]
  (let [name (ns-name ns name)]
    {:type :function
     :name name
     :display-string (str name (build-param-string parameters))
     :insert-string (str name (build-param-string parameters true))
     :tab-triggers {:select (param-names parameters true) :exit (when parameters ")")}}))

;; TODO: Remove dev stuff below -------------------------------------

(defn load-webview-yaml
  []
  (yp/load (io/reader "c:/Users/markus.gustavsson/Projects/defold/editor/markus/defedit/1532/webview.script_api") keyword))


(defn convert-webview-yaml
  [yaml]
  (mapv convert yaml)
  ;;(lua/create-code-hint )
  )

#_ (convert-webview-yaml (load-webview-yaml))

#_ (convert {:type "FUNCTION" :ns "hej" :name "plopp" :parameters [{:name "callback",
                                                                    :type "FUNCTION",
                                                                    :desc
                                                                    "A callback which receives info about finished requests."}]})


