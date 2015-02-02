(ns internal.texture.pack-max-rects
  (:require [schema.core :as s]
            [schema.macros :as sm]
            [dynamo.geom :refer :all]
            [dynamo.image :refer :all]
            [dynamo.types :as t :refer [rect width height]]
            [internal.texture.math :refer :all])
  (:import [dynamo.types Rect Image TexturePacking]))

(set! *warn-on-reflection* true)

(defn- short-side-fit
  [^Rect r1 ^Rect r2]
  (min (Math/abs ^int (- (width r1)  (width r2)))
       (Math/abs ^int (- (height r1) (height r2)))))

(sm/defn ^:private area-fit :- s/Int
  [r1 :- Rect r2 :- Rect]
  (- (area r1) (area r2)))

(sm/defn ^:private score-rect :- [(s/one s/Int "score") (s/one Rect "free-rect")]
  [free-rect :- Rect rect :- Rect]
  (when (and  (>= (:width free-rect) (:width rect))
              (>= (:height free-rect) (:height rect)))
    [(area-fit free-rect rect) free-rect]))

; find the best free-rect into which to place rect
(sm/defn score-rects :- [[(s/one s/Int "score") (s/one Rect "free-rect")]]
  [free-rects :- [Rect] {:keys [width height] :as rect} :- Rect]
  "Sort the free-rects according to how well rect fits into them.
   A 'good fit' means the least amount of leftover area."
  (reverse (remove nil? (sort-by first (map score-rect free-rects (repeat rect))))))

(sm/defn place-in :- Rect
  [container :- Rect content :- Rect]
  (assoc content
         :x (.x container)
         :y (.y container)))

(def max-width 2048)

(sm/defn ^:private with-top-right-margin :- Rect
  [margin :- s/Int r :- Rect]
  (assoc r
         :width (+ margin (:width r))
         :height (+ margin (:height r))))

(sm/defn pack-at-size :- TexturePacking
  [margin :- s/Int sources :- [Rect] space-available :- Rect]
  (loop [free-rects   [space-available]
         remaining    (reverse (sort-by area sources))
         placed       []]
    (if-let [unplaced (first remaining)]
      (let [scored-rects   (score-rects free-rects (with-top-right-margin margin unplaced))
            best-fit       (second (first scored-rects))
            remaining-free (map second (rest scored-rects))]
        (if (nil? best-fit)
          :packing-failed
          (let [newly-placed (place-in best-fit unplaced)]
            (recur (reverse (sort-by area (concat remaining-free (split-rect best-fit (with-top-right-margin margin newly-placed)))))
                   (rest remaining)
                   (conj placed newly-placed)))))
      (t/->TexturePacking space-available nil placed sources []))))

(sm/defn ^:private stepwise-doubling-sizes :- [Rect]
  [start :- s/Int]
  (for [[w h] (partition 2 (rest (mapcat #(repeat 4 %) (doubling (max 1 start)))))]
    (rect 0 0 w h)))

(sm/defn plausible-sizes :- [Rect]
  [sources :- [Rect]]
  (let [initial-power-of-two 64
        total-area           (reduce + (map area sources))
        sq                   (Math/sqrt total-area)]
    (if (> initial-power-of-two sq)
      (stepwise-doubling-sizes initial-power-of-two)
      (let [split                (partition-by #(> sq %) (doubling initial-power-of-two))
            short-side           (last (first split))
            short-side           (max short-side initial-power-of-two)]
        (stepwise-doubling-sizes short-side)))))

(sm/defn max-rects-packing :- TexturePacking
  ([sources :- [Rect]]
    (max-rects-packing 0 sources))
  ([margin :- s/Int sources :- [Rect]]
    (case (count sources)
      0   :packing-failed
      1   (t/->TexturePacking (first sources) nil sources sources [])
      (->> sources
        plausible-sizes
        (map #(pack-at-size margin sources %))
        (filter #(not= :packing-failed %))
        first))))
