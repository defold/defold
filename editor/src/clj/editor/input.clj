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

(ns editor.input
  (:require [schema.core :as s])
  (:import [javafx.event EventType]
           [javafx.scene.input DragEvent InputEvent MouseEvent MouseButton ScrollEvent TransferMode]))

(set! *warn-on-reflection* true)

(def ActionType (s/enum :scroll :mouse-pressed :mouse-released :mouse-clicked :mouse-moved :undefined))

(def ButtonType (s/enum :none :primary :middle :secondary))

(def action-map {ScrollEvent/SCROLL :scroll
                 MouseEvent/MOUSE_PRESSED :mouse-pressed
                 MouseEvent/MOUSE_RELEASED :mouse-released
                 MouseEvent/MOUSE_CLICKED :mouse-clicked
                 MouseEvent/MOUSE_MOVED :mouse-moved
                 MouseEvent/MOUSE_DRAGGED :mouse-moved
                 DragEvent/DRAG_OVER :drag-over
                 DragEvent/DRAG_DROPPED :drag-dropped})

(defn translate-action [^EventType jfx-action]
  (get action-map jfx-action :undefined))

(def button-map {MouseButton/NONE :none
                 MouseButton/PRIMARY :primary
                 MouseButton/SECONDARY :secondary
                 MouseButton/MIDDLE :middle})

(defn translate-button [^MouseButton jfx-button]
  (get button-map jfx-button :none))

(defn action-from-jfx [^InputEvent jfx-event]
  (let [type (translate-action (.getEventType jfx-event))
        action {:type type
                :event jfx-event}]
    (case type
      :undefined action
      :scroll (let [scroll-event ^ScrollEvent jfx-event]
                (assoc action
                  :x (.getX scroll-event)
                  :y (.getY scroll-event)
                  :delta-x (.getDeltaX scroll-event)
                  :delta-y (.getDeltaY scroll-event)
                  :alt (.isAltDown scroll-event)
                  :shift (.isShiftDown scroll-event)
                  :meta (.isMetaDown scroll-event)
                  :control (.isControlDown scroll-event)))
      :drag-over (let [drag-event ^DragEvent jfx-event]
                   (.acceptTransferModes drag-event TransferMode/ANY)
                   (assoc action
                     :x (.getX drag-event)
                     :y (.getY drag-event)))
      :drag-dropped (let [drag-event ^DragEvent jfx-event
                          dragboard (.getDragboard drag-event)]
                      (assoc action
                        :x (.getX drag-event)
                        :y (.getY drag-event)
                        :files (.getFiles dragboard)
                        :string (.getString dragboard)
                        :transfer-mode (.getTransferMode drag-event)
                        :gesture-target (.getGestureTarget drag-event)
                        :gesture-source (.getGestureSource drag-event)))
      (let [mouse-event ^MouseEvent jfx-event]
        (assoc action
          :button (translate-button (.getButton mouse-event))
          :x (.getX mouse-event)
          :y (.getY mouse-event)
          :alt (.isAltDown mouse-event)
          :shift (.isShiftDown mouse-event)
          :meta (.isMetaDown mouse-event)
          :control (.isControlDown mouse-event)
          :click-count (.getClickCount mouse-event)
          :target (.getTarget mouse-event)
          :screen-x (.getScreenX mouse-event)
          :screen-y (.getScreenY mouse-event))))))
