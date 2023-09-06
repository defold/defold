(ns integration.float-properties-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.field-expression :as field-expression]
            [editor.properties :as properties]
            [editor.properties-view :as properties-view]
            [editor.types :as t]
            [support.test-support :refer [with-clean-system]])
  (:import [clojure.core Vec]
           [editor.properties Curve CurveSpread]
           [javafx.event ActionEvent]
           [javafx.scene Parent]
           [javafx.scene.control ColorPicker Control Label Slider TextField ToggleButton]
           [javafx.scene.paint Color]))

(def ^:private rotation-edit-type properties/quat-rotation-edit-type)

(def ^:private slider-edit-type
  {:type :slider
   :min 0.0
   :max 1.0
   :precision 0.01})

(g/defnode FloatTypePropertiesNode
  (property num g/Num)
  (property vec2 t/Vec2)
  (property vec3 t/Vec3)
  (property vec4 t/Vec4)
  (property rotation t/Vec4 (dynamic edit-type (g/constantly rotation-edit-type)))
  (property slider g/Num (dynamic edit-type (g/constantly slider-edit-type)))
  (property color t/Color)
  (property curve Curve)
  (property curve-spread CurveSpread))

(defn- double-vec [& args]
  (into [] (map double) args))

(defn- float-vec [& args]
  (into [] (map float) args))

(def ^:private generic-float-property-values
  {:num (float 0.1)
   :vec2 (float-vec 0.1 0.2)
   :vec3 (float-vec 0.1 0.2 0.3)
   :vec4 (float-vec 0.1 0.2 0.3 0.4)
   :rotation (float-vec 0.1 0.2 0.3 1.0)
   :slider (float 0.1)
   :color (float-vec 0.1 0.2 0.3 0.4)
   :curve (properties/->curve [(float-vec 0.0 0.1 1.0 0.2)])
   :curve-spread (properties/->curve-spread [(float-vec 0.0 0.1 1.0 0.2)] (float 0.3))})

(def ^:private generic-double-property-values
  {:num (double 0.1)
   :vec2 (double-vec 0.1 0.2)
   :vec3 (double-vec 0.1 0.2 0.3)
   :vec4 (double-vec 0.1 0.2 0.3 0.4)
   :rotation (double-vec 0.1 0.2 0.3 1.0)
   :slider (double 0.1)
   :color (double-vec 0.1 0.2 0.3 0.4)
   :curve (properties/->curve [(double-vec 0.0 0.1 1.0 0.2)])
   :curve-spread (properties/->curve-spread [(double-vec 0.0 0.1 1.0 0.2)] (double 0.3))})

(def ^:private specialized-float-property-values
  {:num (float 0.1)
   :vec2 (vector-of :float 0.1 0.2)
   :vec3 (vector-of :float 0.1 0.2 0.3)
   :vec4 (vector-of :float 0.1 0.2 0.3 0.4)
   :rotation (vector-of :float 0.1 0.2 0.3 1.0)
   :slider (float 0.1)
   :color (vector-of :float 0.1 0.2 0.3 0.4)
   :curve (properties/->curve [(vector-of :float 0.0 0.1 1.0 0.2)])
   :curve-spread (properties/->curve-spread [(vector-of :float 0.0 0.1 1.0 0.2)] (float 0.3))})

(def ^:private specialized-double-property-values
  {:num (double 0.1)
   :vec2 (vector-of :double 0.1 0.2)
   :vec3 (vector-of :double 0.1 0.2 0.3)
   :vec4 (vector-of :double 0.1 0.2 0.3 0.4)
   :rotation (vector-of :double 0.1 0.2 0.3 1.0)
   :slider (double 0.1)
   :color (vector-of :double 0.1 0.2 0.3 0.4)
   :curve (properties/->curve [(vector-of :double 0.0 0.1 1.0 0.2)])
   :curve-spread (properties/->curve-spread [(vector-of :double 0.0 0.1 1.0 0.2)] (double 0.3))})

(defn- property-widget-controls [^Parent parent]
  (->> parent
       (tree-seq (constantly true)
                 #(.getChildrenUnmodifiable %))
       (filter #(and (instance? Control %)
                     (not (instance? Label %))))))

(defmulti set-control-value! (fn [^Control control _num-value] (class control)))

(defmethod set-control-value! ColorPicker [^ColorPicker color-picker num-value]
  (.setValue color-picker (Color/gray num-value))
  (.fireEvent color-picker (ActionEvent. color-picker color-picker)))

(defmethod set-control-value! Slider [^Slider slider num-value]
  (.setValue slider num-value))

