(ns editor.progress
  (:refer-clojure :exclude [mapv]))

;; Progress can be represented as a single progress map (produced by make)
;; or a vector of progress maps (when a job has subtasks).

(defn make
  ([msg]
   (make msg 1))
  ([msg size]
   (make msg size 0))
  ([msg size pos]
   {:pre [(<= pos size)]}
   {:message msg :size size :pos pos}))

(def done (make "Done" 1 1))

(defn message [p msg]
  (assoc p :message msg))

(defn advance
  ([p]
   (advance p 1))
  ([p n]
   (update p :pos #(min (:size p) (+ % n))))
  ([p n msg]
   (message (advance p n) msg)))

(defn- ensure-vector [d]
  (and d (if (vector? d) d [d])))

;; If you have a progress with subjobs, the subjobs' contribution to the overall percentage is
;; weighted. If the subjob has subjobs, they are also weigthed relative to their parent.
;; Please note that for the percentages with subjobs to add up, the correct :size of the parent
;; jobs needs to be set.

(defn percentage [progress]
  (let [res (first
             (reduce (fn [[acc divisor] p]
                       (let [percent (/ (:pos p) (:size p))]
                         [(+ acc (/ percent divisor))
                          (* divisor (:size p))]))
                     [0 1]
                     (reverse (ensure-vector progress))))]
    (if (> res 1) 1 res)))

(defn description [progress]
  (->> (ensure-vector progress)
       (map :message)
       (remove nil?)
       (interpose "\n")
       (apply str)))

;; ----------------------------------------------------------

(defn null-render-progress! [_])

(defn nest-render-progress [render-progress! next]
  (fn [progress] (render-progress!
                  (conj (ensure-vector progress) next))))

(defn progress-mapv
  ([f coll render-progress!]
   (progress-mapv f coll render-progress! (constantly "")))
  ([f coll render-progress! message-fn]
   (let [progress (make "" (count coll))]
     (first
      (reduce (fn [[result progress] e]
                (let [progress (message progress (or (message-fn e) ""))]
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
                (let [progress (message progress (or (message-fn e) ""))]
                  (render-progress! progress)
                  [(conj result (f e)) (advance progress)]))
              [[] progress]
              coll)))))

(defn make-mapv
  [render-progress! message-fn]
  (fn [f coll] (mapv f coll render-progress! message-fn)))
