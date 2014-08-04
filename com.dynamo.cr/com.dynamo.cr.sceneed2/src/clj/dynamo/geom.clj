(ns dynamo.geom
  (:require [schema.macros :as sm]
            [dynamo.types :as dt :refer [rect]])
  (:import [dynamo.types Rect]
           [com.dynamo.bob.textureset TextureSetGenerator]))

(defn clamper [low high] (fn [x] (min (max x low) high)))

(sm/defn area :- double
  [r :- Rect]
  (if r
    (* (double (.width r)) (double (.height r)))
    0))

(sm/defn intersect :- Rect
  ([r :- Rect] r)
  ([r1 :- Rect r2 :- Rect]
    (when (and r1 r2)
      (let [l (max (.x r1) (.x r2))
            t (max (.y r1) (.y r2))
            r (min (+ (.x r1) (.width r1))  (+ (.x r2) (.width r2)))
            b (min (+ (.y r1) (.height r1)) (+ (.y r2) (.height r2)))
            w (- r l)
            h (- b t)]
        (if (and (< 0 w) (< 0 h))
          (rect l t w h)
          nil))))
  ([r1 :- Rect r2 :- Rect & rs :- [Rect]]
    (reduce intersect (intersect r1 r2) rs)))

(sm/defn split-rect :- [Rect]
  [container :- Rect content :- Rect]
  (let [new-rects (transient [])]
    (if-let [overlap (intersect container content)]
      (do
        ;; bottom slice
        (if (< (.y container) (.y overlap))
          (conj! new-rects (rect (.x container)
                                 (.y container)
                                 (.width container)
                                 (- (.y overlap) (.y container)))))

        ;; top slice
        (if (< (+ (.y overlap) (.height overlap)) (+ (.y container) (.height container)))
          (conj! new-rects (rect (.x container)
                                 (+ (.y overlap) (.height overlap))
                                 (.width container)
                                 (- (+ (.y container) (.height container))
                                    (+ (.y overlap)   (.height overlap))))))

	      ;; left slice
	      (if (< (.x container) (.x overlap))
	        (conj! new-rects (rect (.x container)
                                 (.y overlap)
                                 (- (.x overlap) (.x container))
                                 (.height overlap))))

	      ;; right slice
	      (if (< (+ (.x overlap) (.width overlap)) (+ (.x container) (.width container)))
	        (conj! new-rects (Rect. ""
	                                (+ (.x overlap) (.width overlap))
	                                (.y overlap)
	                                (- (+ (.x container) (.width container))
	                                   (+ (.x overlap)   (.width overlap)))
	                                (.height overlap)))))
      (conj! new-rects container))
    (persistent! new-rects)))

; This is off-by-one in many cases, due to Clojure's preference to promote things into Double and Long.
;
;(defn to-short-uv
;  "Return a fixed-point integer representation of the fractional part of the given fuv."
;  [^Float fuv]
;  (.shortValue
;    (bit-and
;      (int
;        (* (float fuv) (.floatValue 65535.0)))
;      0xffff)))

(defn to-short-uv
  [^Float fuv]
  (TextureSetGenerator/toShortUV fuv))
