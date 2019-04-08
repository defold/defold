(ns editor.progress
  (:refer-clojure :exclude [mapv]))

(defn make
  ([msg]
   (make msg 1))
  ([msg size]
   (make msg size 0))
  ([msg size pos]
   (make msg size pos false))
  ([msg size pos cancellable?]
   {:pre [(<= pos size)]}
   {:message msg :size size :pos pos :cancellable cancellable? :cancelled false}))

(defn make-indeterminate [msg]
  (make msg 0))

(defn make-cancellable-indeterminate [msg]
  (make msg 0 0 true))

(def done (make "Ready" 1 1))

(defn with-message [p msg]
  (assoc p :message msg))

(defn jump
  ([p pos]
   (assoc p :pos (min (:size p) pos)))
  ([p pos msg]
   (with-message (jump p pos) msg)))

(defn advance
  ([p]
   (advance p 1))
  ([p n]
   (jump p (+ (:pos p) n)))
  ([p n msg]
   (with-message (advance p n) msg)))

(defn fraction [{:keys [pos size] :as _progress}]
  (when (pos? size)
    (/ pos size)))

(defn percentage [progress]
  (when-some [fraction (fraction progress)]
    (int (* 100 fraction))))

(defn message [progress]
  (:message progress))

(defn done? [{:keys [pos size] :as _progress}]
  (and (pos? size)
       (= pos size)))

(defn cancellable? [progress]
  (:cancellable progress))

(defn cancel! [progress]
  (assoc progress :cancelled true))

(defn cancelled? [progress]
  (:cancelled progress))

;; ----------------------------------------------------------

(defn- relevant-change? [last-progress progress]
  (or (nil? last-progress)
      (not= (message last-progress)
            (message progress))
      (not= (percentage last-progress)
            (percentage progress))))

(defn null-render-progress! [_])

(defn println-render-progress! [progress]
  (if-some [percentage (percentage progress)]
    (println (message progress) percentage "%")
    (println (message progress))))

(defn throttle-render-progress [render-progress!]
  (let [last-progress (atom nil)]
    (fn [progress]
      (when (relevant-change? @last-progress progress)
        (swap! last-progress progress)
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
  ([render-progress! parent-progress]
   (nest-render-progress render-progress! parent-progress 1))
  ([render-progress! parent-progress span]
   {:pre [(<= (+ (:pos parent-progress) span) (:size parent-progress))]}
   (if (= render-progress! null-render-progress!)
     null-render-progress!
     (fn [progress]
       (let [scale (:size progress)]
         (render-progress!
           (make
             (message progress)
             (* scale (:size parent-progress))
             (+ (* scale (:pos parent-progress)) (* (:pos progress) span)))))))))

(defn progress-mapv
  ([f coll render-progress!]
   (progress-mapv f coll render-progress! (constantly "")))
  ([f coll render-progress! message-fn]
   (let [progress (make "" (count coll))]
     (first
      (reduce (fn [[result progress] e]
                (let [progress (with-message progress (or (message-fn e) ""))]
                  (render-progress! progress)
                  [(conj result (f e progress)) (advance progress)]))
              [[] progress]
              coll)))))

(defn mapv
  "Similar to progress-mapv, but is mapv compatible and will call f with same
  args as regular mapv."
  ([f coll render-progress!]
   (progress-mapv f coll render-progress! (constantly "")))
  ([f coll render-progress! message-fn]
   (let [progress (make "" (count coll))]
     (first
      (reduce (fn [[result progress] e]
                (let [progress (with-message progress (or (message-fn e) ""))]
                  (render-progress! progress)
                  [(conj result (f e)) (advance progress)]))
              [[] progress]
              coll)))))

(defn make-mapv
  [render-progress! message-fn]
  (fn [f coll] (mapv f coll render-progress! message-fn)))
