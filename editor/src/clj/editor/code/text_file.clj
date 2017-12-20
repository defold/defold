(ns editor.code.text-file
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.lang.cish :as cish]
            [editor.code.resource :as r]
            [editor.resource :as resource]
            [editor.workspace :as workspace]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private cish-opts {:new-code {:grammar cish/grammar}})

(def ^:private text-file-defs
  [{:ext "cpp"
    :label "C++"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "cxx"
    :label "C++"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "C"
    :label "C++"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "cc"
    :label "C++"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "hpp"
    :label "C++"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "h"
    :label "C/C++ Header"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "mm"
    :label "Objective-C"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "c"
    :label "C"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "manifest"
    :label "Manifest"
    :icon "icons/32/Icons_11-Script-general.png"}
   {:ext "appmanifest"
    :label "App Manifest"
    :icon "icons/32/Icons_11-Script-general.png"}])

(g/defnode TextNode
  (inherits r/CodeEditorResourceNode))

(defn register-resource-types [workspace]
  (for [def text-file-defs
        :let [args (assoc def
                          :node-type TextNode
                          :view-types [:new-code :default])]]
    (apply r/register-code-resource-type workspace (mapcat identity args))))
