;; Copyright 2020-2023 The Defold Foundation
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
            [dynamo.graph :as g]
            [editor.error-reporting :as error-reporting]
            [editor.fxui :as fxui]
            [cljfx.fx.v-box :as fx.v-box]
            [editor.ui :as ui]
            [cljfx.fx.flow-pane :as fx.flow-pane]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.button :as fx.button]
            [editor.notifications :as notifications]))

(def ^:private ext-with-v-box-props
  (fx/make-ext-with-props fx.v-box/props))

(defn- close-notification! [{:keys [notifications-node]}]
  (notifications/close-latest! notifications-node))

(defn- invoke-action! [{:keys [on-action] :as event}]
  (try
    (on-action)
    (catch Exception e
      (error-reporting/report-exception! e)))
  (close-notification! event))

(defn- notifications-view [{:keys [id->notification ids]} parent]
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
                      {:keys [type text actions]} (id->notification (peek ids))]
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
                               :children [{:fx/type fxui/label
                                           :h-box/hgrow :always
                                           :max-width Double/MAX_VALUE
                                           :text text}
                                          {:fx/type fx.region/lifecycle
                                           :h-box/margin close-button-margin
                                           :on-mouse-clicked {:fn #'close-notification!}
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
                                                (fn [{:keys [text on-action]}]
                                                  {:fx/type fx.button/lifecycle
                                                   :style-class ["button" "notification-card-button"]
                                                   :text text
                                                   :on-action {:fn #'invoke-action!
                                                               :on-action on-action}})
                                                actions)}))}]})))}}))

(defn init! [notifications-node parent]
  (let [renderer (fx/create-renderer
                   :error-handler error-reporting/report-exception!
                   :opts {:fx.opt/map-event-handler #((:fn %) (assoc % :notifications-node notifications-node))}
                   :middleware (comp
                                 fxui/wrap-dedupe-desc
                                 (fx/wrap-map-desc #'notifications-view parent)))]
    (ui/timer-start!
      (ui/->timer 30 "notifications-view-timer"
                  (fn [_timer _elapsed _dt]
                    (renderer (g/node-value notifications-node :notifications)))))
    nil))