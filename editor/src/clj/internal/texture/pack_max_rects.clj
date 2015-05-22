(ns internal.texture.pack-max-rects
  (:require [dynamo.geom :refer :all]
            [dynamo.image :refer :all]
            [dynamo.types :as t :refer [rect width height]]
            [internal.texture.math :refer :all]
            [schema.core :as s])
  (:import [dynamo.types Rect Image TexturePacking]))

; ---------------------------------------------------------------------------
; Configuration parameters
; ---------------------------------------------------------------------------
(def texture-min-width    64)
(def texture-max-width  4096)
(def texture-min-height   64)
(def texture-max-height 4096)

; ---------------------------------------------------------------------------
; Packing algorithm
; ---------------------------------------------------------------------------
(defn- short-side-fit
  [^Rect r1 ^Rect r2]
  (min (Math/abs ^int (- (width r1)  (width r2)))
       (Math/abs ^int (- (height r1) (height r2)))))

(s/defn ^:private area-fit :- t/Int
  [r1 :- Rect r2 :- Rect]
  (let [a1 (area r1)
        a2 (area r2)]
    (when (> a1 a2)
      (/ a2 a1))))

(s/defn score-rect :- [(t/one t/Int "score") (t/one Rect "free-rect")]
  [free-rect :- Rect rect :- Rect]
  (if (and  (>= (:width free-rect) (:width rect))
            (>= (:height free-rect) (:height rect)))
    [(area-fit free-rect rect) free-rect]
    [nil free-rect]))

; find the best free-rect into which to place rect
(s/defn score-rects :- [[(t/one t/Int "score") (t/one Rect "free-rect")]]
  [free-rects :- [Rect] {:keys [width height] :as rect} :- Rect]
  "Sort the free-rects according to how well rect fits into them.
   A 'good fit' means the least amount of leftover area."
  (reverse (sort-by first (map score-rect free-rects (repeat rect)))))

(s/defn place-in :- Rect
  [container :- Rect content :- Rect]
  (assoc content
         :x (.x container)
         :y (.y container)))

(def max-width 2048)

(s/defn with-top-right-margin :- Rect
  [margin :- t/Int r :- Rect]
  (assoc r
         :width (+ margin (:width r))
         :height (+ margin (:height r))))

(def ^:dynamic *debug-packing* true)
(defmacro debug [& forms] (when *debug-packing* `(do ~@forms)))
(def trace (atom []))

(s/defn pack-at-size :- TexturePacking
  [margin :- t/Int sources :- [Rect] space-available :- Rect]
  (loop [free-rects   [space-available]
         remaining    (reverse (sort-by area sources))
         placed       []]
    (debug
      (swap! trace conj {:free-rects free-rects :remaining remaining :placed placed}))
    (if-let [unplaced (first remaining)]
      (let [scored-rects          (score-rects free-rects (with-top-right-margin margin unplaced))
            [best-score best-fit] (first scored-rects)
            remaining-free        (map second (rest scored-rects))]
        (if (nil? best-score)
          nil
          (let [newly-placed (place-in best-fit unplaced)]
            (recur (reverse (sort-by area (concat remaining-free (split-rect best-fit (with-top-right-margin margin newly-placed)))))
                   (rest remaining)
                   (conj placed newly-placed)))))
      (t/->TexturePacking space-available nil placed sources []))))

; ---------------------------------------------------------------------------
; Binary search of sizes
; ---------------------------------------------------------------------------
(defn- occupancy
  [{:keys [aabb sources]}]
  (if-not (nil? aabb)
    (/ (reduce + (map area sources)) (area aabb))
    0))

(defn- keep-best
  [p1 p2]
  (cond
    (nil? p1)                         p2
    (nil? p2)                         p1
    (> (occupancy p1) (occupancy p2)) p1
    :else                             p2))

(defn- pack-width-search
  [height min-width max-width margin sources]
  (loop [width-search  (binary-search-start min-width texture-max-width)
         best-so-far   nil]
    (if-let [current-width (binary-search-current width-search)]
      (let [packing (pack-at-size margin sources (Rect. "" 0 0 current-width height))]
        (recur
          (binary-search-next width-search (nil? packing))
          (keep-best best-so-far packing)))
      best-so-far)))

(defn- pack-height-search
  [min-height max-height min-width max-width margin sources]
  (loop [height-search (binary-search-start min-height max-height)
         best-so-far   nil]
    (if-let [current-height (binary-search-current height-search)]
      (let [packing-at-height (pack-width-search current-height min-width max-width margin sources)]
        (recur
          (binary-search-next height-search (nil? packing-at-height))
          (keep-best best-so-far packing-at-height)))
      best-so-far)))

(defn pack-sources
  [margin sources]
  (debug
    (reset! trace []))
  (let [min-width     (max texture-min-width (reduce min (map :width sources)))
        min-height    (max texture-min-height (reduce min (map :height sources)))]
    (pack-height-search min-height texture-max-height min-width texture-max-width margin sources)))

; ---------------------------------------------------------------------------
; External API
; ---------------------------------------------------------------------------
(s/defn max-rects-packing :- TexturePacking
  ([sources :- [Rect]]
    (max-rects-packing 0 sources))
  ([margin :- t/Int sources :- [Rect]]
    (case (count sources)
      0   :packing-failed
      1   (t/->TexturePacking (first sources) nil sources sources [])
      (pack-sources margin sources))))
