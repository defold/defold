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

(ns editor.progress
  (:require [editor.localization :as localization]
            [util.coll :refer [pair]]
            [util.defonce :as defonce])
  (:import [com.defold.editor.localization MessagePattern]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/record Progress
  [^MessagePattern message
   ^long size
   ^long pos
   cancel-state])

(defn ->progress
  ^Progress [^MessagePattern message ^long size ^long pos cancel-state]
  {:pre [(localization/message-pattern? message)
         (nat-int? size) ; size 0 means indeterminate.
         (nat-int? pos)
         (<= pos size)
         (case cancel-state (:not-cancellable :cancellable :cancelled) true false)]}
  (->Progress message size pos cancel-state))

(defn make
  (^Progress [^MessagePattern message]
   (->progress message 1 0 :not-cancellable))
  (^Progress [^MessagePattern message ^long size]
   (->progress message size 0 :not-cancellable))
  (^Progress [^MessagePattern message ^long size ^long pos]
   (->progress message size pos :not-cancellable))
  (^Progress [^MessagePattern message ^long size ^long pos cancellable]
   (->progress message size pos (if cancellable :cancellable :not-cancellable))))

(defn make-indeterminate
  ^Progress [^MessagePattern message]
  (->progress message 0 0 :not-cancellable))

(defn make-cancellable-indeterminate
  ^Progress [^MessagePattern message]
  (->progress message 0 0 :cancellable))

(def ^Progress done (->progress (localization/message "progress.ready") 1 1 :not-cancellable))

(defn with-message
  ^Progress [^Progress progress ^MessagePattern message]
  (assoc progress :message message))

(defn jump
  (^Progress [^Progress progress ^long pos]
   (assoc progress :pos (min pos (.-size progress))))
  (^Progress [^Progress progress ^long pos ^MessagePattern message]
   (with-message (jump progress pos) message)))

(defn advance
  (^Progress [^Progress progress]
   (advance progress 1))
  (^Progress [^Progress progress ^long delta]
   (jump progress (+ (.-pos progress) delta)))
  (^Progress [^Progress progress ^long delta ^MessagePattern message]
   (with-message (advance progress delta) message)))

(defn fraction [^Progress progress]
  (let [size (.-size progress)]
    (when (pos? size)
      (/ (.-pos progress) size))))

(defn percentage [^Progress progress]
  (when-some [fraction (fraction progress)]
    (int (* 100.0 (double fraction)))))

(definline message
  ^MessagePattern [^Progress progress]
  `(.-message ~(with-meta progress {:tag `Progress})))

(defn done? [^Progress progress]
  (let [size (.-size progress)]
    (and (pos? size)
         (= size (.-pos progress)))))

(definline cancellable? [^Progress progress]
  `(= :cancellable (.-cancel-state ~(with-meta progress {:tag `Progress}))))

(defn cancel
  ^Progress [^Progress progress]
  (assert (cancellable? progress))
  (assoc progress :cancel-state :cancelled))

(definline cancelled? [^Progress progress]
  `(= :cancelled (.-cancel-state ~(with-meta progress {:tag `Progress}))))

(defn- inherited-cancel-state [^Progress new-progress old-cancellable old-cancelled]
  (cond
    (or old-cancelled (cancelled? new-progress))
    :cancelled

    (or old-cancellable (cancellable? new-progress))
    :cancellable

    :else
    :not-cancellable))

(defn with-inherited-cancel-state
  ^Progress [^Progress new-progress ^Progress old-progress]
  (if (nil? old-progress)
    new-progress
    (let [old-cancellable (cancellable? old-progress)
          old-cancelled (cancelled? old-progress)
          inherited-cancel-state (inherited-cancel-state new-progress old-cancellable old-cancelled)]
      (assoc new-progress :cancel-state inherited-cancel-state))))

;; ----------------------------------------------------------

(defn relevant-change? [^Progress last-progress ^Progress progress]
  (or (nil? last-progress)
      (not= (.-cancel-state last-progress)
            (.-cancel-state progress))
      (not= (.-message last-progress)
            (.-message progress))
      (not= (percentage last-progress)
            (percentage progress))))

(defn null-render-progress! [_])

(defn throttle-render-progress [render-progress!]
  (let [last-progress (atom nil)]
    (fn throttled-render-progress! [^Progress progress]
      (when (relevant-change? @last-progress progress)
        (reset! last-progress progress)
        (render-progress! progress)))))

(defn until-done [render-progress!]
  (let [state-atom (atom :before) ;; :before => :done => :after
        track-progress (fn [state new-progress]
                         (case state
                           :before (if (= new-progress done) :done :before)
                           :done :after
                           :after :after))]
    (fn until-done-progress [^Progress progress]
      (when-not (= :after (swap! state-atom track-progress progress))
        (render-progress! progress)))))

(defn nest-render-progress
  "This creates a render-progress! function to be passed to a sub task.

  When creating a task using progress reporting you are responsible
  for splitting it into a number of steps. For instance one step per
  file processed. You then call render-progress! with a progress
  object containing a descriptive message, a size = number of steps,
  and a current pos = current step number.

  If you break out a sub task from a parent task, you need to decide
  how many steps the sub task represents. To the sub task, you pass a
  nested render progress function. This is created from the parents
  `render-progress!`, the parents current progress `parent-progress`,
  and a `span` - the number of parent steps steps the sub task
  represents. When returning from the sub task, the parent task should
  seem to have advanced `span` steps."
  ([render-progress! ^Progress parent-progress]
   (nest-render-progress render-progress! parent-progress 1))
  ([render-progress! ^Progress parent-progress ^long span]
   (if (= render-progress! null-render-progress!)
     null-render-progress!
     (let [parent-size (.-size parent-progress)
           parent-pos (.-pos parent-progress)
           parent-cancellable (cancellable? parent-progress)
           parent-cancelled (cancelled? parent-progress)]
       (assert (<= (+ parent-pos span) parent-size))
       (fn nested-render-progress! [^Progress progress]
         (let [size (.-size progress)
               pos (.-pos progress)]
           (render-progress!
             (->progress
               (message progress)
               (* size parent-size)
               (+ (* size parent-pos) (* pos span))
               (inherited-cancel-state progress parent-cancellable parent-cancelled)))))))))

(defn progress-mapv
  ([f coll render-progress!]
   (progress-mapv f coll render-progress! (constantly localization/empty-message)))
  ([f coll render-progress! message-fn]
   (persistent!
     (first
       (reduce (fn [[result progress] item]
                 (let [progress (with-message progress (or (message-fn item) localization/empty-message))]
                   (render-progress! progress)
                   (pair (conj! result (f item progress))
                         (advance progress))))
               (pair (transient [])
                     (->progress localization/empty-message (count coll) 0 :not-cancellable))
               coll)))))
