(ns editors.presenters
  (:require [schema.core :as s]
            [dynamo.project :as p]
            [dynamo.property :refer :all]
            [dynamo.system :as ds]
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

(when (ds/in-transaction?)
  (p/register-presenter s/Str  (->StringPresenter))
  (p/register-presenter s/Int  (->IntPresenter))
  (p/register-presenter t/Vec3 (->Vec3Presenter)))
