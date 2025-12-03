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

(ns integration.property-type-preservation-test
  (:require [cljfx.api :as fx]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.properties :as properties]
            [editor.properties-view :as properties-view]
            [editor.types :as t]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]])
  (:import [editor.properties Curve CurveSpread]
           [javafx.scene Parent]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private rotation-edit-type properties/quat-rotation-edit-type)

(def ^:private slider-edit-type
  {:type :slider
   :min 0.0
   :max 1.0
   :precision 0.01})

(g/defnode NumericPropertiesNode
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
  (mapv double args))

(defn- float-vec [& args]
  (mapv float args))

(def ^:private generic-float-property-values
  {:num (float 0.1)
   :vec2 (float-vec 0.1 0.2)
   :vec3 (float-vec 0.1 0.2 0.3)
   :vec4 (float-vec 0.1 0.2 0.3 0.4)
   :rotation (float-vec 0.1 0.2 0.3 1.0)
   :slider (float 0.1)
   :color (float-vec 0.1 0.2 0.3 0.4)
   :curve (properties/->curve [(float-vec 0.0 0.1 1.0 0.0)])
   :curve-spread (properties/->curve-spread [(float-vec 0.0 0.1 1.0 0.0)] (float 0.2))})

(def ^:private generic-double-property-values
  {:num (double 0.1)
   :vec2 (double-vec 0.1 0.2)
   :vec3 (double-vec 0.1 0.2 0.3)
   :vec4 (double-vec 0.1 0.2 0.3 0.4)
   :rotation (double-vec 0.1 0.2 0.3 1.0)
   :slider (double 0.1)
   :color (double-vec 0.1 0.2 0.3 0.4)
   :curve (properties/->curve [(double-vec 0.0 0.1 1.0 0.0)])
   :curve-spread (properties/->curve-spread [(double-vec 0.0 0.1 1.0 0.0)] (double 0.2))})

(def ^:private specialized-float-property-values
  {:num (float 0.1)
   :vec2 (vector-of :float 0.1 0.2)
   :vec3 (vector-of :float 0.1 0.2 0.3)
   :vec4 (vector-of :float 0.1 0.2 0.3 0.4)
   :rotation (vector-of :float 0.1 0.2 0.3 1.0)
   :slider (float 0.1)
   :color (vector-of :float 0.1 0.2 0.3 0.4)
   :curve (properties/->curve [(vector-of :float 0.0 0.1 1.0 0.0)])
   :curve-spread (properties/->curve-spread [(vector-of :float 0.0 0.1 1.0 0.0)] (float 0.2))})

(def ^:private specialized-double-property-values
  {:num (double 0.1)
   :vec2 (vector-of :double 0.1 0.2)
   :vec3 (vector-of :double 0.1 0.2 0.3)
   :vec4 (vector-of :double 0.1 0.2 0.3 0.4)
   :rotation (vector-of :double 0.1 0.2 0.3 1.0)
   :slider (double 0.1)
   :color (vector-of :double 0.1 0.2 0.3 0.4)
   :curve (properties/->curve [(vector-of :double 0.0 0.1 1.0 0.0)])
   :curve-spread (properties/->curve-spread [(vector-of :double 0.0 0.1 1.0 0.0)] (double 0.2))})

(defn- coalesced-property [node-id prop-kw]
  (prop-kw (:properties (properties/coalesce [(g/node-value node-id :_properties)]))))

(defn- make-property-widget
  ^Parent [node-id prop-kw]
  (let [property (coalesced-property node-id prop-kw)
        context {:prefs (test-util/make-build-stage-test-prefs)}
        localization-state @test-util/localization]
    (-> (properties-view/make-control-view property context localization-state)
        fx/create-component
        fx/instance)))

(defmulti test-property-widget! (fn [node-id prop-kw]
                                  (properties-view/edit-type->type (:edit-type (coalesced-property node-id prop-kw)))))

(defmethod test-property-widget! g/Num [node-id prop-kw]
  (let [graph-id (g/node-id->graph-id node-id)
        original-value (g/node-value node-id prop-kw)
        widget (make-property-widget node-id prop-kw)
        [num-field] (test-util/editable-controls widget)]
    (with-open [_ (test-util/make-graph-reverter graph-id)]
      (test-util/set-control-value! num-field 0.11)
      (let [modified-value (g/node-value node-id prop-kw)]
        (is (not= original-value modified-value))
        (test-util/ensure-number-type-preserving! original-value modified-value)))))

(defn- test-vector-property-widget! [node-id prop-kw]
  (let [graph-id (g/node-id->graph-id node-id)
        original-value (g/node-value node-id prop-kw)
        widget (make-property-widget node-id prop-kw)
        check! (fn check! [num-field num-value]
                 (with-open [_ (test-util/make-graph-reverter graph-id)]
                   (test-util/set-control-value! num-field num-value)
                   (let [modified-value (g/node-value node-id prop-kw)]
                     (is (not= original-value modified-value))
                     (is (= (count original-value) (count modified-value)))
                     (test-util/ensure-number-type-preserving! original-value modified-value))))]
    (doall
      (map check!
           (test-util/editable-controls widget)
           (range 0.11 0.99 0.11)))))

