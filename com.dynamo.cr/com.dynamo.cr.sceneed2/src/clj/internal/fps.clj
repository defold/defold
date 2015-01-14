(ns internal.fps
  (:require [dynamo.geom :as g]
            [dynamo.node :as n]
            [dynamo.types :as t]
            [plumbing.core :refer [defnk]]
            [dynamo.gl :as gl]
            [internal.render.pass :as pass]))

(set! *warn-on-reflection* true)

(deftype FpsTracker [sum ^longs ringbuf len idx lasttick]
  clojure.lang.IDeref
  (deref [this] (* 1000000000.0 (/ len @sum)))

  clojure.lang.IFn
  (invoke [this]
    (let [ticktime      (System/nanoTime)
          delta         (- ticktime @lasttick)
          frame-to-drop (aget ringbuf @idx)
          newsum        (swap! sum #(+ % delta (- frame-to-drop)))]
      (aset ringbuf @idx ^long delta)
      (reset! lasttick ticktime)
      (swap! idx #(let [n (inc %)]
                    (if (>= n len) 0 n)))
      (* 1000000000.0 (/ len newsum)))))

(defn make-fps-tracker [frames]
  (FpsTracker. (atom 0) (long-array frames) frames (atom 0) (atom (System/nanoTime))))

(defn render-fps
  [ctx gl text-renderer ^FpsTracker tracker]
  (let [fps (tracker)]
    (gl/overlay ctx gl text-renderer (format "FPS: %.2f" fps) 12.0 -44.0 1.0 1.0)))

(defnk render-overlay :- t/RenderData
  [this project]
  {pass/overlay
   [{:world-transform g/Identity4d
     :render-fn
     (fn [ctx gl glu text-renderer]
       (render-fps ctx gl text-renderer (:tracker this)))}]})

(n/defnode FpsTrackerNode
  (property tracker FpsTracker)

  (output renderable t/RenderData render-overlay))

(defn new-fps-tracker []
  (n/construct FpsTrackerNode :tracker (make-fps-tracker 300)))