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

(ns editor.scene-async
  (:require [util.profiler :as profiler])
  (:import [com.jogamp.opengl GL2]
           [java.nio ByteBuffer ByteOrder]
           [javafx.scene.image PixelBuffer PixelFormat WritableImage]
           [javafx.util Callback]))

(set! *warn-on-reflection* true)

;; NOTE You have to know what you are doing if you want to change this value. If
;; it does not match the native format exactly,
;; image.getPixelWriter().setPixels(...) below will take a lot more time. BGRA
;; is the native format for most GPUs so by using that in our CPU side buffers
;; we avoid conversions.
(def ^:private ^PixelFormat pixel-format (PixelFormat/getByteBgraPreInstance))

(defn- make-direct-buffer-backed-writable-image
  [width height]
  (let [buf (doto (ByteBuffer/allocateDirect (* width height 4))
              (.order (ByteOrder/nativeOrder)))
        pixel-buffer (PixelBuffer. width height buf pixel-format)]
    {:byte-buffer buf
     :pixel-buffer pixel-buffer
     :image (WritableImage. pixel-buffer)}))

(definterface IPixelWriteCallback
  (set_buffer_BANG_ [x]))

(deftype PixelWriteCallback
    [^:unsynchronized-mutable ^ByteBuffer from-buf]
  IPixelWriteCallback
  (set-buffer! [_ x]
    (set! from-buf x))
  Callback
  (call [_ pb]
    (let [^ByteBuffer to-buf (.getBuffer ^PixelBuffer pb)]
      (.rewind from-buf)
      (.rewind to-buf)
      ;; NOTE: We copy the pixels here instead of just letting OpenGL write
      ;; directly to the backing ByteBuffer of the PixelBuffer. This may seem
      ;; strange. And it is. The reason we do it is that this turned out to
      ;; actually be faster! glBufferSubData was tested, which can put the
      ;; pixels directly in the buffer instead of returning a new one like
      ;; glMapBuffer does. But glBufferSubData turned out to be slower. At least
      ;; this way of copying pixels is slightly faster than what we had before
      ;; (just putting pixels in the WritableImage without accessing the
      ;; underlying buffer).
      (.put to-buf from-buf)
      (.flip to-buf)
      nil)))

(defn make-async-copy-state [width height]
  {:width width
   :height height
   :state :done
   :pbo 0
   :pbo-size nil
   :images [(make-direct-buffer-backed-writable-image width height)
            (make-direct-buffer-backed-writable-image width height)]
   :buffer-update-callback (->PixelWriteCallback nil)
   :current-image 0})

(defn request-resize! [async-copy-state width height]
  (assoc async-copy-state :width width :height height))

(defn dispose! [{:keys [pbo]} ^GL2 gl]
  (.glDeleteBuffers gl 1 (int-array [pbo]) 0)
  nil)

(defmulti begin-read! (fn [async-copy-state ^GL2 gl] (:state async-copy-state)))
(defmulti finish-image! :state)

(defn image
  ^WritableImage [{:keys [current-image images] :as async-copy-state}]
  (:image (nth images current-image)))

(defn- lazy-init! [{:keys [^int pbo] :as async-copy-state} ^GL2 gl]
  (cond-> async-copy-state
          (zero? pbo)
          (assoc :pbo (let [pbos (int-array 1)]
                        (.glGenBuffers gl 1 pbos 0)
                        (first pbos)))))

(defn- bind-pbo! [async-copy-state ^GL2 gl]
  (.glBindBuffer gl GL2/GL_PIXEL_PACK_BUFFER (:pbo async-copy-state))
  async-copy-state)

(defn- unbind-pbo! [async-copy-state ^GL2 gl]
  (.glBindBuffer gl GL2/GL_PIXEL_PACK_BUFFER 0)
  async-copy-state)

(defn- resize-pbo! [{:keys [width height pbo-size] :as async-copy-state} ^GL2 gl]
  (profiler/profile "resize-pbo" -1
    (if (and (= width (:width pbo-size))
             (= height (:height pbo-size)))
      async-copy-state
      (let [data-size (* width height 4)]
        (.glBufferData gl GL2/GL_PIXEL_PACK_BUFFER data-size nil GL2/GL_STREAM_READ)
        (assoc async-copy-state :pbo-size {:width width :height height})))))

(defn- begin-read-pixels-to-pbo! [async-copy-state ^GL2 gl]
  (profiler/profile "fbo->pbo" -1
    ;; NOTE You have to know what you are doing if you want to change these values.
    ;; If it does not match the native format exactly, glReadPixels will take a lot more time.
    ;; The read will apparently happen asynchronously. glMapBuffer below will wait until the read completes.
    (.glReadPixels gl 0 0 ^int (:width async-copy-state) ^int (:height async-copy-state) GL2/GL_BGRA GL2/GL_UNSIGNED_INT_8_8_8_8_REV 0)
    (assoc async-copy-state :state :reading)))

(defmethod begin-read! :done
  [async-copy-state ^GL2 gl]
  (-> async-copy-state
      (lazy-init! gl)
      (bind-pbo! gl)
      (resize-pbo! gl)
      (begin-read-pixels-to-pbo! gl)
      (unbind-pbo! gl)))

(defmethod begin-read! :reading
  [async-copy-state ^GL2 gl]
  (begin-read! (finish-image! async-copy-state gl) gl))

(defmethod finish-image! :done
  [async-copy-state ^GL2 gl]
  async-copy-state)

(defn- next-image [async-copy-state]
  (update async-copy-state :current-image (fn [ix] (mod (inc ix) (count (:images async-copy-state))))))

(defn- resize-image-to-pbo! [{:keys [current-image pbo-size] :as async-copy-state}]
  (profiler/profile "resize-image" -1
    (let [image (get-in async-copy-state [:images current-image])
          writable-image ^WritableImage (:image image)
          {:keys [^int width ^int height]} pbo-size]
      (cond-> async-copy-state
              (or (not= (.getWidth writable-image) width)
                  (not= (.getHeight writable-image) height))
              (assoc-in [:images current-image] (make-direct-buffer-backed-writable-image width height))))))

(defn- copy-pbo-to-image! [async-copy-state ^GL2 gl]
  (profiler/profile "pbo->image" -1
    (let [^PixelBuffer pb (:pixel-buffer (nth (:images async-copy-state) (:current-image async-copy-state)))
          ^PixelWriteCallback cb (:buffer-update-callback async-copy-state)
          ^ByteBuffer gl-buffer (.glMapBuffer gl GL2/GL_PIXEL_PACK_BUFFER GL2/GL_READ_ONLY)]
      (.set-buffer! cb gl-buffer)
      (.updateBuffer pb cb)
      (.glUnmapBuffer gl GL2/GL_PIXEL_PACK_BUFFER)
      (assoc async-copy-state :state :done))))

(defmethod finish-image! :reading
  [async-copy-state ^GL2 gl]
  (-> async-copy-state
      (next-image)
      (resize-image-to-pbo!)
      (bind-pbo! gl)
      (copy-pbo-to-image! gl)
      (unbind-pbo! gl)))

