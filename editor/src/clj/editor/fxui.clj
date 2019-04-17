(ns editor.fxui
  (:require [cljfx.api :as fx]
            [editor.error-reporting :as error-reporting]
            [editor.ui :as ui]
            [editor.util :as eutil])
  (:import [cljfx.lifecycle Lifecycle]
           [javafx.application Platform]))

(def ext-value
  "Extension lifecycle that returns value on `:value` key"
  (reify Lifecycle
    (create [_ desc _]
      (:value desc))
    (advance [_ _ desc _]
      (:value desc))
    (delete [_ _ _])))

(defn mount-renderer-and-await-result!
  "Mounts `renderer` and blocks current thread until `state-atom`'s value
  receives has a `:result` key"
  [state-atom renderer]
  (let [event-loop-key (Object.)
        result-promise (promise)]
    (future
      (error-reporting/catch-all!
        (let [result @result-promise]
          (fx/on-fx-thread
            (Platform/exitNestedEventLoop event-loop-key result)))))
    (add-watch state-atom event-loop-key
               (fn [k r _ n]
                 (let [result (:result n ::no-result)]
                   (when-not (= result ::no-result)
                     (deliver result-promise result)
                     (remove-watch r k)))))
    (fx/mount-renderer state-atom renderer)
    (Platform/enterNestedEventLoop event-loop-key)))

(defn wrap-state-handler [state-atom f]
  (-> f
      (fx/wrap-co-effects
        {:state (fx/make-deref-co-effect state-atom)})
      (fx/wrap-effects
        {:state (fx/make-reset-effect state-atom)})))

(defn stage [props]
  (assoc props
    :fx/type :stage
    :on-focused-changed ui/focus-change-listener
    :icons (if (eutil/is-mac-os?) [] [ui/application-icon-image])))

(defn dialog-stage [props]
  (assoc props
    :fx/type stage
    :resizable false
    :style :decorated
    :modality (if (contains? props :owner) :window-modal :application-modal)))

(defn dialog-body [{:keys [size header content footer]
                    :or {size :small}
                    :as props}]
  (-> props
      (dissoc :size :header :content :footer)
      (assoc :fx/type :v-box
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
                           :children [footer]}]))))

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
