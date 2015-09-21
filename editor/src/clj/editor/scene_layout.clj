(ns editor.scene-layout
  (:require [editor.types :refer [->Region]])
  (:import [editor.types Region]))

(defn- width [^Region r]
  (- (:right r) (:left r)))

(defn- height [^Region r]
  (- (:bottom r) (:top r)))

(defn- dims [desc dim]
  (let [[width height] dim
        padding (:padding desc 0)
        width (:max-width desc width)
        height (:max-height desc height)]
    {:width width
     :height height
     :children (loop [width (- width (* padding 2))
                      children (:children desc)
                      child-count (count (:children desc))
                      result []]
                 (if-let [child (first children)]
                   (let [child-width (min (/ width child-count) (:max-width child width))
                         result (conj result (dims child [child-width (- height (* padding 2))]))]
                     (recur (- width child-width) (rest children) (dec child-count) result))
                   result))}))

(defn- ->region [offset dims]
  (let [[x y] offset
        width (:width dims)
        height (:height dims)]
    (->Region x (+ x width) y (+ y height))))

(defn- bound [anchor value op default]
  (or (and anchor (op value anchor))
      default))

(defn- constrain [anchors dims offset parent-region]
  (let [min-x (bound (:right anchors) (- (:right parent-region) (:width dims)) - (:left parent-region))
        min-y (bound (:bottom anchors) (- (:bottom parent-region) (:height dims)) - (:top parent-region))
        max-x (bound (:left anchors) (:left parent-region) + (:right parent-region))
        max-y (bound (:top anchors) (:top parent-region) + (:bottom parent-region))
        [x y] offset]
    [(min max-x (max min-x x))
     (min max-y (max min-y y))]))

(defn- shrink [^Region region amount]
  (apply ->Region (mapv (fn [[key op]] (op (get region key) amount)) [[:left +] [:right -] [:top +] [:bottom -]])))

(defn- place [desc dims offset parent-region]
  (let [offset (constrain (:anchors desc) dims offset parent-region)
        region (->region offset dims)
        sub-region (shrink region (:padding desc 0))]
    (into [[(:id desc) region]]
         (loop [children (:children desc)
                dims (:children dims)
                offset [(:left sub-region) (:top sub-region)]
                result []]
           (if-let [child (first children)]
             (let [dim (first dims)
                   width (:width dim)
                   height (:height dim)
                   [x y] offset
                   next-offset [(+ x width) y]]
               (recur (rest children) (rest dims) next-offset (into result (place child dim offset sub-region))))
             result)))))

(defn layout [desc ^Region viewport]
  (let [dims (dims desc [(width viewport) (height viewport)])
        padding (:padding desc 0)]
    (into {} (filter (comp some? first) (place desc dims [0 0] viewport)))))
