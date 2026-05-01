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

(ns editor.editor-extensions.tile-map
  (:require [clojure.data.int-map :as int-map]
            [editor.editor-extensions.runtime :as rt]
            [editor.tile-map-common :as tile-map-common]
            [util.defonce :as defonce])
  (:import [clojure.lang IFn]
           [com.defold.editor.luart DefoldFourArgsLuaFn DefoldOneArgLuaFn DefoldThreeArgsLuaFn DefoldVarargsLuaFn DefoldZeroArgsLuaFn]
           [editor.tile_map_common Tile]
           [org.luaj.vm2 LuaNumber LuaUserdata LuaValue]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/type Tiles [^:unsynchronized-mutable cell-map]
  IFn
  (invoke [_] cell-map)
  (invoke [_ f] (set! cell-map (f cell-map)))
  (invoke [_ f a] (set! cell-map (f cell-map a)))
  (invoke [_ f a b] (set! cell-map (f cell-map a b))))

;; region create

(defn make
  ([]
   (make (int-map/int-map)))
  ([cell-map]
   (rt/wrap-userdata (->Tiles cell-map) "tiles")))

;; endregion

;; region reads

(defn- ext-get-tile [^LuaValue lua-tiles ^LuaValue lua-x ^LuaValue lua-y]
  (let [tiles (.checkuserdata lua-tiles)
        ;; 1-indexed (lua) -> 0-indexed (editor)
        x (unchecked-dec (.checklong lua-x))
        y (unchecked-dec (.checklong lua-y))]
    (if-let [^Tile tile ((tiles) (tile-map-common/cell-index x y))]
      ;; 0-indexed (editor) -> 1-indexed (lua)
      (LuaValue/valueOf (unchecked-inc-int (int (.-tile tile))))
      LuaValue/NIL)))

(def ^:private ^LuaValue index-lua-str (LuaValue/valueOf "index"))
(def ^:private ^LuaValue h-flip-lua-str (LuaValue/valueOf "h_flip"))
(def ^:private ^LuaValue v-flip-lua-str (LuaValue/valueOf "v_flip"))
(def ^:private ^LuaValue rotate-90-lua-str (LuaValue/valueOf "rotate_90"))

(defn- ext-get-info [^LuaValue lua-tiles ^LuaValue lua-x ^LuaValue lua-y]
  (let [tiles (.checkuserdata lua-tiles)
        ;; 1-indexed (lua) -> 0-indexed (editor)
        x (unchecked-dec (.checklong lua-x))
        y (unchecked-dec (.checklong lua-y))]
    (if-let [^Tile tile ((tiles) (tile-map-common/cell-index x y))]
      (doto (LuaValue/tableOf 0 4)
        ;; 0-indexed (editor) -> 1-indexed (lua)
        (.rawset index-lua-str (LuaValue/valueOf (unchecked-inc-int (int (.-tile tile)))))
        (.rawset h-flip-lua-str (LuaValue/valueOf (boolean (.-h-flip tile))))
        (.rawset v-flip-lua-str (LuaValue/valueOf (boolean (.-v-flip tile))))
        (.rawset rotate-90-lua-str (LuaValue/valueOf (boolean (.-rotate90 tile)))))
      LuaValue/NIL)))

(defn- ext-iterator [^LuaUserdata lua-tiles]
  (let [tiles (.userdata lua-tiles)
        xs (persistent! (reduce-kv #(conj! %1 %3) (transient []) (tiles)))
        n (count xs)
        vi (volatile! (int 0))]
    (DefoldVarargsLuaFn.
      (fn ext-next [_]
        (let [i (int @vi)]
          (if (< i n)
            (let [^Tile tile (xs i)]
              (vswap! vi unchecked-inc-int)
              (LuaValue/varargsOf
                ;; 0-indexed (editor) -> 1-indexed (lua)
                (LuaValue/valueOf (unchecked-inc-int (int (.-x tile))))
                (LuaValue/valueOf (unchecked-inc-int (int (.-y tile))))
                (LuaValue/valueOf (unchecked-inc-int (int (.-tile tile))))))
            LuaValue/NIL))))))

;; endregion

;; region writes

(defn- ext-clear [^LuaValue lua-tiles]
  (let [tiles (.checkuserdata lua-tiles)]
    (tiles empty)
    lua-tiles))

(defn- ext-set [^LuaValue lua-tiles ^LuaValue lua-x ^LuaValue lua-y ^LuaValue lua-tile-or-info]
  (let [tiles (.checkuserdata lua-tiles)
        ;; 1-indexed (lua) -> 0-indexed (editor)
        x (unchecked-dec (.checklong lua-x))
        y (unchecked-dec (.checklong lua-y))
        index (tile-map-common/cell-index x y)
        tile (if (instance? LuaNumber lua-tile-or-info)
               ;; 1-indexed (lua) -> 0-indexed (editor)
               (tile-map-common/->Tile x y (unchecked-dec (.checklong lua-tile-or-info)) false false false)
               (tile-map-common/->Tile
                 x y
                 ;; 1-indexed (lua) -> 0-indexed (editor)
                 (unchecked-dec (.checklong (.rawget lua-tile-or-info index-lua-str)))
                 (.toboolean (.rawget lua-tile-or-info h-flip-lua-str))
                 (.toboolean (.rawget lua-tile-or-info v-flip-lua-str))
                 (.toboolean (.rawget lua-tile-or-info rotate-90-lua-str))))]
    (tiles assoc index tile)
    lua-tiles))

(defn- ext-remove [^LuaValue lua-tiles ^LuaValue lua-x ^LuaValue lua-y]
  (let [tiles (.checkuserdata lua-tiles)
        ;; 1-indexed (lua) -> 0-indexed (editor)
        x (unchecked-dec (.checklong lua-x))
        y (unchecked-dec (.checklong lua-y))]
    (tiles dissoc (tile-map-common/cell-index x y))
    lua-tiles))

;; endregion

(def env
  {"tiles" {"new" (DefoldZeroArgsLuaFn. make)

            "get_tile" (DefoldThreeArgsLuaFn. ext-get-tile)
            "get_info" (DefoldThreeArgsLuaFn. ext-get-info)
            "iterator" (DefoldOneArgLuaFn. ext-iterator)

            "clear" (DefoldOneArgLuaFn. ext-clear)
            "remove" (DefoldThreeArgsLuaFn. ext-remove)
            "set" (DefoldFourArgsLuaFn. ext-set)}})
