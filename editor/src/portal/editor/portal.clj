;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.portal
  (:require [clojure.java.io :as io]
            [editor.code.data]
            [editor.resource :as resource]
            [internal.graph.types]
            [portal.api :as portal.api]
            [portal.viewer :as portal.viewer]
            [util.coll :as coll]
            [util.defonce :as defonce])
  (:import [editor.code.data Cursor CursorRange]
           [editor.resource Resource]
           [internal.graph.types Endpoint]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/protocol Viewable
  (portal-view [value] "Returns a customized Portal viewer for the value."))

(declare default-viewer)

(extend-protocol Viewable
  Cursor
  (portal-view [cursor]
    (portal.viewer/pprint cursor))

  CursorRange
  (portal-view [cursor-range]
    (portal.viewer/pprint
      (default-viewer cursor-range)))

  Endpoint
  (portal-view [endpoint]
    (portal.viewer/pr-str endpoint))

  Resource
  (portal-view [resource]
    (let [class-name (.getSimpleName (class resource))
          class-keyword (keyword class-name)
          proj-path (resource/proj-path resource)]
      (portal.viewer/pr-str {class-keyword proj-path})))

  byte/1
  (portal-view [bytes]
    (portal.viewer/bin bytes)))

(declare viewer)

(defn default-viewer [value]
  (cond
    (map? value)
    (reduce-kv ; This also works with records, maintaining their type.
      (fn [coll old-key old-value]
        (let [new-key (viewer old-key)
              new-value (viewer old-value)]
          (cond-> (assoc coll new-key new-value)
                  (not= old-key new-key) (dissoc old-key))))
      value
      value)

    (coll? value)
    (coll/transfer
      value (coll/empty-with-meta value)
      (map viewer))

    :else
    value))

(defn viewer [value]
  (if (satisfies? Viewable value)
    (portal-view value)
    (default-viewer value)))

(defn submit! [value]
  (portal.api/submit (viewer value)))

;; Add the #'submit! var to the tap set rather than the function to ensure the
;; set won't grow if the function is redefined.
(defonce ^:private -add-tap-
  (add-tap #'submit!))

(defn editor-default-options []
  ;; These temporary files will be present based on the editor integration.
  (cond
    (.exists (io/file ".portal" "emacs.edn"))
    {:launcher :emacs}

    (.exists (io/file ".portal" "intellij.edn"))
    {:launcher :intellij}

    (.exists (io/file ".portal" "vs-code.edn"))
    {:launcher :vs-code}))

(defonce ^:private portal-session-atom (atom nil))

(defn open!
  ([]
   (open! (editor-default-options)))
  ([options]
   (println "Opening Portal with options" options)
   (swap! portal-session-atom portal.api/open options)))

(defn clear! []
  (some-> (deref portal-session-atom)
          (portal.api/clear)))

(defn selected []
  (some-> (deref portal-session-atom)
          (portal.api/selected)))

(defn sel []
  (first (selected)))
