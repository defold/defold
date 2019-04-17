(ns editor.fxui
  (:require [cljfx.api :as fx]
            [editor.util :as eutil]
            [editor.ui :as ui])
  (:import [javafx.stage Stage]
           [javafx.beans.value ChangeListener]
           [cljfx.lifecycle Lifecycle]))

(def ext-value
  "Extension lifecycle that just returns a value on `:value` key"
  (reify Lifecycle
    (create [_ desc _]
      (:value desc))
    (advance [_ _ desc _]
      (:value desc))
    (delete [_ _ _])))

(defn stage [props]
  {:fx/type fx/ext-on-instance-lifecycle
   :on-created (fn [^Stage stage]
                 (.addListener (.focusedProperty stage) ^ChangeListener ui/focus-change-listener))
   :desc (assoc props
           :fx/type :stage
           :icons (if (eutil/is-mac-os?) [] [ui/application-icon-image]))})

(defn dialog-stage [props]
  (assoc props
    :fx/type stage
    :resizable false
    :style :decorated
    :modality (if (contains? props :owner) :window-modal :application-modal)))

(defn dialog-body [{:keys [size header content footer]
                    :or {size :small}}]
  {:fx/type :v-box
   :style-class ["dialog-body" (case size
                                 :small "dialog-body-small"
                                 :large "dialog-body-large")]
   :children (if (some? content)
               [{:fx/type :v-box
                 :style-class "dialog-with-content-header"
                 :children [header]}
                {:fx/type :v-box
                 :style-class "dialog-content"
                 :children [content]}
                {:fx/type :v-box
                 :style-class "dialog-with-content-footer"
                 :children [footer]}]
               [{:fx/type :v-box
                 :style-class "dialog-without-content-header"
                 :children [header]}
                {:fx/type :region :style-class "dialog-no-content"}
                {:fx/type :v-box
                 :style-class "dialog-without-content-footer"
                 :children [footer]}])})

(defn dialog-buttons [props]
  (assoc props :fx/type :h-box :style-class "dialog-buttons"))

(defn header [props]
  (assoc props :fx/type :label :style-class "header"))

(defn button [{:keys [variant]
               :or {variant :secondary}
               :as props}]
  (-> props
      (dissoc :variant)
      (assoc :fx/type :button
             :style-class ["button" (case variant
                                      :primary "button-primary"
                                      :secondary "button-secondary")])))

(defn two-col-input-grid-pane [props]
  (-> props
      (assoc :fx/type :grid-pane
             :style-class "input-grid"
             :column-constraints [{:fx/type :column-constraints}
                                  {:fx/type :column-constraints
                                   :hgrow :always}])
      (update :children (fn [children]
                          (into []
                                (comp
                                  (partition-all 2)
                                  (map-indexed
                                    (fn [row [label input]]
                                      [(assoc label :grid-pane/column 0
                                                    :grid-pane/row row
                                                    :grid-pane/halignment :right)
                                       (assoc input :grid-pane/column 1
                                                    :grid-pane/row row)]))
                                  (mapcat identity))
                                children)))))

(defn text-field [{:keys [variant]
                   :or {variant :default}
                   :as props}]
  (-> props
      (dissoc :variant)
      (assoc :fx/type :text-field
             :style-class ["text-field" (case variant
                                          :default "text-field-default"
                                          :error "text-field-error")])))
