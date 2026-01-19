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

(ns editor.graph-util
  (:require [clojure.set :as set]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.outline :as outline]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [util.coll :as coll]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:dynamic *check-pb-field-names* in/*check-schemas*)

(defmacro passthrough [field]
  `(g/fnk [~field] ~field))

(defn array-subst-remove-errors [arr]
  (filterv (complement g/error?) arr))

(defn explicit-outputs
  ([node-id]
   (explicit-outputs (g/now) node-id))
  ([basis node-id]
   ;; don't include arcs from override-original nodes
   (mapv (fn [[_ src-label tgt-id tgt-label]]
           [src-label [tgt-id tgt-label]])
         (g/explicit-outputs basis node-id))))

(defn connect-existing-outputs [source-node-type source-node-id target-node-id connections]
  (let [existing-source-output-labels (g/output-labels source-node-type)]
    (for [[source-output-label target-output-label] connections
          :when (contains? existing-source-output-labels source-output-label)]
      (g/connect source-node-id source-output-label target-node-id target-output-label))))

(defn node-qualifier-label
  "Given a node-id, returns a string that identifies the node for the user.
  Typically, this will be the URL that uniquely identifies the node inside its
  owner resource, or the id that the user has specified for the node. Returns
  nil if there is no suitable qualifier for the given node."
  ([node-id]
   (g/with-auto-evaluation-context evaluation-context
     (node-qualifier-label node-id evaluation-context)))
  ([node-id {:keys [basis] :as evaluation-context}]
   (when-some [node (g/node-by-id-at basis node-id)]
     (let [node-type (g/node-type node)]
       (or (when (in/behavior node-type :url)
             (let [value (in/node-value node :url evaluation-context)]
               (when (and (string? value)
                          (coll/not-empty value))
                 ;; Convert urls to a more readable representation.
                 ;;   "#book_script" -> "book_script"
                 ;;   "/referenced_book#book_script" -> "referenced_book/book_script"
                 (-> value
                     (subs 1)
                     (string/replace "#" "/")))))
           (when (in/behavior node-type :id)
             (let [value (in/node-value node :id evaluation-context)]
               (when (string? value)
                 (coll/not-empty value)))))))))

(defn node-debug-label
  ([node-id]
   (g/with-auto-evaluation-context evaluation-context
     (node-debug-label node-id evaluation-context)))
  ([node-id {:keys [basis] :as evaluation-context}]
   (let [node (g/node-by-id basis node-id)
         node-type (g/node-type node)]
     (or (when (in/inherits? node-type resource/ResourceNode)
           (let [resource (resource-node/resource basis node-id)]
             (cond
               (resource/memory-resource? resource)
               (str "embedded." (resource/ext resource))

               (some? (gt/original node))
               (let [proj-path (resource/proj-path resource)]
                 (if-let [owner-resource (resource-node/owner-resource basis node-id)]
                   (str proj-path " override in " (resource/proj-path owner-resource))
                   (str proj-path " override")))

               :else
               (resource/proj-path resource))))
         (node-qualifier-label node-id evaluation-context)
         (when (in/inherits? node-type outline/OutlineNode)
           (coll/not-empty (:label (g/maybe-node-value node-id :node-outline evaluation-context))))
         (let [name (g/maybe-node-value node-id :name evaluation-context)]
           (when (string? name)
             (coll/not-empty name)))
         (str (name (:k node-type)) \# node-id)))))

(defn node-debug-label-path
  ([node-id]
   (g/with-auto-evaluation-context evaluation-context
     (node-debug-label-path node-id evaluation-context)))
  ([node-id {:keys [basis] :as evaluation-context}]
   (let [graph-id (g/node-id->graph-id node-id)
         project-node-id (g/graph-value basis graph-id :project-id)]
     (->> node-id
          (iterate #(core/owner-node-id basis %))
          (take-while #(some-> % (not= project-node-id)))
          (reverse)
          (mapv #(node-debug-label % evaluation-context))))))

(defn node-debug-info
  ([node-id]
   (g/with-auto-evaluation-context evaluation-context
     (node-debug-info node-id evaluation-context)))
  ([node-id {:keys [basis] :as evaluation-context}]
   (let [node-type-kw (g/node-type-kw basis node-id)
         node-debug-label-path (node-debug-label-path node-id evaluation-context)
         owner-resource-node-id (try
                                  (resource-node/owner-resource-node-id basis node-id)
                                  (catch Exception _
                                    nil))
         owner-resource-node-type-kw (some->> owner-resource-node-id (g/node-type-kw basis))]
     {:node-type-kw node-type-kw
      :node-debug-label-path node-debug-label-path
      :owner-resource-node-id owner-resource-node-id
      :owner-resource-node-type-kw owner-resource-node-type-kw})))

;; -----------------------------------------------------------------------------
;; set-properties-from-pb-map macro
;; -----------------------------------------------------------------------------

(letfn [(parse-source-expression [source-expression]
          (or (cond
                (keyword? source-expression)
                {:pb-field-keyword source-expression}

                (coll/list-or-cons? source-expression)
                (case (coll/bounded-count 4 source-expression)
                  2 (let [[pb-field-value->property-value sub-source-expression] source-expression]
                      (assert (symbol? pb-field-value->property-value))
                      (assoc (parse-source-expression sub-source-expression)
                        :pb-field-value->property-value pb-field-value->property-value))
                  3 (let [[pb-field-keyword or-keyword default-expression] source-expression]
                      (assert (keyword? pb-field-keyword))
                      (assert (= :or or-keyword))
                      {:pb-field-keyword pb-field-keyword
                       :default-expression default-expression})
                  nil)

                :else
                nil)
              (assert false (str "Malformed set-properties-from-pb-map call. Expected pb-map-keyword, (fn-symbol sub-source-expression), or (pb-map-keyword :or default-expression), got: " source-expression))))

        (parse-mapping-expressions [mapping-expressions]
          (assert (seq mapping-expressions) "Malformed set-properties-from-pb-map call. Expected interleaved property-symbol and source-expression values to follow pb-map-expression")
          (into []
                (comp (partition-all 2)
                      (map (fn [[property-symbol source-expression]]
                             (assert (symbol? property-symbol) (str "Malformed set-properties-from-pb-map call. Expected property-symbol, got: " property-symbol))
                             (assert (some? source-expression) (str "Malformed set-properties-from-pb-map call. Expected source-expression to follow property-symbol: " property-symbol))
                             (-> source-expression
                                 (parse-source-expression)
                                 (assoc :property-keyword (keyword property-symbol))))))
                mapping-expressions))

        (pb-field-validation-forms [pb-class-expression mapping-infos]
          (when *check-pb-field-names*
            (let [mapped-pb-field-keywords
                  (into (sorted-set)
                        (map :pb-field-keyword)
                        mapping-infos)]
              (list
                `(let [^Class pb-class# ~pb-class-expression
                       ~'invalid-pb-field-keywords (set/difference ~mapped-pb-field-keywords
                                                                   (protobuf/field-key-set pb-class#))]
                   (assert (empty? ~'invalid-pb-field-keywords)
                           (format "Protobuf class '%s' does not have fields: %s"
                                   (.getName pb-class#)
                                   (string/join ", " ~'invalid-pb-field-keywords))))))))

        (set-property-form [node-id-symbol pb-map-symbol {:keys [default-expression pb-field-keyword pb-field-value->property-value property-keyword] :as _mapping-info}]
          (cond
            (and default-expression pb-field-value->property-value)
            `(g/set-property ~node-id-symbol ~property-keyword
               (~pb-field-value->property-value
                 (if-some [pb-field-value# (~pb-field-keyword ~pb-map-symbol)]
                   pb-field-value#
                   ~default-expression)))

            default-expression
            `(g/set-property ~node-id-symbol ~property-keyword
               (if-some [pb-field-value# (~pb-field-keyword ~pb-map-symbol)]
                 pb-field-value#
                 ~default-expression))

            pb-field-value->property-value
            `(when-some [pb-field-value# (~pb-field-keyword ~pb-map-symbol)]
               (g/set-property ~node-id-symbol ~property-keyword
                 (~pb-field-value->property-value pb-field-value#)))

            :else
            `(when-some [pb-field-value# (~pb-field-keyword ~pb-map-symbol)]
               (g/set-property ~node-id-symbol ~property-keyword pb-field-value#))))

        (set-properties-tx-data-form [node-id-symbol pb-map-symbol mapping-infos]
          (list*
            `concat
            (map #(set-property-form node-id-symbol pb-map-symbol %)
                 mapping-infos)))]

  (defmacro set-properties-from-pb-map
    "Convenience macro for setting graph node properties from a protobuf message
    in map format. Returns a sequence of g/set-property calls for use with
    g/transact.

    The node-id-expression is expected to evaluate to the id of a graph node.

    The pb-class-expression is expected to resolve to a Class that has been
    compiled from a protobuf message declaration. We use this to validate that
    any field names mentioned among the mapping-expressions correspond to fields
    in the protobuf message.

    The pb-map-expression is expected to resolve to a Clojure map of protobuf
    field keywords to field values. Typically, this would be obtained from the
    protobuf/read-map-without-defaults function.

    Then follows the mapping-expressions, which are interleaved pairs of the
    target node property symbols and source-expressions.

    Each source-expression can be either:

    1. A keyword denoting a protobuf field. Note that we mangle the field names
       of the compiled Java class to match the Clojure convention using the
       protobuf/field-name->key function. If the denoted field is present in the
       pb-map, a g/set-property call will be included in the resulting tx-data.

    2. A two-element list expression consisting of a function-symbol and a
       source-expression. This will evaluate the source-expression, and apply
       the function to the resulting value before supplying it to the
       g/set-property call.

    3. A three-element list expression consisting of a protobuf field keyword,
       the keyword literal :or, and a default-expression that will be evaluated
       to produce a default value for the property if no value for the field is
       present in the pb-map.

    Example:

    (let [resolve-resource #(workspace/resolve-resource resource %)]
      (gu/set-properties-from-pb-map sprite-node-id Sprite$SpriteDesc sprite-desc
        default-animation :default-animation
        manual-size (v4->v3 :size)
        material (resolve-resource (:material :or default-material-proj-path))))"
    [node-id-expression pb-class-expression pb-map-expression & mapping-expressions]
    (let [node-id-symbol (gensym "node-id")
          pb-map-symbol (gensym "pb-map")
          mapping-infos (parse-mapping-expressions mapping-expressions)]
      `(let [~node-id-symbol ~node-id-expression
             ~pb-map-symbol ~pb-map-expression]
         ~@(pb-field-validation-forms pb-class-expression mapping-infos)
         ~(set-properties-tx-data-form node-id-symbol pb-map-symbol mapping-infos)))))
