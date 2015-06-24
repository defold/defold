(ns internal.history)

(defprotocol Iterator
  (inext  [this] "Advance cursor to the next-most-recent item")
  (iprev  [this] "Step cursor backward to the previous-to-current item")
  (ivalue [this] "Return the current item")
  (before [this] "Return a seq of the items up and including the current value")
  (after  [this] "Return a seq of the items after the current value"))

(defprotocol Truncate
  (truncate [this] "Drop all values after the current one"))

(deftype PaperTape [limit limiter on-drop left right]
  clojure.lang.IPersistentCollection
  (seq [this]     (concat left right))
  (count [this]   (+ (count left) (count right)))
  (cons  [this o] (PaperTape. limit limiter on-drop (limiter (conj left o)) []))
  (empty [this]   (PaperTape. limit limiter on-drop [] []))
  (equiv [this o] (let [is-paper-tape? (and (instance? PaperTape o) o)
                        ^PaperTape that (when is-paper-tape? o)]
                    (when that
                      (= limit (.limit that))
                      (= left  (.left that))
                      (= right (.right that)))))

  Iterator
  ;; Move one from right to left
  (inext [this]
    (if-let [v (peek right)]
      (PaperTape. limit limiter on-drop (conj left v) (pop right))
      this))

  ;; Move one from left to right
  (iprev [this]
    (if-let [v (peek left)]
      (PaperTape. limit limiter on-drop (pop left) (conj right v))
      this))

  (ivalue [this]  (peek left))
  (before [this]  left)
  (after  [this]  right)

  Truncate
  (truncate [this]
    (PaperTape. limit limiter on-drop left [])))

(defn- make-limiter
  [limit]
  (if-not limit
    identity
    (fn [v]
      (if (> (count v) limit)
        (subvec v (- (count v) limit))
        v))))

(defn paper-tape
  ([limit]
   (paper-tape limit (fn [v])))
  ([limit on-drop]
   (PaperTape. limit (make-limiter limit) on-drop [] [])))
