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
  (:require [clojure.core.protocols :as protocols]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.code.data]
            [editor.resource :as resource]
            [internal.graph.types :as gt]
            [portal.api :as portal.api]
            [portal.viewer :as portal.viewer]
            [util.coll :as coll :refer [pair]]
            [util.defonce :as defonce])
  (:import [editor.code.data Cursor CursorRange]
           [editor.resource Resource]))

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

  Resource
  (portal-view [resource]
    (let [class-name (.getSimpleName (class resource))
          class-keyword (keyword class-name)
          proj-path (resource/proj-path resource)]
      (portal.viewer/pr-str {class-keyword proj-path})))

  byte/1
  (portal-view [bytes]
    (portal.viewer/bin bytes)))

(defn- arc-source-key [basis arc]
  (let [source-node-id (gt/source-id arc)
        source-label (gt/source-label arc)
        source-node-type-kw (g/node-type-kw basis source-node-id)]
    (pair source-node-type-kw source-label)))

(defn- arc-target-key [basis arc]
  (let [target-node-id (gt/target-id arc)
        target-label (gt/target-label arc)
        target-node-type-kw (g/node-type-kw basis target-node-id)]
    (pair target-node-type-kw target-label)))

(declare ^:private try-nav-node-id)

(defn- nav-node-id [_coll _index value]
  (or (try-nav-node-id (g/now) value)
      value))

(def ^:private empty-navigable-node-id-vector
  (-> []
      (with-meta {`protocols/nav nav-node-id})
      (portal.viewer/inspector)))

(defn- try-nav-node-id [basis node-id]
  (when-some [node (g/node-by-id-at basis node-id)]
    (let [node-type (g/node-type node)]
      {:node-id node-id
       :node-type (:k node-type)
       :originals (vec (butlast (g/override-originals basis node-id)))
       :properties (into (sorted-map)
                         (g/own-property-values node))
       :inputs (reduce
                 (fn [target-label->source-key->source-node-id arc]
                   (let [target-label (gt/target-label arc)
                         source-node-id (gt/source-id arc)
                         source-key (arc-source-key basis arc)]
                     (update-in
                       target-label->source-key->source-node-id
                       [target-label source-key]
                       (fn [source-node-ids]
                         (conj (or source-node-ids empty-navigable-node-id-vector)
                               source-node-id)))))
                 (sorted-map)
                 (sort-by gt/source-id
                          (gt/arcs-by-target basis node-id)))
       :outputs (reduce
                  (fn [source-label->target-key->target-node-id arc]
                    (let [source-label (gt/source-label arc)
                          target-node-id (gt/target-id arc)
                          target-key (arc-target-key basis arc)]
                      (update-in
                        source-label->target-key->target-node-id
                        [source-label target-key]
                        (fn [target-node-ids]
                          (conj (or target-node-ids empty-navigable-node-id-vector)
                                target-node-id)))))
                  (sorted-map)
                  (sort-by gt/target-id
                           (gt/arcs-by-source basis node-id)))})))

(defn- default-navigable-value? [basis value]
  (and (g/node-id? value)
       (g/node-exists? basis value)))

(defn- default-nav-fn [_coll _key value]
  (let [basis (g/now)]
    (or (try-nav-node-id basis value)
        value)))

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
    (as-> value coll

          ;; Apply the viewer to the values.
          (coll/transfer coll (coll/empty-with-meta value)
            (map viewer))

          ;; Support navigation in case the coll contains navigable values.
          (cond-> coll
                  (and (not (contains? (meta coll) `protocols/nav))
                       (coll/any? (partial default-navigable-value? (g/now)) coll))
                  (vary-meta assoc `protocols/nav default-nav-fn))

          ;; Use a viewer that allows us to select individual items in case the
          ;; coll is navigable.
          (cond-> coll
                  (contains? (meta coll) `protocols/nav)
                  (portal.viewer/inspector)))

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
