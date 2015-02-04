(ns editors.presenters
  (:require [schema.core :as s]
            [dynamo.project :as p]
            [dynamo.property :as dp :refer :all]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.ui.widgets :as widgets]
            [dynamo.util :refer :all]))

(defrecord StringPresenter []
  Presenter
  (control-for-property [this]
    {:type :text :listen #{:key-down :focus-out}})
  (settings-for-control [_ value]
    {:text (str value)})

  (on-event [_ widget-subtree path event _]
    (let [new-value (widgets/get-text (widgets/widget widget-subtree path))]
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

  (on-event [_ widget-subtree path event _]
    (let [new-value (parse-int (widgets/get-text (widgets/widget widget-subtree path)))]
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
     :layout {:type :grid :margin-width 0 :columns [{:horizontal-alignment :fill}
                                                    {:horizontal-alignment :fill}
                                                    {:horizontal-alignment :fill}]}
     :children [[:x {:type :text :listen #{:key-down :focus-out}}]
                [:y {:type :text :listen #{:key-down :focus-out}}]
                [:z {:type :text :listen #{:key-down :focus-out}}]]})
  (settings-for-control [_ value]
    {:children [[:x {:text (str (nth value 0))}]
                [:y {:text (str (nth value 1))}]
                [:z {:text (str (nth value 2))}]]})
  (on-event [_ widget-subtree path event value]
    (when-let [index (get {:x 0 :y 1 :z 2} (first path))]
      (let [widget (widgets/widget widget-subtree path)
            new-value (assoc value index (parse-number (widgets/get-text widget)))]
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
     :layout {:type :grid :margin-width 0 :margin-height 0 :horizontal-spacing 0 :columns [{:min-width 55} {}]}
     :children [[:label {:type :label}]
                [:selector {:type :color-selector :listen #{:selection}}]]})
  (settings-for-control [_ [r g b :as value]]
    {:children [[:label {:text (format "#%02x%02x%02x" (int r) (int g) (int b))}]
                [:selector {:color (mapv int value)}]]})
  (on-event [_ widget-subtree path event _]
    (case (:type event)
      :selection (final-value (widgets/get-color (widgets/widget widget-subtree path)))
      (no-change))))

(defrecord ResourcePresenter [extensions title]
  Presenter
  (control-for-property [this]
    {:type :composite
     :layout {:type :grid :margin-width 0 :margin-height 0 :horizontal-spacing 5 :columns [{:horizontal-alignment :fill} {:min-width 25}]}
     :children [[:text {:type :text :listen #{:key-down :focus-out}}]
                [:selector {:type :resource-selector :extensions extensions :title title :listen #{:selection}}]]})
  (settings-for-control [_ value]
    {:children [[:text {:text (str value)}]]})

  (on-event [_ widget-subtree path event _]
    (case (last path)
      :text     (let [new-value (widgets/get-text (widgets/widget widget-subtree path))]
                  (case (:type event)
                    :key-down (if (is-enter-key? event)
                                (final-value new-value)
                                (no-change))
                    :focus-out (final-value new-value)
                    (no-change)))
      :selector (if-let [selection (first (:data event))]
                  (final-value (t/local-path (:filename selection)))
                  (no-change))
      (no-change))))

(when (ds/in-transaction?)
  (p/register-presenter {:value-type s/Str}                     (->StringPresenter))
  (p/register-presenter {:value-type s/Int}                     (->IntPresenter))
  (p/register-presenter {:value-type t/Vec3}                    (->Vec3Presenter))
  (p/register-presenter {:value-type t/Vec3 :tag :color}        (->ColorPresenter))
  (p/register-presenter {:value-type s/Str  :tag ::dp/image}    (map->ResourcePresenter {:extensions ["png" "jpg"] :title "Select Image"})))
