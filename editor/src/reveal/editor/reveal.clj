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

(ns editor.reveal
  (:require [cljfx.fx.button :as fx.button]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.label :as fx.label]
            [cljfx.fx.v-box :as fx.v-box]
            [clojure.core.async :as a]
            [clojure.core.async.impl.channels]
            [clojure.main :as m]
            [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.code.data]
            [editor.math :as math]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [internal.graph.types :as gt]
            [internal.system :as is]
            [util.coll :as coll]
            [util.eduction :as e]
            [vlaaad.reveal :as r])
  (:import [clojure.core.async.impl.channels ManyToManyChannel]
           [clojure.lang IRef]
           [editor.code.data Cursor CursorRange]
           [editor.resource FileResource ZipResource]
           [editor.workspace BuildResource]
           [internal.graph.types Arc Endpoint]
           [javafx.scene Parent]
           [javax.vecmath Color3f Color4f Matrix3d Matrix3f Matrix4d Matrix4f Point2d Point2f Point3d Point3f Point4d Point4f Quat4d Quat4f Tuple2d Tuple2f Tuple3d Tuple3f Tuple4d Tuple4f Vector2d Vector2f Vector3d Vector3f Vector4d Vector4f]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- workspace []
  0)

(defn- make-evaluation-context
  ([] (is/default-evaluation-context (or @g/*the-system* g/fake-system)))
  ([options] (is/custom-evaluation-context (or @g/*the-system* g/fake-system) options)))

(defn- node-value-or-err [ec node-id label]
  (try
    [(g/node-value node-id label ec) nil]
    (catch Exception e
      [nil e])))

(defn- node-value-or-err->sf [[v e]]
  (if e
    (let [{:clojure.error/keys [cause class]} (-> e Throwable->map m/ex-triage)]
      (r/as e (r/raw-string (or cause class) {:fill :error})))
    (r/stream v)))

(defn- node-id-sf [{:keys [basis] :as ec} node-id]
  (let [type-sym (symbol (:k (g/node-type* basis node-id)))]
    (r/horizontal
      (r/raw-string
        (if (g/node-instance? basis resource-node/ResourceNode node-id)
          (or (resource/proj-path (g/node-value node-id :resource ec))
              "[in-memory]")
          "")
        {:fill :string})
      (r/raw-string (str "#" node-id) {:fill :scalar})
      (r/raw-string (str "@" type-sym) {:fill :object}))))

(declare label-tree-node)

(defn- node-children-fn [ec node-id]
  (fn []
    (let [{:keys [basis]} ec
          node-type-def @(g/node-type* basis node-id)
          override-original (g/override-original basis node-id)
          children (->> [:input :property :output]
                        (mapcat (fn [k] (keys (get node-type-def k))))
                        distinct
                        sort
                        (map
                          (fn [label]
                            (label-tree-node ec node-id label))))]
      (cond->> children
               override-original
               (cons {:value override-original
                      :render (r/horizontal
                                (r/raw-string "override-original" {:fill :symbol})
                                r/separator
                                (node-id-sf ec override-original))
                      :children (node-children-fn ec override-original)})))))

(defn- label-tree-node [{:keys [basis] :as ec} node-id label]
  (let [[v e :as v-or-e] (node-value-or-err ec node-id label)
        sources (g/sources-of basis node-id label)
        targets (g/targets-of basis node-id label)]
    (cond->
      {:value (or e v)
       :annotation {::node-id+label [node-id label]}
       :render (r/horizontal
                 (r/stream label)
                 r/separator
                 (node-value-or-err->sf v-or-e))}
      (or (seq sources) (seq targets))
      (assoc :children #(map (fn [[relation [rel-node-id related-label]]]
                               (let [[v e :as v-or-e] (node-value-or-err ec rel-node-id related-label)]
                                 {:value (or e v)
                                  :render (r/horizontal
                                            (r/raw-string
                                              ({:source "<- " :target "-> "} relation)
                                              {:fill :util})
                                            (r/stream related-label)
                                            (r/raw-string " of " {:fill :util})
                                            (node-id-sf ec rel-node-id)
                                            (r/raw-string ": " {:fill :util})
                                            (node-value-or-err->sf v-or-e))
                                  :children (node-children-fn ec rel-node-id)}))
                             (concat (map (fn [x] [:source x]) sources)
                                     (map (fn [x] [:target x]) targets)))))))

(defn root-tree-node [ec node-id]
  {:value node-id
   :render (node-id-sf ec node-id)
   :children (node-children-fn ec node-id)})

(r/defaction ::defold:node-tree [x ann]
  (when (g/node-id? x)
    (let [ec (or (::evaluation-context ann) (make-evaluation-context))]
      (when (g/node-by-id (:basis ec) x)
        (fn []
          {:fx/type r/tree-view
           :branch? :children
           :render #(:render % (:value %))
           :valuate :value
           :annotate :annotation
           :children #((:children %))
           :root (root-tree-node ec x)})))))

(defn- endpoint-successors [basis endpoint]
  (let [node-id (g/endpoint-node-id endpoint)
        graph-id (g/node-id->graph-id node-id)]
    (get-in basis [:graphs graph-id :successors node-id (g/endpoint-label endpoint)])))

(defn- render-endpoint-successor [ec endpoint]
  (let [node-id (g/endpoint-node-id endpoint)
        label (g/endpoint-label endpoint)
        cached (contains? (g/cached-outputs (g/node-type* (:basis ec) node-id)) label)]
    (r/horizontal
      (r/raw-string (str label) {:fill (if cached :object :keyword)})
      (r/raw-string " of " {:fill :util})
      (node-id-sf ec node-id))))

(r/defaction ::defold:successors [x]
  (when (instance? Endpoint x)
    (let [ec (make-evaluation-context)
          basis (:basis ec)]
      (when (endpoint-successors basis x)
        (fn []
          {:fx/type r/tree-view
           :render #(render-endpoint-successor ec %)
           :branch? (comp seq #(endpoint-successors basis %))
           :children (comp sort #(endpoint-successors basis %))
           :root x})))))

(defn node-id-in-context
  ([node-id]
   (node-id-in-context node-id (make-evaluation-context)))
  ([node-id evaluation-context]
   (r/stream node-id {::evaluation-context evaluation-context})))

(defn- de-duplicating-observable [ref f]
  (reify IRef
    (deref [_] (f @ref))
    (addWatch [this key callback]
      (add-watch ref [this key] (fn [_ _ old new]
                                  (let [old (f old)
                                        new (f new)]
                                    (when-not (= old new)
                                      (callback key this old new))))))
    (removeWatch [this key]
      (remove-watch ref [this key]))))

(defn watch-all [node-id label]
  {:fx/type r/ref-watch-all-view
   :ref (de-duplicating-observable
          g/*the-system*
          (fn [sys]
            (g/node-value node-id label (is/default-evaluation-context sys))))})

(r/defaction ::defold:watch [_ {::keys [node-id+label]}]
  (when (and node-id+label @g/*the-system*)
    #(apply watch-all node-id+label)))

(defn- stream-arc-contents [arc]
  (apply
    r/horizontal
    (r/stream (gt/source-id arc))
    r/separator
    (r/stream (gt/source-label arc))
    r/separator
    (r/stream (gt/target-id arc))
    r/separator
    (r/stream (gt/target-label arc))
    (when-let [other (seq (dissoc arc :source-id :source-label :target-id :target-label))]
      [r/separator
       (->> other
            (eduction (map r/horizontally))
            (r/horizontally))])))

(r/defstream Arc [arc]
  (r/horizontal
    (r/raw-string "#g/arc [" {:fill :object})
    (stream-arc-contents arc)
    (r/raw-string "]" {:fill :object})))

(r/defstream Endpoint [endpoint]
  (r/horizontal
    (r/raw-string "#g/endpoint [" {:fill :object})
    (r/stream (g/endpoint-node-id endpoint))
    r/separator
    (r/stream (g/endpoint-label endpoint))
    (r/raw-string "]" {:fill :object})))

(defn- read-file-resource [str-expr]
  `(workspace/resolve-workspace-resource (workspace) ~str-expr))

(r/defstream FileResource [resource]
  (r/horizontal
    (r/raw-string "#resource/file" {:fill :object})
    r/separator
    (r/stream (resource/proj-path resource))))

(defn- read-zip-resource [str-expr]
  `(workspace/find-resource (workspace) ~str-expr))

(r/defstream ZipResource [resource]
  (r/horizontal
    (r/raw-string "#resource/zip" {:fill :object})
    r/separator
    (r/stream (resource/proj-path resource))))

(r/defstream BuildResource [resource]
  (r/horizontal
    (r/raw-string "#resource/build" {:fill :object})
    r/separator
    (r/stream (resource/proj-path resource))))

(defn- stream-cursor-contents [{:keys [row col] :as cursor}]
  (apply
    r/horizontal
    (r/stream row)
    r/separator
    (r/stream col)
    (when-let [other (seq (dissoc cursor :row :col))]
      [r/separator
       (->> other
            (eduction (map r/horizontally))
            (r/horizontally))])))

(r/defstream Cursor [{:keys [row col] :as cursor}]
  (r/horizontal
    (r/raw-string "#code/cursor [" {:fill :object})
    (stream-cursor-contents cursor)
    (r/raw-string "]" {:fill :object})))

(r/defstream CursorRange [{:keys [from to] :as range}]
  (r/horizontal
    (r/raw-string "#code/range [" {:fill :object})
    (apply
      r/horizontal
      (r/as from
        (r/horizontal
          (r/raw-string "[" {:fill :object})
          (stream-cursor-contents from)
          (r/raw-string "]" {:fill :object})))
      r/separator
      (r/as to
        (r/horizontal
          (r/raw-string "[" {:fill :object})
          (stream-cursor-contents to)
          (r/raw-string "]" {:fill :object})))
      (when-let [rest (seq (dissoc range :from :to))]
        [r/separator (->> rest (eduction (map r/horizontally)) (r/horizontally))]))
    (r/raw-string "]" {:fill :object})))

(r/defaction ::javafx:children [x]
  (when (instance? Parent x)
    (constantly {:fx/type r/tree-view
                 :render (fn [^Parent node]
                           (r/horizontal
                             (r/raw-string (.getName (class node)) {:fill :object})
                             (r/raw-string (format "@%08x" (System/identityHashCode node))
                                           {:fill :util})
                             (r/raw-string (str (when-let [id (.getId node)]
                                                  (str "#" id))
                                                (when-let [style-classes (seq (.getStyleClass node))]
                                                  (str/join (map #(str "." %) style-classes)))
                                                (when-let [pseudo-classes (seq (.getPseudoClassStates node))]
                                                  (str/join (map #(str ":" %) pseudo-classes))))
                                           {:fill :string})))
                 :root x
                 :branch? #(and (instance? Parent %)
                                (seq (.getChildrenUnmodifiable ^Parent %)))
                 :children #(vec (.getChildrenUnmodifiable ^Parent %))})))

(r/defstream ManyToManyChannel [ch]
  (r/horizontal
    (r/raw-string "(a/chan " {:fill :object})
    (r/raw-string (format "#_0x%08x" (System/identityHashCode ch)) {:fill :util})
    (r/raw-string ")" {:fill :object})))

(r/defaction ::lsp:watch [x]
  (when (= :editor.lsp/running-lsps (:type (meta x)))
    (fn []
      {:fx/type r/observable-view
       :ref x
       :fn (fn [in->state]
             {:fx/type fx.v-box/lifecycle
              :children (for [[in state] in->state]
                          {:fx/type fx.v-box/lifecycle
                           :v-box/vgrow :always
                           :children [{:fx/type fx.h-box/lifecycle
                                       :alignment :center-left
                                       :padding 10
                                       :spacing 5
                                       :children [{:fx/type fx.label/lifecycle
                                                   :max-width ##Inf
                                                   :h-box/hgrow :always
                                                   :text (format "LSP server #0x%08x" (System/identityHashCode in))}
                                                  {:fx/type fx.button/lifecycle
                                                   :focus-traversable false
                                                   :text "Shutdown"
                                                   :on-action (fn [_] (a/close! in))}]}
                                      {:fx/type r/value-view
                                       :v-box/vgrow :always
                                       :value state}]})})})))

(defn- vecmath-matrix-sf [matrix]
  (let [row-col-strs (math/vecmath-matrix-pprint-strings matrix)]
    (r/horizontal
      (r/raw-string "#v/" {:fill :object})
      (r/raw-string (.getSimpleName (class matrix)) {:fill :object})
      (r/raw-string " [" {:fill :object})
      (apply
        r/vertical
        (coll/transfer row-col-strs :eduction
          (map (fn [col-strs]
                 (apply
                   r/horizontal
                   (coll/transfer col-strs :eduction
                     (map (fn [col-str]
                            (let [style (if (math/zero-vecmath-matrix-col-str? col-str)
                                          {:fill :util}
                                          {:fill :scalar})]
                              (r/raw-string col-str style))))
                     (interpose r/separator)))))))
      (r/raw-string "]" {:fill :object}))))

(r/defstream Matrix3d [^Matrix3d matrix]
  (vecmath-matrix-sf matrix))

(r/defstream Matrix3f [^Matrix3f matrix]
  (vecmath-matrix-sf matrix))

(r/defstream Matrix4d [^Matrix4d matrix]
  (vecmath-matrix-sf matrix))

(r/defstream Matrix4f [^Matrix4f matrix]
  (vecmath-matrix-sf matrix))

(defn- vecmath-tuple-sf [^Class tuple-class & component-values]
  (apply
    r/horizontal
    (r/raw-string "#v/" {:fill :object})
    (r/raw-string (.getSimpleName tuple-class) {:fill :object})
    (r/raw-string " [" {:fill :object})
    (-> component-values
        (coll/transfer :eduction
          (map r/stream)
          (interpose r/separator))
        (e/conj (r/raw-string "]" {:fill :object})))))

(r/defstream Tuple2d [^Tuple2d tuple]
  (vecmath-tuple-sf (class tuple) (.getX tuple) (.getY tuple)))

(r/defstream Tuple2f [^Tuple2f tuple]
  (vecmath-tuple-sf (class tuple) (.getX tuple) (.getY tuple)))

(r/defstream Tuple3d [^Tuple3d tuple]
  (vecmath-tuple-sf (class tuple) (.getX tuple) (.getY tuple) (.getZ tuple)))

(r/defstream Tuple3f [^Tuple3f tuple]
  (vecmath-tuple-sf (class tuple) (.getX tuple) (.getY tuple) (.getZ tuple)))

(r/defstream Tuple4d [^Tuple4d tuple]
  (vecmath-tuple-sf (class tuple) (.getX tuple) (.getY tuple) (.getZ tuple) (.getW tuple)))

(r/defstream Tuple4f [^Tuple4f tuple]
  (vecmath-tuple-sf (class tuple) (.getX tuple) (.getY tuple) (.getZ tuple) (.getW tuple)))

(defn- read-color-3f [[x y z]]
  `(Color3f. ~x ~y ~z))

(defn- read-color-4f [[x y z w]]
  `(Color4f. ~x ~y ~z ~w))

(defn- read-matrix-3d [[m00 m01 m02 m10 m11 m12 m20 m21 m22]]
  `(Matrix3d. ~m00 ~m01 ~m02 ~m10 ~m11 ~m12 ~m20 ~m21 ~m22))

(defn- read-matrix-3f [[m00 m01 m02 m10 m11 m12 m20 m21 m22]]
  `(Matrix3f. ~m00 ~m01 ~m02 ~m10 ~m11 ~m12 ~m20 ~m21 ~m22))

(defn- read-matrix-4d [[m00 m01 m02 m03 m10 m11 m12 m13 m20 m21 m22 m23 m30 m31 m32 m33]]
  `(Matrix4d. ~m00 ~m01 ~m02 ~m03 ~m10 ~m11 ~m12 ~m13 ~m20 ~m21 ~m22 ~m23 ~m30 ~m31 ~m32 ~m33))

(defn- read-matrix-4f [[m00 m01 m02 m03 m10 m11 m12 m13 m20 m21 m22 m23 m30 m31 m32 m33]]
  `(Matrix4f. ~m00 ~m01 ~m02 ~m03 ~m10 ~m11 ~m12 ~m13 ~m20 ~m21 ~m22 ~m23 ~m30 ~m31 ~m32 ~m33))

(defn- read-point-2d [[x y]]
  `(Point2d. ~x ~y))

(defn- read-point-2f [[x y]]
  `(Point2f. ~x ~y))

(defn- read-point-3d [[x y z]]
  `(Point3d. ~x ~y ~z))

(defn- read-point-3f [[x y z]]
  `(Point3f. ~x ~y ~z))

(defn- read-point-4d [[x y z w]]
  `(Point4d. ~x ~y ~z ~w))

(defn- read-point-4f [[x y z w]]
  `(Point4f. ~x ~y ~z ~w))

(defn- read-quat-4d [[x y z w]]
  `(Quat4d. ~x ~y ~z ~w))

(defn- read-quat-4f [[x y z w]]
  `(Quat4f. ~x ~y ~z ~w))

(defn- read-vector-2d [[x y]]
  `(Vector2d. ~x ~y))

(defn- read-vector-2f [[x y]]
  `(Vector2f. ~x ~y))

(defn- read-vector-3d [[x y z]]
  `(Vector3d. ~x ~y ~z))

(defn- read-vector-3f [[x y z]]
  `(Vector3f. ~x ~y ~z))

(defn- read-vector-4d [[x y z w]]
  `(Vector4d. ~x ~y ~z ~w))

(defn- read-vector-4f [[x y z w]]
  `(Vector4f. ~x ~y ~z ~w))

(comment
  ;; A map containing simple values for manual testing.
  (sorted-map
    :arc (assoc (gt/->Arc 12345 :source-label 54321 :target-label) :ex/arc "ex")
    :color-3f (Color3f. 0.0 0.1 0.2)
    :color-4f (Color4f. 0.0 0.1 0.2 0.3)
    :cursor (assoc (Cursor. 5 80) :ex/cursor "ex")
    :cursor-range (assoc (CursorRange. (assoc (Cursor. 4 0) :ex/from-cursor "ex")
                                       (assoc (Cursor. 5 80) :ex/to-cursor "ex")) :ex/cursor-range "ex")
    :endpoint (g/endpoint 12345 :label)
    :matrix-3d (Matrix3d. 0.0 0.1 0.2
                          1.0 1.1 1.2
                          2.0 2.1 2.2)
    :matrix-3f (Matrix3f. 0.0 0.1 0.2
                          1.0 1.1 1.2
                          2.0 2.1 2.2)
    :matrix-4d (Matrix4d. 0.0 0.1 0.2 0.3
                          1.0 1.1 1.2 1.3
                          2.0 2.1 2.2 2.3
                          3.0 3.1 3.2 3.3)
    :matrix-4f (Matrix4f. 0.0 0.1 0.2 0.3
                          1.0 1.1 1.2 1.3
                          2.0 2.1 2.2 2.3
                          3.0 3.1 3.2 3.3)
    :point-2d (Point2d. 0.0 0.1)
    :point-2f (Point2f. 0.0 0.1)
    :point-3d (Point3d. 0.0 0.1 0.2)
    :point-3f (Point3f. 0.0 0.1 0.2)
    :point-4d (Point4d. 0.0 0.1 0.2 0.3)
    :point-4f (Point4f. 0.0 0.1 0.2 0.3)
    :quat-4d (Quat4d. 0.0 0.1 0.2 0.3)
    :quat-4f (Quat4f. 0.0 0.1 0.2 0.3)
    :vector-2d (Vector2d. 0.0 0.1)
    :vector-2f (Vector2f. 0.0 0.1)
    :vector-3d (Vector3d. 0.0 0.1 0.2)
    :vector-3f (Vector3f. 0.0 0.1 0.2)
    :vector-4d (Vector4d. 0.0 0.1 0.2 0.3)
    :vector-4f (Vector4f. 0.0 0.1 0.2 0.3)))