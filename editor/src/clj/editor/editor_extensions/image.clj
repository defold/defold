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

(ns editor.editor-extensions.image
  (:require [clojure.java.io :as io]
            [editor.editor-extensions.runtime :as rt]
            [util.path :as path])
  (:import [com.defold.editor.luart DefoldOneArgLuaFn DefoldVarargsLuaFn]
           [java.awt.image BufferedImage]
           [java.util.concurrent.atomic AtomicInteger]
           [javax.imageio ImageIO]
           [org.luaj.vm2 LuaError LuaValue Varargs]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn env [project-path]
  {"load_file" (DefoldOneArgLuaFn.
                (fn image-load-file [^LuaValue lua-path]
                  (let [path-string (.checkjstring lua-path)
                        path (path/resolve-normalized project-path path-string)]
                    (when-not (path/exists? path)
                      (throw (LuaError. (str "Image file does not exist: " path-string))))
                    (when (path/directory? path)
                      (throw (LuaError. (str "Image path is a directory: " path-string))))
                    (with-open [stream (io/input-stream path)]
                      (rt/wrap-userdata
                        (or (ImageIO/read stream)
                            (throw (LuaError. (str "Unsupported image file: " path-string))))
                        "image")))))

   "size" (DefoldVarargsLuaFn.
           (fn image-size [^Varargs args]
             (let [^BufferedImage image (.checkuserdata (.arg1 args) BufferedImage)]
               (LuaValue/varargsOf
                 (LuaValue/valueOf (.getWidth image))
                 (LuaValue/valueOf (.getHeight image))))))

   "pixel" (DefoldVarargsLuaFn.
            (fn image-pixel [^Varargs args]
              (let [^BufferedImage image (.checkuserdata (.arg1 args) BufferedImage)
                    x (unchecked-dec-int (.checkint args 2))
                    y (unchecked-dec-int (.checkint args 3))]
                (if (or (< x 0) (<= (.getWidth image) x)
                        (< y 0) (<= (.getHeight image) y))
                  (throw (LuaError. (format "Pixel coordinate out of bounds: %d, %d" (inc x) (inc y))))
                  (let [argb-pixel (.getRGB image x y)
                        ^LuaValue/1 values (make-array LuaValue 4)]
                    (aset values 0 (LuaValue/valueOf (bit-and 0xff (bit-shift-right argb-pixel 16))))
                    (aset values 1 (LuaValue/valueOf (bit-and 0xff (bit-shift-right argb-pixel 8))))
                    (aset values 2 (LuaValue/valueOf (bit-and 0xff argb-pixel)))
                    (aset values 3 (LuaValue/valueOf (bit-and 0xff (bit-shift-right argb-pixel 24))))
                    (LuaValue/varargsOf values))))))

   "pixels" (DefoldOneArgLuaFn.
             (fn image-pixels [^LuaValue lua-image]
               (let [^BufferedImage image (.checkuserdata lua-image BufferedImage)
                     width (.getWidth image)
                     height (.getHeight image)
                     pixel-count (Math/multiplyExact width height)
                     ^LuaValue/1 x-values (make-array LuaValue width)
                     ^LuaValue/1 y-values (make-array LuaValue height)
                     i (AtomicInteger. 0)]
                 (dotimes [x width]
                   (aset x-values x (LuaValue/valueOf (unchecked-inc-int x))))
                 (dotimes [y height]
                   (aset y-values y (LuaValue/valueOf (unchecked-inc-int y))))
                 (DefoldVarargsLuaFn.
                  (fn image-pixels-next [_]
                    (let [current-i (.getAndIncrement i)]
                      (if-not (< current-i pixel-count)
                        LuaValue/NIL
                        (let [x (unchecked-remainder-int current-i width)
                              y (unchecked-divide-int current-i width)
                              argb-pixel (.getRGB image x y)
                              ^LuaValue/1 values (make-array LuaValue 6)]
                          (aset values 0 (aget x-values x))
                          (aset values 1 (aget y-values y))
                          (aset values 2 (LuaValue/valueOf (bit-and 0xff (bit-shift-right argb-pixel 16))))
                          (aset values 3 (LuaValue/valueOf (bit-and 0xff (bit-shift-right argb-pixel 8))))
                          (aset values 4 (LuaValue/valueOf (bit-and 0xff argb-pixel)))
                          (aset values 5 (LuaValue/valueOf (bit-and 0xff (bit-shift-right argb-pixel 24))))
                          (LuaValue/varargsOf values)))))))))})