(defmethod set-control-value! TextField [^TextField text-field num-value]
  (.setText text-field (field-expression/format-number num-value))
  (.fireEvent text-field (ActionEvent. text-field text-field)))

(defmethod set-control-value! ToggleButton [^ToggleButton toggle-button _num-value]
  (.fire toggle-button))

(defn- coalesced-properties [node-id]
  (let [properties (g/node-value node-id :_properties)]
    (properties/coalesce [properties])))

(defn- make-coalesced-prop-info-fn [node-id prop-kw]
  (fn coalesced-prop-info-fn []
    (let [coalesced-properties (coalesced-properties node-id)]
      (get (:properties coalesced-properties) prop-kw))))

(defn- make-property-widget
  ^Parent [edit-type node-id prop-kw]
  (let [context {}
        coalesced-prop-info-fn (make-coalesced-prop-info-fn node-id prop-kw)
        [widget _update-ui-fn] (properties-view/create-property-control! edit-type context coalesced-prop-info-fn)]
    widget))

(defn- type-preserving? [a b]
  (assert (or (float? a) (vector? a)))
  (assert (or (float? b) (vector? b)))
  (and (is (= (type a) (type b)))
       (or (float? a)
           (and (is (= (count a) (count b)))
                (is (identical? (meta a) (meta b)))
                (or (not (instance? Vec a))
                    (is (= (type (.am ^Vec a))
                           (type (.am ^Vec b)))))
                (is (every? true?
                            (map type-preserving? a b)))))))

(deftest type-preserving?-test
  (let [type-preserving? ; Silence inner assertions since we'll be triggering failures.
        (fn silenced-type-preserving? [a b]
          (with-redefs [do-report identity]
            (type-preserving? a b)))

        original-meta {:version "original"}
        altered-meta {:version "altered"}]

    (is (true? (type-preserving? (float 0.0) (float 0.0))))
    (is (true? (type-preserving? (double 0.0) (double 0.0))))
    (is (false? (type-preserving? (float 0.0) (double 0.0))))
    (is (false? (type-preserving? (double 0.0) (float 0.0))))

    (is (true? (type-preserving? [(float 0.0)] [(float 0.0)])))
    (is (true? (type-preserving? [(double 0.0)] [(double 0.0)])))
    (is (false? (type-preserving? [(float 0.0)] [(double 0.0)])))
    (is (false? (type-preserving? [(double 0.0)] [(float 0.0)])))

    (is (true? (type-preserving? [[(float 0.0)]] [[(float 0.0)]])))
    (is (true? (type-preserving? [[(double 0.0)]] [[(double 0.0)]])))
    (is (false? (type-preserving? [[(float 0.0)]] [[(double 0.0)]])))
    (is (false? (type-preserving? [[(double 0.0)]] [[(float 0.0)]])))

    (is (true? (type-preserving? (vector-of :float 0.0) (vector-of :float 0.0))))
    (is (true? (type-preserving? (vector-of :double 0.0) (vector-of :double 0.0))))
    (is (false? (type-preserving? (vector-of :float 0.0) (vector-of :double 0.0))))
    (is (false? (type-preserving? (vector-of :double 0.0) (vector-of :float 0.0))))

    (is (false? (type-preserving? [(float 0.0)] [(float 0.0) (float 0.0)])))
    (is (false? (type-preserving? [(float 0.0) (float 0.0)] [(float 0.0)])))
    (is (false? (type-preserving? [(double 0.0)] [(double 0.0) (double 0.0)])))
    (is (false? (type-preserving? [(double 0.0) (double 0.0)] [(double 0.0)])))

    (is (true? (type-preserving? (with-meta [(float 0.0)] original-meta) (with-meta [(float 0.0)] original-meta))))
    (is (false? (type-preserving? (with-meta [(float 0.0)] original-meta) (with-meta [(float 0.0)] altered-meta))))
    (is (true? (type-preserving? (with-meta (vector-of :float 0.0) original-meta) (with-meta (vector-of :float 0.0) original-meta))))
    (is (false? (type-preserving? (with-meta (vector-of :float 0.0) original-meta) (with-meta (vector-of :float 0.0) altered-meta))))))

(def ensure-type-preserving! type-preserving?)

(defmulti test-property-widget! (fn [edit-type _node-id _prop-kw]
                                  (properties-view/edit-type->type edit-type)))

