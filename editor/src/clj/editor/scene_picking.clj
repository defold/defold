(ns editor.scene-picking)

(defn picking-id->color [^long picking-id]
  (assert (<= picking-id 0xffffff))
  (let [picking-id (- 0xffffff (* 10 picking-id))
        red (bit-shift-right (bit-and picking-id 0xff0000) 16)
        green (bit-shift-right (bit-and picking-id 0xff00) 8)
        blue (bit-and picking-id 0xff)
        alpha 1.0]
    [(/ red 255.0) (/ green 255.0) (/ blue 255.0) alpha]))

(defn renderable-picking-id-uniform [renderable]
  (assert (some? (:picking-id renderable)))
  (let [picking-id (:picking-id renderable)
        id-color (picking-id->color picking-id)]
    (float-array id-color)))
