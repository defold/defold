(ns editor.json
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.graph-util :as gu]
            [editor.workspace :as workspace])
  (:import [java.io PushbackReader]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def json-grammar
  {:name "JSON"
   :scope-name "source.json"
   ;; indent patterns shamelessly stolen from textmate:
   ;; https://github.com/textmate/json.tmbundle/blob/master/Preferences/Miscellaneous.tmPreferences
   :indent {:begin #"^.*(\{[^}]*|\[[^\]]*)$"
            :end #"^\s*[}\]],?\s*$"}
   :patterns [{:match #"\b(?:true|false|null)\b"
               :name "constant.language.json"}
              {:match #"(?x)-?(?:0|[1-9]\d*)(?:\n(?:\n\.\d+)?(?:[eE][+-]?\d+)?)?"
               :name "constant.numeric.json"}
              {:begin #"\""
               :begin-captures {0 {:name "punctuation.definition.string.begin.json"}}
               :end #"\""
               :end-captures {0 {:name "punctuation.definition.string.end.json"}}
               :name "string.quoted.double.json"}
              {:begin #"/\*"
               :captures {0 {:name "punctuation.definition.comment.json"}}
               :end #"\*/"
               :name "comment.block.json"}
              {:captures {1 {:name "punctuation.definition.comment.json"}}
               :match #"(//).*$\n?"
               :name "comment.line.double-slash.js"}]})

(defonce ^:private json-loaders (atom {}))

(defn- read-then-close [reader]
  (with-open [pushback-reader (PushbackReader. reader)]
    (json/read pushback-reader)))

(def ^:private resource->json (comp read-then-close io/reader))
(def ^:private lines->json (comp read-then-close data/lines-reader))

(g/defnode JsonNode
  (inherits r/CodeEditorResourceNode)
  (property content-transform g/Any (dynamic visible (g/constantly false)))
  (input consumer-passthrough g/Any)
  (output consumer-passthrough g/Any (gu/passthrough consumer-passthrough))
  (output content g/Any (g/fnk [content-transform lines resource editable?]
                          (content-transform (if editable?
                                               (lines->json lines)
                                               (resource->json resource))))))

(defn load-json [_project self resource]
  (let [lines (r/read-fn resource)
        content (lines->json lines)
        [load-fn accept-fn new-content] (some (fn [[_loader-id {:keys [accept-fn load-fn]}]]
                                                (when-some [new-content (accept-fn content)]
                                                  [load-fn accept-fn new-content]))
                                              @json-loaders)]
    (if (some? load-fn)
      (concat
        (g/set-property self :content-transform accept-fn :editable? false)
        (load-fn self new-content))
      (let [indent-type (data/guess-indent-type (take 500 lines) 4)]
        (g/set-property self :content-transform identity :editable? true :indent-type indent-type :lines lines)))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "json"
                                    :label "JSON"
                                    :icon "icons/32/Icons_29-AT-Unknown.png"
                                    :view-types [:code :default]
                                    :view-opts {:code {:grammar json-grammar}}
                                    :node-type JsonNode
                                    :load-fn load-json
                                    :read-fn r/read-fn
                                    :write-fn r/write-fn
                                    :textual? true))

(defn register-json-loader [loader-id accept-fn load-fn]
  (swap! json-loaders assoc loader-id {:accept-fn accept-fn :load-fn load-fn}))
