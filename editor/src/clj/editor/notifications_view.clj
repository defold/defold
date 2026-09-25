;; Copyright 2020-2026 The Defold Foundation
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

(ns editor.notifications-view
  (:require [cljfx.api :as fx]
            [cljfx.fx.button :as fx.button]
            [cljfx.fx.flow-pane :as fx.flow-pane]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [cljfx.fx.v-box :as fx.v-box]
            [dynamo.graph :as g]
            [editor.error-reporting :as error-reporting]
            [editor.fxui :as fxui]
            [editor.localization :as localization]
            [editor.notifications :as notifications]
            [editor.ui :as ui]))

(def ^:private ext-with-v-box-props
  (fx/make-ext-with-props fx.v-box/props))

(defn- invoke-action! [notifications-node id on-action]
  (try
    (on-action)
    (catch Exception e
      (error-reporting/report-exception! e)))
  (notifications/close! notifications-node id))

(defn- notification-view
  [{:keys [id localization-state notification notifications-node]}]
  (let [typical-button-count 3
        horizontal-padding 14
        vertical-padding 8
        close-button-margin 4
        spacing 4
        {:keys [type message actions]} notification]
    {:fx/type fx.stack-pane/lifecycle
     :max-width (+ (* typical-button-count 100) ;; min button width
                   (* 2 horizontal-padding)
                   (* (dec typical-button-count) spacing))
     :children
     [{:fx/type fx.region/lifecycle
       :style-class "notification-card-background"
       :pseudo-classes #{type}}
      {:fx/type fxui/vertical
       :children
       (cond-> [{:fx/type fxui/horizontal
                 :alignment :top-left
                 :padding {:top vertical-padding
                           :bottom vertical-padding
                           :left horizontal-padding
                           :right (- horizontal-padding close-button-margin)}
                 :spacing spacing
                 :fill-height false
                 :children [{:fx/type fxui/paragraph
                             :h-box/hgrow :always
                             :text (localization-state message)}
                            {:fx/type fx.region/lifecycle
                             :h-box/margin close-button-margin
                             :on-mouse-clicked (fn [_]
                                                 (notifications/close! notifications-node id))
                             :min-width 10
                             :min-height 10
                             :style-class "notification-card-close-button"}]}]
               (pos? (count actions))
               (conj {:fx/type fx.flow-pane/lifecycle
                      :hgap spacing
                      :vgap spacing
                      :padding {:left horizontal-padding
                                :right horizontal-padding
                                :bottom vertical-padding}
                      :children (mapv
                                  (fn [{:keys [message on-action]}]
                                    {:fx/type fx.button/lifecycle
                                     :style-class ["button" "notification-card-button"]
                                     :text (localization-state message)
                                     :on-action (fn [_]
                                                  (invoke-action! notifications-node id on-action))})
                                  actions)}))}]}))

(ui/defc ^:private notifications-view
  {:compose [{:fx/type fx/ext-watcher :ref (:localization props) :key :localization-state}]}
  [{:keys [localization-state notifications notifications-node parent]}]
  (let [{:keys [id->notification ids]} notifications
        hidden-count (max 0 (- (count ids) 3))]
    {:fx/type ext-with-v-box-props
     :desc {:fx/type ui/ext-value :value parent}
     :props
     {:alignment :center-right
      :fill-width false
      :padding 10
      :spacing 8
      :children
      (-> []
          (cond-> (pos? hidden-count)
            (conj {:fx/type fx.stack-pane/lifecycle
                   :children
                   [{:fx/type fx.region/lifecycle
                     :style-class "notification-card-background"
                     :pseudo-classes #{(reduce #(max-key {:info 0 :warning 1 :error 2} %1 (:type (id->notification %2)))
                                               :info
                                               (subvec ids 0 hidden-count))}}
                    {:fx/type fxui/vertical
                     :padding {:top 0 :right 6 :bottom 0 :left 6}
                     :children
                     [{:fx/type fxui/label
                       :text (localization-state
                               (localization/message
                                 "notification.more"
                                 {"count" hidden-count}))}]}]}))
          (into
            (map
              (fn [id]
                {:fx/type notification-view
                 :id id
                 :localization-state localization-state
                 :notification (id->notification id)
                 :notifications-node notifications-node}))
            (subvec ids hidden-count)))}}))

(defn init! [notifications-node parent localization]
  (ui/timer-start!
    (ui/->timer 30 "notifications-view-timer"
                (fn [_timer _elapsed _dt]
                  (ui/advance-ui-user-data-component!
                    parent
                    ::view
                    {:fx/type notifications-view
                     :localization localization
                     :notifications (g/node-value notifications-node :notifications)
                     :notifications-node notifications-node
                     :parent parent}))))
  nil)
