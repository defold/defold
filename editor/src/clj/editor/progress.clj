(ns editor.progress
  (:refer-clojure :exclude [mapv]))

(defn make
  ([msg]
   (make msg 1))
  ([msg size]
   (make msg size 0))
  ([msg size pos]
   {:pre [(<= pos size)]}
   {:message msg :size size :pos pos}))

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

(defn fraction [progress]
  (/ (:pos progress) (:size progress)))

(defn percentage [progress]
  (int (* 100 (fraction progress))))

(defn message [progress]
  (:message progress))

(defn done? [progress]
  (= (:pos progress)
     (:size progress)))

;; ----------------------------------------------------------

(defn- relevant-change? [last-progress progress]
  (or (nil? last-progress)
      (not= (message last-progress)
            (message progress))
      (not= (percentage last-progress)
            (percentage progress))))

(defn null-render-progress! [_])

(defn println-render-progress! [progress]
  (println (message progress) (percentage progress) "%"))

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

(comment
  ;; silly nest-render-progress usage example

  (defn morning-sub-task! [render-progress!]
    (render-progress! (make "morning: make covfefe" 2 0))
    (render-progress! (make "morning: have coffee" 2 1))
    (render-progress! (make "morning: be happy" 2 2)))

  (defn go-to-work-sub-task! [render-progress!]
    (render-progress! (make "go-to-work: walk to subway" 4 0))
    (render-progress! (make "go-to-work: wait for subway" 4 1))
    (render-progress! (make "go-to-work: go by subway" 4 2))
    (render-progress! (make "go-to-work: walk to work" 4 3))
    (render-progress! (make "go-to-work: arrive at work" 4 4)))

  (defn feed-kids! [render-progress!]
    (loop [progress (make "feed-kids: feed" 20 0)]
      (render-progress! progress)
      (when-not (done? progress)
        (recur (advance progress)))))

  (defn evening-sub-task! [render-progress!]
    (render-progress! (make "evening: make food" 5 0))
    (feed-kids! (nest-render-progress render-progress! (make "" 5 1) 2))
    (render-progress! (make "evening: eat" 5 3))
    (render-progress! (make "evening: telly" 5 4))
    (render-progress! (make "evening: sleep" 5 5)))

  (defn parent-task! [render-progress!]
    (render-progress! (make "parent: wake up" 5 0))
    (morning-sub-task! (nest-render-progress render-progress! (make "" 5 0)))
    (render-progress! (make "parent: morning over" 5 1))
    (go-to-work-sub-task! (nest-render-progress render-progress! (make "" 5 1)))
    (render-progress! (make "parent: workify" 5 2))
    (render-progress! (make "parent: go home" 5 3))
    (evening-sub-task! (nest-render-progress render-progress! (make "" 5 4)))
    (render-progress! (make "parent: not to bad" 5 5)))

  (parent-task! println-render-progress!)

  )

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
