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

(ns editor.notifications-view
  (:require [cljfx.api :as fx]
            [cljfx.fx.button :as fx.button]
            [cljfx.fx.flow-pane :as fx.flow-pane]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [cljfx.fx.v-box :as fx.v-box]
            [dynamo.graph :as g]
            [editor.error-reporting :as error-reporting]
            [editor.fxui :as fxui]
            [editor.notifications :as notifications]
            [editor.ui :as ui]))

(def ^:private ext-with-v-box-props
  (fx/make-ext-with-props fx.v-box/props))

(defn- close-notification! [{:keys [notifications-node id]}]
  (notifications/close! notifications-node id))

(defn- invoke-action! [{:keys [on-action] :as event}]
  (try
    (on-action)
    (catch Exception e
      (error-reporting/report-exception! e)))
  (close-notification! event))

(defn- notifications-view [{:keys [id->notification ids]} parent localization]
  (let [n (count ids)]
    {:fx/type ext-with-v-box-props
     :desc {:fx/type fxui/ext-value :value parent}
     :props
     {:padding 10
      :children
      (cond-> []
              (pos? n)
              (conj
                (let [typical-button-count 3
                      horizontal-padding 14
                      vertical-padding 8
                      close-button-margin 4
                      spacing 4
                      id (peek ids)
                      {:keys [type message actions]} (id->notification id)]
                  {:fx/type fx.stack-pane/lifecycle
                   :max-width (+ (* typical-button-count 100) ;; min button width
                                 (* 2 horizontal-padding)
                                 (* (dec typical-button-count) spacing))
                   :children
                   [{:fx/type fx.region/lifecycle
                     :style-class "notification-card-background"
                     :pseudo-classes #{type}}
                    {:fx/type fx.v-box/lifecycle
                     :children
                     (cond-> [{:fx/type fx.h-box/lifecycle
                               :alignment :top-left
                               :padding {:top vertical-padding
                                         :bottom vertical-padding
                                         :left horizontal-padding
                                         :right (- horizontal-padding close-button-margin)}
                               :spacing spacing
                               :fill-height false
                               :children [{:fx/type fxui/ext-localize
                                           :h-box/hgrow :always
                                           :localization localization
                                           :message message
                                           :desc {:fx/type fxui/legacy-label
                                                  :max-width Double/MAX_VALUE}}
                                          {:fx/type fx.region/lifecycle
                                           :h-box/margin close-button-margin
                                           :on-mouse-clicked {:fn #'close-notification! :id id}
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
                                                  {:fx/type fxui/ext-localize
                                                   :localization localization
                                                   :message message
                                                   :desc
                                                   {:fx/type fx.button/lifecycle
                                                    :style-class ["button" "notification-card-button"]
                                                    :on-action {:fn #'invoke-action!
                                                                :id id
                                                                :on-action on-action}}})
                                                actions)}))}]})))}}))

(defn init! [notifications-node parent localization]
  (let [renderer (fx/create-renderer
                   :error-handler error-reporting/report-exception!
                   :opts {:fx.opt/map-event-handler #((:fn %) (assoc % :notifications-node notifications-node))}
                   :middleware (comp
                                 fxui/wrap-dedupe-desc
                                 (fx/wrap-map-desc #'notifications-view parent localization)))]
    (ui/timer-start!
      (ui/->timer 30 "notifications-view-timer"
                  (fn [_timer _elapsed _dt]
                    (renderer (g/node-value notifications-node :notifications)))))
    nil))