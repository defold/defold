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

(ns editor.portal
  (:require [clojure.core.protocols :as protocols]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data]
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.node]
            [portal.api :as portal.api]
            [portal.viewer :as portal.viewer]
            [util.coll :as coll :refer [pair]]
            [util.defonce :as defonce]
            [util.fn :as fn])
  (:import [clojure.lang Fn]
           [editor.code.data Cursor CursorRange]
           [editor.resource Resource]
           [internal.node NodeTypeRef]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; How to use Portal during editor development
;;
;; Install the relevant Portal plugin for your IDE of choice.
;;   * Cursive: https://plugins.jetbrains.com/plugin/18467-portal-inspector/
;;   * Visual Studio Code: https://marketplace.visualstudio.com/items?itemName=djblue.portal
;;
;; Once installed,
;;   * Use `lein with-profile +portal ...` when starting up to include Portal
;;     on the class path.
;;   * Evaluate the following expression through the REPL connection:
;;     ```
;;     ((requiring-resolve 'editor.portal/open!))
;;     ```
;;     You might want to set up a REPL Command for this in your IDE.

(defonce/protocol Viewable
  (portal-view [value evaluation-context] "Returns a customized Portal view of the value."))

(defn- workspace []
  0)

(defn- project [basis]
  (some-> (ig/explicit-arcs-by-source basis (workspace) :resource-map)
          (first)
          (gt/target-id)))

(defn- proj-path-node-id [^String proj-path evaluation-context]
  (when (and (string? proj-path)
             (< 1 (.length proj-path))
             (= \/ (.charAt proj-path 0)))
    (let [basis (:basis evaluation-context)
          project (project basis)]
      (project/get-resource-node project proj-path evaluation-context))))

(defn- make-resource-view [resource]
  (let [class-name (.getSimpleName (class resource))
        class-keyword (keyword class-name)
        proj-path (resource/proj-path resource)]
    (portal.viewer/pr-str {class-keyword proj-path})))

(defn- resource-view-node-id [resource-view evaluation-context]
  (when (and (map? resource-view)
             (= 1 (count resource-view)))
    (let [[class-keyword proj-path] (first resource-view)]
      (case class-keyword
        (:FileResource :ZipResource) (proj-path-node-id proj-path evaluation-context)
        nil))))

(def no-viewer
  ;; HACK: Height specified to achieve consistent row height with other rows.
  (portal.viewer/hiccup
    [:div {:style {:height 17.5}}]))

(declare ^:private default-viewer ^:private map-viewer viewer)

(extend-protocol Viewable
  Cursor
  (portal-view [cursor _evaluation-context]
    (portal.viewer/pprint cursor))

  CursorRange
  (portal-view [cursor-range evaluation-context]
    (portal.viewer/pprint
      (default-viewer cursor-range evaluation-context)))

  Fn
  (portal-view [fn _evaluation-context]
    (fn/demunged-symbol (.getName (class fn))))

  NodeTypeRef
  (portal-view [node-type _evaluation-context]
    (symbol (:k node-type)))

  Resource
  (portal-view [resource _evaluation-context]
    (make-resource-view resource))

  byte/1
  (portal-view [bytes _evaluation-context]
    (portal.viewer/bin bytes)))

(defn- simple-namespace
  ^String [^String namespace]
  (string/replace namespace #"^.*\." ""))

(defn- simple-node-type-symbol [node-type-kw]
  (when node-type-kw
    (symbol (simple-namespace (namespace node-type-kw))
            (name node-type-kw))))

(defn- arc-source-key [basis arc]
  (let [source-label (gt/source-label arc)
        source-node-id (gt/source-id arc)
        source-node-type-kw (g/node-type-kw basis source-node-id)
        source-node-type-symbol (simple-node-type-symbol source-node-type-kw)]
    (portal.viewer/pr-str
      (pair source-node-type-symbol source-label))))

(defn- arc-target-key [basis arc]
  (let [target-label (gt/target-label arc)
        target-node-id (gt/target-id arc)
        target-node-type-kw (g/node-type-kw basis target-node-id)
        target-node-type-symbol (simple-node-type-symbol target-node-type-kw)]
    (portal.viewer/pr-str
      (pair target-node-type-symbol target-label))))

(declare ^:private default-nav-fn ^:private try-nav-node-id)

(defn- navigable-node-label-map-nav-fn [coll key value]
  (if (nil? key)
    (let [node-id (get (meta coll) :node-id)]
      (assert (g/node-id? node-id))
      (g/with-auto-or-fake-evaluation-context evaluation-context
        (let [node-value (g/node-value node-id value evaluation-context)]
          (viewer node-value evaluation-context))))
    (default-nav-fn coll key value)))

(defn- navigable-node-id-vector-nav-fn [_coll _index value]
  (g/with-auto-or-fake-evaluation-context evaluation-context
    (or (try-nav-node-id value evaluation-context)
        value)))

(def ^:private empty-navigable-node-id-vector
  (-> []
      (with-meta {`protocols/nav navigable-node-id-vector-nav-fn})
      (portal.viewer/inspector)))

(defn- sectioned-map [first-section & more-sections]
  (let [first-section-key (ffirst first-section)
        make-key (cond
                   (keyword? first-section-key) keyword
                   (symbol? first-section-key) symbol
                   :else (throw (IllegalArgumentException. "first-section must be a map with Named keys")))]
    ;; Prefix the keys in each section with an increasing number of zero-width
    ;; spaces so that the keys from later sections are ordered after prior ones.
    (coll/transfer more-sections first-section
      (coll/mapcat-indexed
        (fn [^long section-index section]
          (let [prefix-length (inc section-index)
                prefix (-> (StringBuilder. prefix-length)
                           (.repeat (int \u200B) prefix-length) ; "" (ZERO WIDTH SPACE)
                           (.toString))]
            (map (fn [[key value]]
                   (let [prefixed-name (str prefix (name key))
                         prefixed-key (make-key (namespace key) prefixed-name)]
                     (pair prefixed-key value)))
                 section)))))))

(def ^:private node-view-instructions-hiccup
  [:<>
   [:h3 [:p "Node View"]]
   [:p "This view shows details related to a node in the graph."]
   [:ul
    [:li [:p "Nodes that belong to a resource node will display their owner resource."]]
    [:li [:p "Override nodes will show the chain of original node-ids."]]
    [:li [:p "The properties map shows raw field values, and includes defaults when viewing a base node. When viewing an override node, only overridden values are shown."]]
    [:li [:p "For the inputs and outputs, we show incoming and outgoing connections instead of values."]]
    [:li [:p "Double-click the keyword for a property, input, or output to evaluate it."]]
    [:li [:p "Double-click a node-id anywhere to inspect the corresponding node."]]
    [:li [:p "Double-click a resource or proj-path anywhere to inspect its resource node."]]]])

(defn- can-nav-node-id? [value evaluation-context]
  (and (g/node-id? value)
       (g/node-exists? (:basis evaluation-context) value)))

(defn- try-nav-node-id [node-id evaluation-context]
  (when (g/node-id? node-id)
    (let [basis (:basis evaluation-context)
          node (g/node-by-id-at basis node-id)]
      (when node
        (let [node-type (g/node-type node)
              node-type-view (viewer node-type evaluation-context)
              resource (resource-node/as-resource basis node-id)
              owner-resource (resource-node/owner-resource basis node-id)

              originals
              (into empty-navigable-node-id-vector
                    (butlast (g/override-originals basis node-id)))

              empty-navigable-node-label-map
              (with-meta {} {`protocols/nav navigable-node-label-map-nav-fn
                             :node-id node-id})

              node-view
              (sectioned-map
                (with-meta
                  {'node-id node-id
                   'node-type node-type-view}
                  {`protocols/nav default-nav-fn
                   :portal.runtime/type node-type-view})

                (when-not (coll/empty? originals)
                  {'originals originals})

                (cond
                  (and owner-resource
                       (not= resource owner-resource))
                  {'owner (viewer owner-resource evaluation-context)}

                  resource
                  {'resource (viewer resource evaluation-context)})

                {'properties
                 (-> node
                     (g/own-property-values)
                     (coll/transfer empty-navigable-node-label-map)
                     (map-viewer evaluation-context))}

                {'inputs
                 (as-> empty-navigable-node-label-map target-label->source-key->source-node-id
                       (reduce
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
                         target-label->source-key->source-node-id
                         (sort-by gt/source-id
                                  (gt/arcs-by-target basis node-id)))
                       (reduce
                         (fn [target-label->source-key->source-node-id [input-label]]
                           (update
                             target-label->source-key->source-node-id input-label
                             fn/or
                             no-viewer))
                         target-label->source-key->source-node-id
                         (g/declared-inputs node-type)))

                 'outputs
                 (as-> empty-navigable-node-label-map source-label->target-key->target-node-id
                       (reduce
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
                         source-label->target-key->target-node-id
                         (sort-by gt/target-id
                                  (gt/arcs-by-source basis node-id)))
                       (reduce
                         (fn [source-label->target-key->target-node-id [output-label]]
                           (update
                             source-label->target-key->target-node-id output-label
                             fn/or
                             no-viewer))
                         source-label->target-key->target-node-id
                         (g/declared-outputs node-type)))})]
          (portal.viewer/hiccup
            [:<>
             [::portal.viewer/inspector node-view]
             node-view-instructions-hiccup]))))))

(defn- can-nav-resource? [value evaluation-context]
  (some? (or (resource-view-node-id value evaluation-context)
             (proj-path-node-id value evaluation-context))))

(defn- try-nav-resource [value evaluation-context]
  (some-> (or (resource-view-node-id value evaluation-context)
              (proj-path-node-id value evaluation-context))
          (try-nav-node-id evaluation-context)))

(defn- can-nav-value? [value evaluation-context]
  (or (can-nav-node-id? value evaluation-context)
      (can-nav-resource? value evaluation-context)))

(defn- can-nav-map-entry? [map-entry evaluation-context]
  (or (can-nav-value? (val map-entry) evaluation-context)
      (can-nav-value? (key map-entry) evaluation-context)))

(defn- default-nav-fn [_coll _key value]
  (g/with-auto-or-fake-evaluation-context evaluation-context
    (or (try-nav-node-id value evaluation-context)
        (try-nav-resource value evaluation-context)
        value)))

(defn- map-viewer [coll evaluation-context]
  (as-> coll coll

        ;; Apply the viewer to the keys and values.
        ;; This also works with records, maintaining their type.
        (reduce-kv
          (fn [coll old-key old-value]
            (let [new-key (viewer old-key evaluation-context)
                  new-value (viewer old-value evaluation-context)]
              (cond-> (assoc coll new-key new-value)
                      (not= old-key new-key) (dissoc old-key))))
          coll
          coll)

        ;; Support navigation in case the coll contains navigable values.
        (cond-> coll
                (and (not (contains? (meta coll) `protocols/nav))
                     (coll/any? #(can-nav-map-entry? % evaluation-context) coll))
                (vary-meta assoc `protocols/nav default-nav-fn))))

(defn- coll-viewer [coll evaluation-context]
  (as-> coll coll

        ;; Apply the viewer to the values.
        (coll/transfer coll (coll/empty-with-meta coll)
          (map #(viewer % evaluation-context)))

        ;; Support navigation in case the coll contains navigable values.
        (cond-> coll
                (and (not (contains? (meta coll) `protocols/nav))
                     (coll/any? #(can-nav-value? % evaluation-context) coll))
                (vary-meta assoc `protocols/nav default-nav-fn))

        ;; Use a viewer that allows us to select individual items in case the
        ;; coll is navigable. Portal will use the pprint view for collections
        ;; of scalars by default, which does not allow element selection.
        (cond-> coll
                (contains? (meta coll) `protocols/nav)
                (portal.viewer/inspector))))

(defn- default-viewer [value evaluation-context]
  (cond
    (map? value)
    (map-viewer value evaluation-context)

    (coll? value)
    (coll-viewer value evaluation-context)

    :else
    value))

(defn viewer [value evaluation-context]
  (if (satisfies? Viewable value)
    (portal-view value evaluation-context)
    (default-viewer value evaluation-context)))

(defn submit! [value]
  (g/with-auto-or-fake-evaluation-context evaluation-context
    (portal.api/submit (viewer value evaluation-context))))

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
