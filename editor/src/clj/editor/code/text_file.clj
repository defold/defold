;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.code.text-file
  (:require [dynamo.graph :as g]
            [editor.code.lang.cish :as cish]
            [editor.code.lang.json :as json]
            [editor.code.lang.zig :as zig]
            [editor.code.resource :as r]
            [editor.localization :as localization]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private cish-opts {:code {:grammar cish/grammar}})

(def ^:private json-opts {:code {:grammar json/grammar}})

(def ^:private zig-opts {:code {:grammar zig/grammar}})

(def ^:private text-file-defs
  [{:ext "cpp"
    :language "cpp"
    :label (localization/message "resource.type.cpp")
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "cxx"
    :language "cpp"
    :label (localization/message "resource.type.cpp")
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "C"
    :language "cpp"
    :label (localization/message "resource.type.cpp")
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "cc"
    :language "cpp"
    :label (localization/message "resource.type.cpp")
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "hpp"
    :language "cpp"
    :label (localization/message "resource.type.cpp")
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "h"
    :language "cpp"
    :label (localization/message "resource.type.h")
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "mm"
    :language "objective-c"
    :label (localization/message "resource.type.objective-c")
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "c"
    :language "c"
    :label (localization/message "resource.type.c")
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "json"
    :language "json"
    :label (localization/message "resource.type.json")
    :icon "icons/32/Icons_29-AT-Unknown.png"
    :view-opts json-opts}
   {:ext "manifest"
    :language "yaml"
    :label (localization/message "resource.type.manifest")
    :icon "icons/32/Icons_11-Script-general.png"}
   {:ext "proto"
    :language "proto"
    :label (localization/message "resource.type.proto")
    :icon "icons/32/Icons_11-Script-general.png"}
   {:ext "java"
    :language "java"
    :label (localization/message "resource.type.java")
    :icon "icons/32/Icons_11-Script-general.png"}
   {:ext "py"
    :language "python"
    :label (localization/message "resource.type.py")
    :icon "icons/32/Icons_11-Script-general.png"}
   {:ext "sh"
    :language "shellscript"
    :label (localization/message "resource.type.sh")
    :icon "icons/32/Icons_11-Script-general.png"}
   {:ext "xml"
    :language "xml"
    :label (localization/message "resource.type.xml")
    :icon "icons/32/Icons_29-AT-Unknown.png"}
   {:ext "ini"
    :language "ini"
    :label (localization/message "resource.type.ini")
    :icon "icons/32/Icons_29-AT-Unknown.png"}
   {:ext "plist"
    :language "plist"
    :label (localization/message "resource.type.plist")
    :icon "icons/32/Icons_29-AT-Unknown.png"}
   {:ext "gitignore"
    :language "gitignore"
    :label (localization/message "resource.type.gitignore")
    :icon "icons/32/Icons_29-AT-Unknown.png"}
   {:ext "gradle"
    :language "gradle"
    :label (localization/message "resource.type.gradle")
    :icon "icons/32/Icons_29-AT-Unknown.png"}
   {:ext "defignore"
    :label (localization/message "resource.type.defignore")
    :icon "icons/32/Icons_11-Script-general.png"}
   {:ext "defunload"
    :label (localization/message "resource.type.defunload")
    :icon "icons/32/Icons_11-Script-general.png"}
   {:ext "zig"
    :language "zig"
    :label (localization/message "resource.type.zig")
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts zig-opts}
   {:ext "zon"
    :language "zig"
    :label (localization/message "resource.type.zon")
    :icon "icons/32/Icons_29-AT-Unknown.png"
    :view-opts zig-opts}])

(g/defnode TextNode
  (inherits r/CodeEditorResourceNode))

(defn register-resource-types [workspace]
  (for [def text-file-defs
        :let [args (assoc def
                          :node-type TextNode
                          :view-types [:code :default]
                          :lazy-loaded true)]]
    (apply r/register-code-resource-type workspace (mapcat identity args))))
