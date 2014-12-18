(ns editors.presenters
  (:require [schema.core :as s]
            [dynamo.project :as p]
            [dynamo.property :as dp :refer :all]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [dynamo.util :refer :all])
  (:import [org.eclipse.swt SWT]))

(defrecord StringPresenter []
  Presenter
  (control-for-property [this]
    {:type :text :listen #{:key-down :focus-out}})
  (settings-for-control [_ value]
    {:text (str value)})

  (on-event [_ _ event value]
    (let [new-value (ui/get-text (:widget event))]
      (case (:type event)
        :key-down (if (is-enter-key? event)
                    (final-value new-value)
                    (no-change))
        :focus-out (final-value new-value)
        (no-change)))))

(defrecord IntPresenter []
  Presenter
  (control-for-property [this]
    {:type :text :listen #{:key-down :focus-out}})
  (settings-for-control [_ value]
    {:text (str value)})

  (on-event [_ _ event value]
    (let [new-value (parse-int (ui/get-text (:widget event)))]
      (case (:type event)
        :key-down (if (is-enter-key? event)
                    (final-value new-value)
                    (no-change))
        :focus-out (final-value new-value)
        (no-change)))))

(defrecord Vec3Presenter []
  Presenter
  (control-for-property [_]
    {:type :composite
     :layout {:type :grid :num-columns 3 :margin-width 0}
     :children [[:x {:type :text :listen #{:key-down :focus-out} :layout-data {:type :grid :grab-excess-horizontal-space true :horizontal-alignment SWT/FILL}}]
                [:y {:type :text :listen #{:key-down :focus-out} :layout-data {:type :grid :grab-excess-horizontal-space true :horizontal-alignment SWT/FILL}}]
                [:z {:type :text :listen #{:key-down :focus-out} :layout-data {:type :grid :grab-excess-horizontal-space true :horizontal-alignment SWT/FILL}}]]})
  (settings-for-control [_ value]
    {:children [[:x {:text (str (nth value 0))}]
                [:y {:text (str (nth value 1))}]
                [:z {:text (str (nth value 2))}]]})
  (on-event [_ path event value]
    (when-let [index (get {:x 0 :y 1 :z 2} (first path))]
      (let [new-value (assoc value index (parse-number (ui/get-text (:widget event))))]
        (case (:type event)
          :key-down (if (is-enter-key? event)
                      (final-value new-value)
                      (no-change))
          :focus-out (final-value new-value)
          (no-change))))))

(defrecord ColorPresenter []
  Presenter
  (control-for-property [_]
    {:type :composite
     :layout {:type :grid :num-columns 2 :margin-width 0 :margin-height 0 :horizontal-spacing 0}
     :children [[:label {:type :label}]
                [:selector {:type :color-selector :listen #{:selection}}]]})
  (settings-for-control [_ [r g b :as value]]
    {:children [[:label {:text (format "#%02x%02x%02x" r g b)}]
                [:selector {:color value}]]})
  (on-event [_ path event value]
    (case (:type event)
      :selection (final-value (ui/get-color (:widget event)))
      (no-change))))

(p/register-presenter s/Str  (->StringPresenter))
(p/register-presenter s/Int  (->IntPresenter))
(p/register-presenter t/Vec3 (->Vec3Presenter))
(p/register-presenter (t/property-value-type dp/Color) (->ColorPresenter))
