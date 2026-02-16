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
  (:import [com.defold.libs MouseCapture MouseCapture$MouseDelta]
           [com.sun.jna Library Memory Native Pointer]
           [com.sun.jna.ptr PointerByReference]
           [javafx.event EventType]
           [javafx.scene.input DragEvent InputEvent KeyCode KeyEvent MouseEvent MouseButton ScrollEvent TransferMode]))

(set! *warn-on-reflection* true)

(def ActionType (s/enum :scroll :mouse-pressed :mouse-released :mouse-clicked :mouse-moved :key-pressed :key-released :undefined))

(def ButtonType (s/enum :none :primary :middle :secondary))

(def action-map {ScrollEvent/SCROLL :scroll
                 MouseEvent/MOUSE_PRESSED :mouse-pressed
                 MouseEvent/MOUSE_RELEASED :mouse-released
                 MouseEvent/MOUSE_CLICKED :mouse-clicked
                 MouseEvent/MOUSE_MOVED :mouse-moved
                 MouseEvent/MOUSE_DRAGGED :mouse-moved
                 KeyEvent/KEY_PRESSED :key-pressed
                 KeyEvent/KEY_RELEASED :key-released
                 DragEvent/DRAG_OVER :drag-over
                 DragEvent/DRAG_DROPPED :drag-dropped})

(defrecord InputState [mouse-buttons pressed-keys modifiers cursor-pos])

(defn make-input-state []
  (->InputState #{} #{} #{} nil))

(def ^:private mouse-capture-context (atom nil))

(comment
  (editor.ui/run-now (start-mouse-capture))
  (editor.ui/run-now (stop-mouse-capture))
  (editor.ui/run-now (get-x11-window))
  :-)

(defn- get-x11-window []
  (try
    (let [window-class (Class/forName "com.sun.glass.ui.Window")
          get-windows-method (.getDeclaredMethod window-class "getWindows" (make-array Class 0))
          windows (.invoke get-windows-method nil (make-array Object 0))
          first-window (.get windows 0)
          get-native-window-method (.getDeclaredMethod window-class "getNativeWindow" (make-array Class 0))]
      (.invoke get-native-window-method first-window (make-array Object 0)))
    (catch Exception e
      (println "Error getting X11 window:" e)
      nil)))

(defn start-mouse-capture []
  (when-let [window (get-x11-window)]
    (let [context (MouseCapture/MouseCapture_CreateContext (long window))]
      (when-not (nil? context)
        (when (MouseCapture/MouseCapture_StartCapture context)
          (reset! mouse-capture-context context)
          true)))))

(defn stop-mouse-capture []
  (when-let [context @mouse-capture-context]
    (MouseCapture/MouseCapture_StopCapture context)
    (MouseCapture/MouseCapture_DestroyContext context)
    (reset! mouse-capture-context nil)
    true))

(defn poll-mouse-delta []
  (when-let [context @mouse-capture-context]
    (let [delta (MouseCapture$MouseDelta.)]
      (when (MouseCapture/MouseCapture_PollDelta context delta)
        {:dx (.dx delta)
         :dy (.dy delta)}))))

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
                :event jfx-event
                :alt (.isAltDown jfx-event)
                :shift (.isShiftDown jfx-event)
                :meta (.isMetaDown jfx-event)
                :control (.isControlDown jfx-event)}]
    (case type
      :undefined action

      :scroll
      (let [scroll-event ^ScrollEvent jfx-event]
        (assoc action
          :x (.getX scroll-event)
          :y (.getY scroll-event)
          :delta-x (.getDeltaX scroll-event)
          :delta-y (.getDeltaY scroll-event)))

      :drag-over
      (let [drag-event ^DragEvent jfx-event]
        (.acceptTransferModes drag-event TransferMode/ANY)
        (assoc action
          :x (.getX drag-event)
          :y (.getY drag-event)))

      :drag-dropped
      (let [drag-event ^DragEvent jfx-event
            dragboard (.getDragboard drag-event)]
        (assoc action
          :x (.getX drag-event)
          :y (.getY drag-event)
          :files (.getFiles dragboard)
          :string (.getString dragboard)
          :transfer-mode (.getTransferMode drag-event)
          :gesture-target (.getGestureTarget drag-event)
          :gesture-source (.getGestureSource drag-event)))

      (:key-pressed :key-released)
      (let [key-event ^KeyEvent jfx-event]
        (assoc action
          :key-code (.getCode key-event)
          :character (.getCharacter key-event)))

      (let [mouse-event ^MouseEvent jfx-event]
        (assoc action
          :button (translate-button (.getButton mouse-event))
          :x (.getX mouse-event)
          :y (.getY mouse-event)
          :click-count (.getClickCount mouse-event)
          :target (.getTarget mouse-event)
          :screen-x (.getScreenX mouse-event)
          :screen-y (.getScreenY mouse-event))))))

(defn update-input-state [state action]
  (let [modifiers (->> [:alt :shift :meta :control]
                       (filter action)
                       set)
        cursor-pos (when (and (:screen-x action) (:screen-y action))
                     [(:screen-x action) (:screen-y action)])]
    (cond-> state
      cursor-pos
      (assoc :cursor-pos cursor-pos)

      :always
      (assoc :modifiers modifiers)

      (= :mouse-pressed (:type action))
      (update :mouse-buttons conj (:button action))

      (= :mouse-released (:type action))
      (update :mouse-buttons disj (:button action))

      (and (= :key-pressed (:type action))
           (let [code (:key-code action)]
             (or (.isLetterKey code) (.isDigitKey code))))
      (update :pressed-keys conj (:key-code action))

      (and (= :key-released (:type action))
           (let [code (:key-code action)]
             (or (.isLetterKey code) (.isDigitKey code))))
      (update :pressed-keys disj (:key-code action)))))