(defmethod test-property-widget! g/Num [edit-type node-id prop-kw]
  (let [original-value (g/node-value node-id prop-kw)
        widget (make-property-widget edit-type node-id prop-kw)
        [num-field] (property-widget-controls widget)]
    (set-control-value! num-field 0.11)
    (ensure-type-preserving! original-value (g/node-value node-id prop-kw))))

(defn- test-vector-property-widget! [edit-type node-id prop-kw]
  (let [original-value (g/node-value node-id prop-kw)
        widget (make-property-widget edit-type node-id prop-kw)]
    (doall
      (map (fn [num-field num-value]
             (set-control-value! num-field num-value)
             (ensure-type-preserving! original-value (g/node-value node-id prop-kw)))
           (property-widget-controls widget)
           (range 0.11 0.99 0.11)))))

(defmethod test-property-widget! t/Vec2 [edit-type node-id prop-kw]
  (test-vector-property-widget! edit-type node-id prop-kw))

(defmethod test-property-widget! t/Vec3 [edit-type node-id prop-kw]
  (test-vector-property-widget! edit-type node-id prop-kw))

(defmethod test-property-widget! t/Vec4 [edit-type node-id prop-kw]
  (test-vector-property-widget! edit-type node-id prop-kw))

(defmethod test-property-widget! t/Color [edit-type node-id prop-kw]
  (let [original-value (g/node-value node-id prop-kw)
        widget (make-property-widget edit-type node-id prop-kw)
        [color-picker] (property-widget-controls widget)]
    (set-control-value! color-picker 0.11)
    (ensure-type-preserving! original-value (g/node-value node-id prop-kw))))

(defmethod test-property-widget! :slider [edit-type node-id prop-kw]
  (let [original-value (g/node-value node-id prop-kw)
        widget (make-property-widget edit-type node-id prop-kw)
        [value-field slider] (property-widget-controls widget)]
    (set-control-value! value-field 0.11)
    (ensure-type-preserving! original-value (g/node-value node-id prop-kw))
    (set-control-value! slider 0.22)
    (ensure-type-preserving! original-value (g/node-value node-id prop-kw))))

(defmethod test-property-widget! Curve [edit-type node-id prop-kw]
  (let [original-value (g/node-value node-id prop-kw)
        widget (make-property-widget edit-type node-id prop-kw)
        [edit-curve-button value-field] (property-widget-controls widget)]
    (set-control-value! edit-curve-button 0.0)
    (ensure-type-preserving! (properties/curve-vals original-value) (properties/curve-vals (g/node-value node-id prop-kw)))
    (set-control-value! value-field 0.11)
    (ensure-type-preserving! (properties/curve-vals original-value) (properties/curve-vals (g/node-value node-id prop-kw)))))

(defmethod test-property-widget! CurveSpread [edit-type node-id prop-kw]
  (let [original-value (g/node-value node-id prop-kw)
        widget (make-property-widget edit-type node-id prop-kw)
        [edit-curve-button value-field spread-field] (property-widget-controls widget)
        check! (fn check! []
                 (let [value (g/node-value node-id prop-kw)]
                   (ensure-type-preserving! (properties/curve-vals original-value)
                                            (properties/curve-vals value))
                   (ensure-type-preserving! (:spread original-value)
                                            (:spread value))))]
    (set-control-value! edit-curve-button 0.0)
    (check!)
    (set-control-value! value-field 0.11)
    (check!)
    (set-control-value! spread-field 0.22)
    (check!)))

(defn- ensure-float-properties-preserve-type! [original-property-values]
  (with-clean-system
    (let [node-id (apply g/make-node! world FloatTypePropertiesNode (mapcat identity original-property-values))]
      (let [edit-type-by-prop-kw
            (into (sorted-map)
                  (map (fn [[prop-kw prop-info]]
                         (let [edit-type (properties/property-edit-type prop-info)]
                           [prop-kw edit-type])))
                  (:properties (g/node-value node-id :_properties)))]
        (doseq [[prop-kw edit-type] edit-type-by-prop-kw]
          (testing (format "Types preserved after editing (property %s)" (name prop-kw))
            (test-property-widget! edit-type node-id prop-kw)))))))

(deftest float-properties-preserve-type-test
  (testing "Values are floats in generic vectors before editing"
    (ensure-float-properties-preserve-type! generic-float-property-values))
  (testing "Values are doubles in generic vectors before editing"
    (ensure-float-properties-preserve-type! generic-double-property-values))
  (testing "Values are floats in specialized vectors before editing"
    (ensure-float-properties-preserve-type! specialized-float-property-values))
  (testing "Values are doubles in specialized vectors before editing"
    (ensure-float-properties-preserve-type! specialized-double-property-values)))