(defmethod test-property-widget! t/Vec2 [node-id prop-kw]
  (test-vector-property-widget! node-id prop-kw))

(defmethod test-property-widget! t/Vec3 [node-id prop-kw]
  (test-vector-property-widget! node-id prop-kw))

(defmethod test-property-widget! t/Vec4 [node-id prop-kw]
  (test-vector-property-widget! node-id prop-kw))

(defmethod test-property-widget! t/Color [node-id prop-kw]
  (let [graph-id (g/node-id->graph-id node-id)
        original-value (g/node-value node-id prop-kw)
        widget (make-property-widget node-id prop-kw)
        [color-input] (test-util/editable-controls widget)]
    (with-open [_ (test-util/make-graph-reverter graph-id)]
      (test-util/set-control-value! color-input "#fff")
      (let [modified-value (g/node-value node-id prop-kw)]
        (is (not= original-value modified-value))
        (is (= (count original-value) (count modified-value)))
        (test-util/ensure-number-type-preserving! original-value modified-value)))))

(defmethod test-property-widget! :slider [node-id prop-kw]
  (let [graph-id (g/node-id->graph-id node-id)
        original-value (g/node-value node-id prop-kw)
        widget (make-property-widget node-id prop-kw)
        [value-field slider] (test-util/editable-controls widget)
        check! (fn check! [perform-edit!]
                 (with-open [_ (test-util/make-graph-reverter graph-id)]
                   (perform-edit!)
                   (let [modified-value (g/node-value node-id prop-kw)]
                     (is (not= original-value modified-value))
                     (test-util/ensure-number-type-preserving! original-value modified-value))))]
    (check! #(test-util/set-control-value! value-field 0.11))
    (check! #(test-util/set-control-value! slider 0.22))))

(defmethod test-property-widget! Curve [node-id prop-kw]
  (let [graph-id (g/node-id->graph-id node-id)
        original-value (g/node-value node-id prop-kw)
        widget (make-property-widget node-id prop-kw)
        [edit-curve-button value-field] (test-util/editable-controls widget)
        check! (fn check! [perform-edit!]
                 (with-open [_ (test-util/make-graph-reverter graph-id)]
                   (perform-edit!)
                   (let [modified-value (g/node-value node-id prop-kw)]
                     (is (not= original-value modified-value))
                     (test-util/ensure-number-type-preserving! original-value modified-value))))]
    (check! #(test-util/set-control-value! edit-curve-button 0.0))
    (check! #(test-util/set-control-value! value-field 0.11))))

(defmethod test-property-widget! CurveSpread [node-id prop-kw]
  (let [graph-id (g/node-id->graph-id node-id)
        original-value (g/node-value node-id prop-kw)
        widget (make-property-widget node-id prop-kw)
        [edit-curve-button value-field spread-field] (test-util/editable-controls widget)
        check! (fn check! [perform-edit!]
                 (with-open [_ (test-util/make-graph-reverter graph-id)]
                   (perform-edit!)
                   (let [modified-value (g/node-value node-id prop-kw)]
                     (is (not= original-value modified-value))
                     (test-util/ensure-number-type-preserving! original-value modified-value))))]
    (check! #(test-util/set-control-value! edit-curve-button 0.0))
    (check! #(test-util/set-control-value! value-field 0.11))
    (check! #(test-util/set-control-value! spread-field 0.22))))

(defn- ensure-numeric-properties-preserve-type! [original-property-values]
  (with-clean-system
    (test-util/with-ui-run-later-rebound
      (let [original-meta {:version "original"}

            property-values
            (into (sorted-map)
                  (map (fn [[prop-kw prop-value]]
                         (let [decorated-value (if (vector? prop-value)
                                                 (with-meta prop-value original-meta)
                                                 prop-value)]
                           [prop-kw decorated-value])))
                  original-property-values)

            graph-id (g/make-graph! :history true)
            node-id (apply g/make-node! graph-id NumericPropertiesNode (mapcat identity property-values))]
        (doseq [prop-kw (sort (keys (:properties (g/node-value node-id :_properties))))]
          (testing (format "Types preserved after editing (property %s)" (name prop-kw))
            (test-property-widget! node-id prop-kw)))))))

(deftest numeric-properties-preserve-type-test
  (testing "Values are floats in generic vectors before editing"
    (ensure-numeric-properties-preserve-type! generic-float-property-values))
  (testing "Values are doubles in generic vectors before editing"
    (ensure-numeric-properties-preserve-type! generic-double-property-values))
  (testing "Values are floats in specialized vectors before editing"
    (ensure-numeric-properties-preserve-type! specialized-float-property-values))
  (testing "Values are doubles in specialized vectors before editing"
    (ensure-numeric-properties-preserve-type! specialized-double-property-values)))
