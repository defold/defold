(ns integration.form-type-preservation-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.cljfx-form-view :as cljfx-form-view]
            [editor.types :as t]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]])
  (:import [javafx.scene Parent]
           [javafx.scene.control Control Label ScrollPane]
           [javafx.scene.layout GridPane Pane]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- double-vec [& args]
  (mapv double args))

(defn- float-vec [& args]
  (mapv float args))

(defn- set-form-op [{:keys [node-id]} path value]
  {:pre [(= 1 (count path))]}
  (let [prop-kw (first path)]
    (g/set-property! node-id prop-kw value)))

(defn- clear-form-op [{:keys [node-id]} path]
  {:pre [(= 1 (count path))]}
  (let [prop-kw (first path)]
    (g/clear-property! node-id prop-kw)))

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
              [:vec4] vec4
              [:table :integer] integer
              [:table :number] number
              [:table :vec4] vec4}
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
       (tree-seq (constantly true)
                 #(.getChildrenUnmodifiable ^Parent %))
       (filterv #(and (instance? Control %)
                      (not (instance? Label %))))))

(defn- test-numeric-form-widget! [resource-node {:keys [path] :as field} num-value]
  {:pre [(= 1 (count path))]}
  (let [prop-kw (first path)
        project-graph (g/node-id->graph-id resource-node)
        original-value (g/node-value resource-node prop-kw)
        [num-field] (:controls field)]
    (with-open [_ (test-util/make-graph-reverter project-graph)]
      (test-util/set-control-value! num-field num-value)
      (let [modified-value (g/node-value resource-node prop-kw)]
        (is (not= original-value modified-value))
        (test-util/ensure-number-type-preserving! original-value modified-value)))))

(defn- test-vector-form-widget! [resource-node {:keys [path] :as field} expected-text-field-count]
  {:pre [(= 1 (count path))]}
  (let [prop-kw (first path)
        project-graph (g/node-id->graph-id resource-node)
        original-value (g/node-value resource-node prop-kw)
        text-fields (:controls field)
        check! (fn check! [num-field num-value]
                 (with-open [_ (test-util/make-graph-reverter project-graph)]
                   (test-util/set-control-value! num-field num-value)
                   (let [modified-value (g/node-value resource-node prop-kw)]
                     (is (not= original-value modified-value))
                     (is (= (count original-value) (count modified-value)))
                     (test-util/ensure-number-type-preserving! original-value modified-value))))]
    (is (= expected-text-field-count (count text-fields)))
    (doall
      (map check!
           text-fields
           (range 0.11 0.99 0.11)))))

(defmulti test-form-widget! (fn [_resource-node field] (:type field)))

(defmethod test-form-widget! :integer [resource-node field]
  (test-numeric-form-widget! resource-node field 2))

(defmethod test-form-widget! :number [resource-node field]
  (test-numeric-form-widget! resource-node field 0.11))

(defmethod test-form-widget! :vec4 [resource-node field]
  (test-vector-form-widget! resource-node field 4))

(defn- render-form! [view-node]
  (let [renderer (g/node-value view-node :renderer)
        form-data (g/node-value view-node :form-data)
        ui-state (g/node-value view-node :ui-state)]
    (deref (renderer {:form-data form-data
                      :ui-state ui-state}))
    form-data))

(defn- form-row-vboxes [^Parent form-view-parent]
  (let [^ScrollPane scroll-pane (.lookup form-view-parent "ScrollPane")
        ^Parent fields-grid-pane (-> scroll-pane .getContent (.lookup ".cljfx-form-fields"))]
    (assert (some? fields-grid-pane))
    (eduction
      (filter #(= 2 (GridPane/getColumnIndex %)))
      (.getChildrenUnmodifiable fields-grid-pane))))

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

          parent (Pane.)
          project-graph (g/make-graph! :history true :volatility 1)
          view-graph (g/make-graph! :history false :volatility 2)
          resource-node (apply g/make-node! project-graph NumericPropertiesNode (mapcat identity property-values))
          view-node (cljfx-form-view/make-form-view-node! view-graph parent resource-node nil nil)
          form-data (render-form! view-node)
          fields (map (fn [field form-row-vbox]
                        (assoc field :controls (test-util/editable-controls form-row-vbox)))
                      (-> form-data :sections first :fields)
                      (form-row-vboxes parent))]
      (is (= 3 (count fields)))
      (doseq [field fields]
        (testing (str "Types preserved after editing " (:path field))
          (test-form-widget! resource-node field))))))

(deftest float-form-fields-preserve-type-test
  (testing "Values are 32-bit in generic vectors before editing"
    (ensure-float-form-fields-preserve-type! generic-32-bit-property-values))
  (testing "Values are 64-bit in generic vectors before editing"
    (ensure-float-form-fields-preserve-type! generic-64-bit-property-values))
  (testing "Values are 32-bit in specialized vectors before editing"
    (ensure-float-form-fields-preserve-type! specialized-32-bit-property-values))
  (testing "Values are 64-bit in specialized vectors before editing"
    (ensure-float-form-fields-preserve-type! specialized-64-bit-property-values)))
