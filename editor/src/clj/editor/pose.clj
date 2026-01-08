;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.pose
  (:require [clojure.spec.alpha :as s]
            [editor.math :as math])
  (:import [javax.vecmath Matrix4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(s/def ::num number?)
(s/def ::num3 (s/tuple ::num ::num ::num))
(s/def ::num4 (s/tuple ::num ::num ::num ::num))
(s/def ::translation ::num3)
(s/def ::rotation ::num4)
(s/def ::scale ::num3)
(s/def ::euler-rotation ::num3)

(defrecord Pose [translation rotation scale])

(defn pose? [value]
  (instance? Pose value))

(def default-translation (vector-of :double 0.0 0.0 0.0))
(def default-rotation (vector-of :double 0.0 0.0 0.0 1.0))
(def default-scale (vector-of :double 1.0 1.0 1.0))
(def ^Pose default (->Pose default-translation default-rotation default-scale))

(def ^:private mat4-identity (doto (Matrix4d.) (.setIdentity)))
(def ^:private num4-zero (vector-of :double 0.0 0.0 0.0 0.0))

(defn- add-vector [[^double ax ^double ay ^double az] [^double bx ^double by ^double bz]]
  (vector-of
    :double
    (+ ax bx)
    (+ ay by)
    (+ az bz)))

(defn- multiply-vector [[^double ax ^double ay ^double az] [^double bx ^double by ^double bz]]
  (vector-of
    :double
    (* ax bx)
    (* ay by)
    (* az bz)))

(defn- rotate-vector [[^double vx ^double vy ^double vz] [^double qx ^double qy ^double qz ^double qw]]
  (vector-of
    :double
    (- (+ (* vx qw qw)
          (* vy qz qw -2.0)
          (* vz qy qw 2.0)
          (* vx qx qx)
          (* vy qx qy 2.0)
          (* vz qx qz 2.0))
       (* vx qy qy)
       (* vx qz qz))
    (- (+ (* vx qx qy 2.0)
          (* vy qy qy)
          (* vz qy qz 2.0)
          (* vx qz qw 2.0)
          (* vy qw qw)
          (* vz qx qw -2.0))
       (* vy qx qx)
       (* vy qz qz))
    (- (+ (* vx qx qz 2.0)
          (* vy qy qz 2.0)
          (* vz qz qz)
          (* vx qy qw -2.0)
          (* vy qx qw 2.0)
          (* vz qw qw))
       (* vz qx qx)
       (* vz qy qy))))

(defn- multiply-quaternion [[^double ax ^double ay ^double az ^double aw] [^double bx ^double by ^double bz ^double bw]]
  (vector-of
    :double
    (- (+ (* ax bw)
          (* ay bz)
          (* aw bx))
       (* az by))
    (- (+ (* ay bw)
          (* az bx)
          (* aw by))
       (* ax bz))
    (- (+ (* ax by)
          (* az bw)
          (* aw bz))
       (* ay bx))
    (- (* aw bw)
       (* ax bx)
       (* ay by)
       (* az bz))))

(defn euler-rotation [^double x-deg ^double y-deg ^double z-deg]
  ;; Implementation based on:
  ;; http://ntrs.nasa.gov/archive/nasa/casi.ntrs.nasa.gov/19770024290.pdf
  ;; Rotation sequence: 231 (YZX)
  (if (= 0.0 x-deg y-deg)
    (if (= 0.0 z-deg)
      default-rotation
      (let [ha (* 0.5 (math/deg->rad z-deg))
            s (Math/sin ha)
            c (Math/cos ha)]
        (vector-of :double 0.0 0.0 s c)))
    (let [t1 (math/deg->rad y-deg)
          t2 (math/deg->rad z-deg)
          t3 (math/deg->rad x-deg)
          c1 (Math/cos (* t1 0.5))
          s1 (Math/sin (* t1 0.5))
          c2 (Math/cos (* t2 0.5))
          s2 (Math/sin (* t2 0.5))
          c3 (Math/cos (* t3 0.5))
          s3 (Math/sin (* t3 0.5))
          c1c2 (* c1 c2)
          s2s3 (* s2 s3)
          qx (+ (* s1 s2 c3)
                (* s3 c1c2))
          qy (+ (* s1 c2 c3)
                (* s2s3 c1))
          qz (+ (- (* s1 s3 c2))
                (* s2 c1 c3))
          qw (+ (- (* s1 s2s3))
                (* c1c2 c3))
          dp (+ (* qx qx) (* qy qy) (* qz qz) (* qw qw))]
      (if (> dp 0.0)
        (let [r (/ 1.0 (Math/sqrt dp))]
          (vector-of :double (* qx r) (* qy r) (* qz r) (* qw r)))
        num4-zero))))

(defn- seq->double-vec3 [coll default-double-vec]
  (if (nil? coll)
    default-double-vec
    (let [[x y z] coll]
      (cond
        (and (= (double x) ^double (default-double-vec 0))
             (= (double y) ^double (default-double-vec 1))
             (= (double z) ^double (default-double-vec 2)))
        default-double-vec

        (and (vector? coll)
             (= 3 (count coll))
             (double? x)
             (double? y)
             (double? z))
        coll

        :else
        (vector-of :double x y z)))))

(defn- seq->double-vec4 [coll default-double-vec]
  (if (nil? coll)
    default-double-vec
    (let [[x y z w] coll]
      (cond
        (and (= (double x) ^double (default-double-vec 0))
             (= (double y) ^double (default-double-vec 1))
             (= (double z) ^double (default-double-vec 2))
             (= (double w) ^double (default-double-vec 3)))
        default-double-vec

        (and (vector? coll)
             (= 4 (count coll))
             (double? x)
             (double? y)
             (double? z)
             (double? w))
        coll

        :else
        (vector-of :double x y z w)))))

(defn seq-translation [component-seq]
  (seq->double-vec3 component-seq default-translation))

(defn seq-rotation [component-seq]
  (seq->double-vec4 component-seq default-rotation))

(defn seq-euler-rotation [euler-seq]
  (if (nil? euler-seq)
    default-rotation
    (let [[^double x-deg ^double y-deg ^double z-deg] euler-seq]
      (euler-rotation x-deg y-deg z-deg))))

(defn seq-scale [component-seq]
  (seq->double-vec3 component-seq default-scale))

(defn translation-pose
  ^Pose [^double x ^double y ^double z]
  (if (and (zero? x) (zero? y) (zero? z))
    default
    (->Pose (vector-of :double x y z) default-rotation default-scale)))

(defn rotation-pose
  ^Pose [^double x ^double y ^double z ^double w]
  (if (and (zero? x) (zero? y) (zero? z) (= 1.0 w))
    default
    (->Pose default-translation (vector-of :double x y z w) default-scale)))

(defn euler-rotation-pose
  ^Pose [^double x-deg ^double y-deg ^double z-deg]
  (if (and (zero? x-deg) (zero? y-deg) (zero? z-deg))
    default
    (->Pose default-translation (euler-rotation x-deg y-deg z-deg) default-scale)))

(defn scale-pose
  ^Pose [^double x ^double y ^double z]
  (if (and (= 1.0 x) (= 1.0 y) (= 1.0 z))
    default
    (->Pose default-translation default-rotation (vector-of :double x y z))))

(defn make
  ^Pose [translation rotation scale]
  {:pre [(or (nil? translation) (s/valid? ::translation translation))
         (or (nil? rotation) (s/valid? ::rotation rotation))
         (or (nil? scale) (s/valid? ::scale scale))]}
  (let [translated (and translation (not= default-translation translation))
        rotated (and rotation (not= default-rotation rotation))
        scaled (and scale (not= default-scale scale))]
    (if (or translated rotated scaled)
      (->Pose (if translated translation default-translation)
              (if rotated rotation default-rotation)
              (if scaled scale default-scale))
      default)))

(defn from-translation
  ^Pose [translation]
  {:pre [(or (nil? translation) (s/valid? ::translation translation))]}
  (if translation
    (apply translation-pose translation)
    default))

(defn from-rotation
  ^Pose [rotation]
  {:pre [(or (nil? rotation) (s/valid? ::rotation rotation))]}
  (if rotation
    (apply rotation-pose rotation)
    default))

(defn from-euler-rotation
  ^Pose [euler-rotation]
  {:pre [(or (nil? euler-rotation) (s/valid? ::euler-rotation euler-rotation))]}
  (if euler-rotation
    (apply euler-rotation-pose euler-rotation)
    default))

(defn from-scale
  ^Pose [scale]
  {:pre [(or (nil? scale) (s/valid? ::scale scale))]}
  (if scale
    (apply scale-pose scale)
    default))

(defn pre-multiply
  ^Pose [^Pose child-pose ^Pose parent-pose]
  {:pre [(pose? child-pose)
         (pose? parent-pose)]}
  (let [child-translation (.-translation child-pose)
        child-rotation (.-rotation child-pose)
        child-scale (.-scale child-pose)
        parent-translation (.-translation parent-pose)
        parent-rotation (.-rotation parent-pose)
        parent-scale (.-scale parent-pose)
        parent-translated (not= default-translation parent-translation)
        parent-rotated (not= default-rotation parent-rotation)
        parent-scaled (not= default-scale parent-scale)]
    (cond-> child-pose

            parent-scaled
            (assoc :scale (multiply-vector parent-scale child-scale))

            parent-rotated
            (assoc :rotation (multiply-quaternion parent-rotation child-rotation))

            (or parent-scaled parent-rotated parent-translated)
            (assoc :translation
                   (cond-> child-translation
                           parent-scaled (multiply-vector parent-scale)
                           parent-rotated (rotate-vector parent-rotation)
                           parent-translated (add-vector parent-translation))))))

(defn matrix
  ^Matrix4d [^Pose pose]
  {:pre [(pose? pose)]}
  (if (identical? default pose)
    mat4-identity
    (math/clj->mat4 (.-translation pose) (.-rotation pose) (.-scale pose))))

(defmacro translation-v3 [pose-expr]
  `(.-translation ~(with-meta pose-expr {:tag `Pose})))

(defmacro translation-v4 [pose-expr w-expr]
  `(conj (translation-v3 ~pose-expr) (float ~w-expr)))

(defmacro rotation-q4 [pose-expr]
  `(.-rotation ~(with-meta pose-expr {:tag `Pose})))

(defmacro euler-rotation-v3 [pose-expr]
  `(math/clj-quat->euler (rotation-q4 ~pose-expr)))

(defmacro euler-rotation-v4 [pose-expr]
  `(conj (math/clj-quat->euler (rotation-q4 ~pose-expr)) 0.0))

(defmacro scale-v3 [pose-expr]
  `(.-scale ~(with-meta pose-expr {:tag `Pose})))

(defmacro scale-v4 [pose-expr]
  `(conj (scale-v3 ~pose-expr) 1.0))

(defmacro translated? [pose-expr]
  `(not= default-translation (translation-v3 ~pose-expr)))

(defmacro rotated? [pose-expr]
  `(not= default-rotation (rotation-q4 ~pose-expr)))

(defmacro scaled? [pose-expr]
  `(not= default-scale (scale-v3 ~pose-expr)))
