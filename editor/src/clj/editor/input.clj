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
  (:require [editor.os :as os])
  (:import [com.defold.libs MouseCapture MouseCapture$MouseDelta]
           [java.awt MouseInfo]
           [javafx.event EventType]
           [javafx.scene.input DragEvent InputEvent KeyCode KeyEvent MouseEvent MouseButton ScrollEvent TransferMode]))

(set! *warn-on-reflection* true)

(def action-map {ScrollEvent/SCROLL :scroll
                 MouseEvent/MOUSE_PRESSED :mouse-pressed
                 MouseEvent/MOUSE_RELEASED :mouse-released
                 MouseEvent/MOUSE_CLICKED :mouse-clicked
                 MouseEvent/MOUSE_MOVED :mouse-moved
                 MouseEvent/MOUSE_DRAGGED :mouse-moved
                 KeyEvent/KEY_PRESSED :key-pressed
                 KeyEvent/KEY_RELEASED :key-released
                 MouseEvent/DRAG_DETECTED :drag-detected
                 DragEvent/DRAG_OVER :drag-over
                 DragEvent/DRAG_DROPPED :drag-dropped})

(defrecord InputState [mouse-buttons pressed-keys modifiers cursor-pos view-pos scroll-delta])

(defn make-input-state []
  (->InputState #{} #{} #{} [0.0 0.0] [0.0 0.0] [0.0 0.0]))

(def ^:private mouse-capture-context (atom nil))

(def is-wayland (and (os/is-linux?)
                     (or (some? (System/getenv "WAYLAND_DISPLAY"))
                         (= "wayland" (System/getenv "XDG_SESSION_TYPE")))))

;; NOTE: JavaFX provides Robot for this sort of thing, however, it requires Accessibility Permissions
;; on macos, so we need to make native calls
(defn warp-cursor [x y] (MouseCapture/MouseCapture_WarpCursor x y))

(defn start-mouse-capture [view-center-x view-center-y]
  (when-let [context (MouseCapture/MouseCapture_StartCapture view-center-x view-center-y)]
    (reset! mouse-capture-context context)
    true))

(defn stop-mouse-capture []
  (when-let [context @mouse-capture-context]
    (MouseCapture/MouseCapture_StopCapture context)
    (reset! mouse-capture-context nil)
    true))

(def ^:private cached-delta (atom (MouseCapture$MouseDelta.)))

(defn poll-mouse-delta ^MouseCapture$MouseDelta []
  (when-let [context @mouse-capture-context]
    (let [delta @cached-delta]
      (when (MouseCapture/MouseCapture_PollDelta context delta)
        @cached-delta))))

(defn get-cursor-pos []
  (let [point (.getLocation (MouseInfo/getPointerInfo))]
    [(.getX point) (.getY point)]))

(defn translate-action [^EventType jfx-action]
  (get action-map jfx-action :undefined))

(def button-map {MouseButton/NONE :none
                 MouseButton/PRIMARY :primary
                 MouseButton/SECONDARY :secondary
                 MouseButton/MIDDLE :middle})

(defn translate-button [^MouseButton jfx-button]
  (get button-map jfx-button :none))

;; NOTE: Not all InputEvents subclasses have the modifier checking methods, so use a protocol to avoid reflection
(defprotocol ModifierKeys
  (get-modifiers [event]))

(extend-protocol ModifierKeys
  KeyEvent
  (get-modifiers [e]
    {:alt (.isAltDown e) :shift (.isShiftDown e)
     :meta (.isMetaDown e) :control (.isControlDown e)})
  MouseEvent
  (get-modifiers [e]
    {:alt (.isAltDown e) :shift (.isShiftDown e)
     :meta (.isMetaDown e) :control (.isControlDown e)})
  ScrollEvent
  (get-modifiers [e]
    {:alt (.isAltDown e) :shift (.isShiftDown e)
     :meta (.isMetaDown e) :control (.isControlDown e)})
  InputEvent
  (get-modifiers [_]
    ;; NOTE: Return an empty map so it doesn't potentially clobber InputState
    {}))

(defn action-from-jfx [^InputEvent jfx-event]
  (let [type (translate-action (.getEventType jfx-event))
        modifiers (get-modifiers jfx-event)
        action (merge {:type type :event jfx-event} modifiers)]
    (case type
      :undefined action

      :scroll
      (let [scroll-event ^ScrollEvent jfx-event]
        (assoc action
          :x (.getX scroll-event)
          :y (.getY scroll-event)
          :delta-x (.getDeltaX scroll-event)
          :delta-y (.getDeltaY scroll-event)))

      :drag-detected
      (let [mouse-event ^MouseEvent jfx-event]
        (assoc action
          :button (translate-button (.getButton mouse-event))
          :x (.getX mouse-event)
          :y (.getY mouse-event)
          :screen-x (.getScreenX mouse-event)
          :screen-y (.getScreenY mouse-event)))

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

(defn- trackable-key? [^KeyCode code]
  (not (.isModifierKey code)))

(defn update-input-state [state action]
  (let [modifiers (->> [:alt :shift :meta :control]
                       (filter action)
                       set)
        cursor-pos (when (and (:screen-x action) (:screen-y action))
                     [(:screen-x action) (:screen-y action)])
        view-pos [(:x action) (:y action)]
        state (assoc state :view-pos view-pos)]
    (cond-> state
      cursor-pos
      (assoc :cursor-pos cursor-pos)

      :always
      (assoc :modifiers modifiers)

      (= :mouse-pressed (:type action))
      (update :mouse-buttons conj (:button action))

      (= :mouse-released (:type action))
      (update :mouse-buttons disj (:button action))

      (= :scroll (:type action))
      (update :scroll-delta #(mapv + % [(:delta-x action) (:delta-y action)]))

      (and (= :key-pressed (:type action))
           (trackable-key? (:key-code action)))
      (update :pressed-keys conj (:key-code action))

      (and (= :key-released (:type action))
           (trackable-key? (:key-code action)))
      (update :pressed-keys disj (:key-code action)))))
