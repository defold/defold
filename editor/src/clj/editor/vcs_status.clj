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

(ns editor.vcs-status)

(def ^:const change-type-icons
  {:add    "icons/32/Icons_M_07_plus.png"
   :modify "icons/32/Icons_M_10_modified.png"
   :delete "icons/32/Icons_M_11_minus.png"
   :rename "icons/32/Icons_S_08_arrow-d-right.png"})

(def ^:const change-type-styles
  {:add    #{"added-file"}
   :modify #{"modified-file"}
   :delete #{"deleted-file"}
   :rename #{"renamed-file"}})

(defn- text [change]
  (format "%s" (or (:new-path change)
                   (:old-path change))))

(defn- verbose-text [change]
  (if (= :rename (:change-type change))
    (format "%s \u2192 %s" ; "->" (RIGHTWARDS ARROW)
            (:old-path change)
            (:new-path change))
    (text change)))

(defn render [change]
  {:text (text change)
   :icon (get change-type-icons (:change-type change))
   :style (get change-type-styles (:change-type change) #{})})

(defn render-verbose [change]
  {:text (verbose-text change)
   :icon (get change-type-icons (:change-type change))
   :style (get change-type-styles (:change-type change) #{})})
