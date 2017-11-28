(ns editor.code.json
  (:require [editor.code.resource :as r]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def json-grammar
  {:name "JSON"
   :scope-name "source.json"
   :indent {:begin #"^\s*(else|elseif|for|(local\s+)?function|if|while)\b((?!end\b).)*$|\{\s*$"
            :end #"^\s*(elseif|else|end|\}).*$"}
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

(def ^:private json-code-opts {:grammar json-grammar})
(def json-defs [{:ext "json"
                 :label "JSON"
                 :icon "icons/32/Icons_11-Script-general.png"
                 :view-types [:new-code :default]
                 :view-opts json-code-opts}])

(defn register-resource-types [workspace]
  (for [def json-defs
        :let [args (assoc def :node-type r/CodeEditorResourceNode)]]
    (apply r/register-code-resource-type workspace (mapcat identity args))))
