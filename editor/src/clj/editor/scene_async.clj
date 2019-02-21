(ns editor.scene-async
  (:require [util.profiler :as profiler])
  (:import [com.jogamp.opengl GL2]
           [java.nio ByteBuffer]
           [javafx.scene.image PixelFormat WritableImage]))

(set! *warn-on-reflection* true)

(defn make-async-copy-state [width height]
  {:width width
   :height height
   :state :done
   :pbo 0
   :pbo-size nil
   :images [(WritableImage. width height)
            (WritableImage. width height)]
   :current-image 0})

(defn request-resize! [async-copy-state width height]
  (assoc async-copy-state :width width :height height))

(defn dispose! [{:keys [pbo]} ^GL2 gl]
  (.glDeleteBuffers gl 1 (int-array [pbo]) 0)
  nil)

(defmulti begin-read! (fn [async-copy-state ^GL2 gl] (:state async-copy-state)))
(defmulti finish-image! :state)

(defn image ^WritableImage [{:keys [current-image images] :as async-copy-state}]
  (nth images current-image))

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
    (let [image ^WritableImage (get-in async-copy-state [:images current-image])
          {:keys [^int width ^int height]} pbo-size]
      (cond-> async-copy-state
              (or (not= (.getWidth image) width)
                  (not= (.getHeight image) height))
              (assoc-in [:images current-image] (WritableImage. width height))))))

;; NOTE You have to know what you are doing if you want to change this value.
;; If it does not match the native format exactly, image.getPixelWriter().setPixels(...) below will take a lot more time.
(def ^:private ^PixelFormat pixel-format (PixelFormat/getByteBgraPreInstance))

(defn- copy-pbo-to-image! [async-copy-state ^GL2 gl]
  (profiler/profile "pbo->image" -1
    (let [image (image async-copy-state)
          {:keys [^int width ^int height]} (:pbo-size async-copy-state)
          ;; glMapBuffer will wait until glReadPixels completes
          buffer ^ByteBuffer (.glMapBuffer gl GL2/GL_PIXEL_PACK_BUFFER GL2/GL_READ_ONLY)]
      (.. image getPixelWriter (setPixels 0 0 width height pixel-format buffer (* width 4)))
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
