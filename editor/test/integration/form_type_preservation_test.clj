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

(ns integration.form-type-preservation-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.cljfx-form-view :as cljfx-form-view]
            [editor.types :as t]
            [editor.ui :as ui]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]
            [util.fn :as fn])
  (:import [javafx.scene Parent]
           [javafx.scene.control Control Label ScrollPane]
           [javafx.scene.layout AnchorPane GridPane]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- double-vec [& args]
  (mapv double args))

(defn- float-vec [& args]
  (mapv float args))

(defn- set-form-op [{:keys [node-id]} path value]
  {:pre [(= 1 (count path))]}
  (let [prop-kw (first path)]
    (g/set-property node-id prop-kw value)))

(defn- clear-form-op [{:keys [node-id]} path]
  {:pre [(= 1 (count path))]}
  (let [prop-kw (first path)]
    (g/clear-property node-id prop-kw)))

(g/defnk produce-form-data [_node-id integer number vec4]
  (let [fields
        [{:label "Integer"
          :type :integer
          :path [:integer]}
         {:label "Number"
          :type :number
          :path [:number]}
         {:label "Vec4"
          :type :vec4
          :path [:vec4]}]]
    {:navigation false
     :sections [{:title "Section"
                 :fields fields}]
     :values {[:integer] integer
              [:number] number
              [:vec4] vec4}
     :form-ops {:user-data {:node-id _node-id}
                :set set-form-op
                :clear clear-form-op}}))

(g/defnode NumericPropertiesNode
  (property integer g/Int)
  (property number g/Num)
  (property vec4 t/Vec4)
  (output form-data g/Any :cached produce-form-data))

(def ^:private generic-32-bit-property-values
  {:integer (int 1)
   :number (float 0.1)
   :vec4 (float-vec 0.1 0.2 0.3 0.4)})

(def ^:private generic-64-bit-property-values
  {:integer (long 1)
   :number (double 0.1)
   :vec4 (double-vec 0.1 0.2 0.3 0.4)})

(def ^:private specialized-32-bit-property-values
  {:integer (int 1)
   :number (float 0.1)
   :vec4 (vector-of :float 0.1 0.2 0.3 0.4)})

(def ^:private specialized-64-bit-property-values
  {:integer (long 1)
   :number (double 0.1)
   :vec4 (vector-of :double 0.1 0.2 0.3 0.4)})

(defn- find-controls [^Parent parent]
  (->> parent
       (tree-seq fn/constantly-true
                 #(.getChildrenUnmodifiable ^Parent %))
       (filterv #(and (instance? Control %)
                      (not (instance? Label %))))))

(defn- render-form! [view-node]
  (let [promise (promise)]
    (g/node-value view-node :form-view)
    (ui/run-later
      (deliver promise true))
    (deref promise)))

(defn- form-row-vboxes [^Parent form-view-parent]
  (let [^ScrollPane scroll-pane (.lookup form-view-parent "ScrollPane")
        ^Parent fields-grid-pane (-> scroll-pane .getContent (.lookup ".cljfx-form-fields"))]
    (assert (some? fields-grid-pane))
    (eduction
      (filter #(= 2 (GridPane/getColumnIndex %)))
      (.getChildrenUnmodifiable fields-grid-pane))))

(defn- form-row-controls [^Parent form-view-parent ^long row]
  (-> form-view-parent
      (form-row-vboxes)
      (nth row)
      (test-util/editable-controls)))

(defn- check-text-fields! [resource-node prop-kw new-num-values text-fields-fn]
  (let [project-graph (g/node-id->graph-id resource-node)
        view-node (ffirst (g/targets-of resource-node :form-data))
        original-value (g/node-value resource-node prop-kw)

        check-field!
        (fn check-field! [field-index new-num-value]
          (with-open [_ (test-util/make-graph-reverter project-graph)]
            (let [text-field-count
                  (do
                    (render-form! view-node)
                    (let [text-fields (text-fields-fn)
                          text-field (get text-fields field-index)]
                      (when text-field
                        (test-util/set-control-value! text-field new-num-value))
                      (count text-fields)))]
              (let [modified-value (g/node-value resource-node prop-kw)]
                (when (is (= (count new-num-values) text-field-count))
                  (is (not= original-value modified-value))
                  (test-util/ensure-number-type-preserving! original-value modified-value)
                  (when-not (number? original-value)
                    (is (= (count original-value) (count modified-value)))))))))]
    (doall
      (map-indexed check-field! new-num-values))))

(defn- test-simple-form-widget! [resource-node {:keys [path row] :as _field} form-view-parent new-num-values]
  (check-text-fields!
    resource-node (first path) new-num-values
    #(form-row-controls form-view-parent row)))

(defmulti test-form-widget! (fn [_resource-node field _form-view-parent] (:type field)))

(defmethod test-form-widget! :integer [resource-node field form-view-parent]
  (test-simple-form-widget! resource-node field form-view-parent [2]))

(defmethod test-form-widget! :number [resource-node field form-view-parent]
  (test-simple-form-widget! resource-node field form-view-parent [0.11]))

(defmethod test-form-widget! :vec4 [resource-node field form-view-parent]
  (test-simple-form-widget! resource-node field form-view-parent [2.22 3.33 4.44 5.55]))

(defn- ensure-float-form-fields-preserve-type! [original-property-values]
  (with-clean-system
    (let [original-meta {:version "original"}

          property-values
          (into (sorted-map)
                (map (fn [[prop-kw prop-value]]
                       (let [decorated-value (if (vector? prop-value)
                                               (with-meta prop-value original-meta)
                                               prop-value)]
                         [prop-kw decorated-value])))
                original-property-values)

          form-view-parent (AnchorPane.)
          project-graph (g/make-graph! :history true :volatility 1)
          view-graph (g/make-graph! :history false :volatility 2)
          resource-node (apply g/make-node! project-graph NumericPropertiesNode (mapcat identity property-values))
          view-node (cljfx-form-view/make-form-view-node! view-graph form-view-parent resource-node nil nil test-util/localization)
          form-data (g/node-value view-node :form-data)
          fields (->> form-data
                      (:sections)
                      (first)
                      (:fields)
                      (map-indexed (fn [row field]
                                     (assoc field :row row))))]
      (is (= 3 (count fields)))
      (doseq [field fields]
        (testing (str "Types preserved after editing " (:path field))
          (test-form-widget! resource-node field form-view-parent))))))

(deftest float-form-fields-preserve-type-test
  (testing "Values are 32-bit in generic vectors before editing"
    (ensure-float-form-fields-preserve-type! generic-32-bit-property-values))
  (testing "Values are 64-bit in generic vectors before editing"
    (ensure-float-form-fields-preserve-type! generic-64-bit-property-values))
  (testing "Values are 32-bit in specialized vectors before editing"
    (ensure-float-form-fields-preserve-type! specialized-32-bit-property-values))
  (testing "Values are 64-bit in specialized vectors before editing"
    (ensure-float-form-fields-preserve-type! specialized-64-bit-property-values)))
