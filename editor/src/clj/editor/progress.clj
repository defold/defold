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
  ([render-progress! parent]
   (nest-render-progress render-progress! parent 1))
  ([render-progress! parent span]
   {:pre [(<= (+ (:pos parent) span) (:size parent))]}
   (fn [progress]
     (let [scale (:size progress)]
       (render-progress!
         (make
           (message progress)
           (* scale (:size parent))
           (+ (* scale (:pos parent)) (* (:pos progress) span))))))))

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
