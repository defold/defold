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

(ns editor.texture-util
  (:require [dynamo.graph :as g]
            [editor.gl.texture :as texture]
            [editor.graphics.types :as graphics.types]
            [editor.image-util :as image-util]
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [internal.java :as java]
            [util.coll :as coll :refer [pair]]
            [util.digest :as digest])
  (:import [editor.gl.texture TextureLifecycle TextureRequestData]
           [java.awt.image BufferedImage]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; -----------------------------------------------------------------------------
;; Generators
;; -----------------------------------------------------------------------------

(defn generator?
  [value]
  (and (map? value)
       (ifn? (:f value))
       (let [args (:args value)]
         (or (nil? args)
             (map? args)))))

(defn content-generator?
  [value]
  (and (generator? value)
       (digest/sha1-hex? (:sha1 value))))

(defn texture-lifecycle-generator?
  [value]
  (and (generator? value)
       (graphics.types/request-id? (:request-id (:args value)))))

(defn content-generatable?
  [value]
  (or (content-generator? value)
      (and (sequential? value)
           (every? content-generator? value))))

(defn call-generator
  [generator & additional-args]
  (let [{:keys [f args]} generator]
    (apply f args additional-args)))

;; -----------------------------------------------------------------------------
;; BufferedImages
;; -----------------------------------------------------------------------------

(defn- buffered-image-gen-fn
  [{:keys [digest-ignored/error-label digest-ignored/error-node-id resource]}]
  (resource-io/with-error-translation resource error-node-id error-label
    (image-util/read-image resource)))

(defn make-buffered-image-generator
  [resource error-node-id error-label]
  {:post [(or (g/error-value? %)
              (content-generator? %))]}
  (let [sha1 (resource-io/with-error-translation resource error-node-id error-label
               (resource/resource->path-inclusive-sha1-hex resource))]
    (if (g/error-value? sha1)
      sha1
      {:f buffered-image-gen-fn
       :sha1 sha1
       :args {:resource resource
              :digest-ignored/error-node-id error-node-id
              :digest-ignored/error-label error-label}})))

;; -----------------------------------------------------------------------------
;; TextureRequestData
;; -----------------------------------------------------------------------------

(defn make-texture-request-datas
  ^TextureRequestData [content-generatable texture-profile flip-y]
  {:pre [(content-generatable? content-generatable)
         (or (nil? texture-profile)
             (graphics.types/texture-profile? texture-profile))
         (boolean? flip-y)]
   :post [(or (g/error-value? %)
              (and (vector? %)
                   (every? texture/texture-request-data? %)))]}
  (if (sequential? content-generatable)
    ;; This is a sequence of content-generators. In this situation, we expect
    ;; each content-generator to return a single BufferedImage or an ErrorValue.
    (let [texture-request-datas
          (coll/into-> content-generatable []
            (map (fn [content-generator]
                   {:pre [(content-generator? content-generator)]}
                   (let [content (call-generator content-generator)]
                     (cond
                       (g/error-value? content)
                       content

                       (instance? BufferedImage content)
                       (let [texture-image (tex-gen/make-preview-texture-image content texture-profile flip-y)
                             data-version (java/combine-hashes
                                            (hash (:sha1 content-generator))
                                            (hash flip-y))]
                         (texture/texture-image->texture-request-data texture-image data-version))

                       :else
                       (throw
                         (ex-info
                           "content-generator returned an unsupported value."
                           {:content-generator content-generator
                            :return-value content})))))))]
      (g/precluding-errors texture-request-datas texture-request-datas))

    ;; This is a single content-generator. In this situation we expect it to
    ;; return a BufferedImage, an ErrorValue, or a sequence of values that may
    ;; be a mix of BufferedImages or ErrorValues.
    (let [content (call-generator content-generatable)
          data-version (java/combine-hashes
                         (hash (:sha1 content-generatable))
                         (hash flip-y))]
      (cond
        (g/error-value? content)
        content

        (instance? BufferedImage content)
        (let [texture-image (tex-gen/make-preview-texture-image content texture-profile flip-y)]
          [(texture/texture-image->texture-request-data texture-image data-version)])

        (sequential? content)
        (g/precluding-errors content
          (coll/into-> content []
            (map-indexed
              (fn [^long image-index ^BufferedImage buffered-image]
                {:pre [(instance? BufferedImage buffered-image)]}
                (let [texture-image (tex-gen/make-preview-texture-image buffered-image texture-profile flip-y)
                      data-version (java/combine-hashes data-version image-index)]
                  (texture/texture-image->texture-request-data texture-image data-version))))))

        :else
        (throw
          (ex-info
            "content-generator returned an unsupported value."
            {:content-generator content-generatable
             :return-value content}))))))

(defn make-texture-request-datas-by-key
  [content-generators-by-key texture-profile flip-y]
  {:post [(or (g/error-value? %)
              (and (map? %)
                   (every? texture/texture-request-data? (vals %))))]}
  (let [[texture-request-datas-by-key error-values]
        (reduce
          (fn [[texture-request-datas-by-key error-values] [key content-generator]]
            {:pre [(content-generator? content-generator)]}
            (let [texture-request-datas (make-texture-request-datas content-generator texture-profile flip-y)]
              (if (g/error-value? texture-request-datas)
                (pair texture-request-datas-by-key
                      (conj error-values texture-request-datas))
                (let [texture-request-data (first texture-request-datas)]
                  (if (texture/texture-request-data? texture-request-data)
                    (pair (assoc texture-request-datas-by-key key texture-request-data)
                          error-values)
                    (throw
                      (ex-info
                        (format "content-generator for key %s returned an unsupported value." key)
                        {:key key
                         :content-generator content-generator
                         :return-value texture-request-datas})))))))
          (pair {} [])
          content-generators-by-key)]
    (g/precluding-errors error-values texture-request-datas-by-key)))

;; -----------------------------------------------------------------------------
;; 2D TextureLifecycles
;; -----------------------------------------------------------------------------

(defn make-gpu-texture
  ^TextureLifecycle [request-id texture-request-datas]
  (if (g/error-value? texture-request-datas)
    texture-request-datas
    (let [texture-units (apply vector-of :int (range (count texture-request-datas)))]
      (texture/make-gpu-texture request-id texture-request-datas texture-units texture/default-image-texture-params))))

(defn- gpu-texture-gen-fn
  [{:keys [request-id texture-request-datas-delay]}]
  (let [texture-request-datas (force texture-request-datas-delay)]
    (make-gpu-texture request-id texture-request-datas)))

(defn gpu-texture-generator?
  [value]
  (and (texture-lifecycle-generator? value)
       (delay? (:texture-request-datas-delay (:args value)))))

(defn make-gpu-texture-generator
  [request-id content-generatable texture-profile]
  {:pre [(graphics.types/request-id? request-id)
         (content-generatable? content-generatable)
         (or (nil? texture-profile)
             (graphics.types/texture-profile? texture-profile))]
   :post [(gpu-texture-generator? %)]}
  (let [texture-request-datas-delay
        (delay
          (make-texture-request-datas content-generatable texture-profile true))]
    {:f gpu-texture-gen-fn
     :args {:request-id request-id
            :texture-request-datas-delay texture-request-datas-delay}}))

(defn generate-gpu-texture
  ^TextureLifecycle [gpu-texture-generator]
  {:pre [(texture-lifecycle-generator? gpu-texture-generator)]
   :post [(or (g/error-value? %)
              (texture/texture-lifecycle? %))]}
  (call-generator gpu-texture-generator))

(defn construct-gpu-texture
  ^TextureLifecycle [request-id content-generatable texture-profile]
  {:post [(or (g/error-value? %)
              (texture/texture-lifecycle? %))]}
  (let [texture-request-datas (make-texture-request-datas content-generatable texture-profile true)]
    (make-gpu-texture request-id texture-request-datas)))

(def placeholder-gpu-texture-generator
  {:f gpu-texture-gen-fn
   :args {:request-id ::placeholder-gpu-texture
          :texture-request-datas-delay (delay texture/placeholder-texture-request-datas)}})

(assert (gpu-texture-generator? placeholder-gpu-texture-generator))

;; -----------------------------------------------------------------------------
;; Cube TextureLifecycles
;; -----------------------------------------------------------------------------

(defn make-cubemap-gpu-texture
  ^TextureLifecycle [request-id texture-request-datas-by-side-kw]
  (if (g/error-value? texture-request-datas-by-side-kw)
    texture-request-datas-by-side-kw
    (let [texture-units (vector-of :int 0)]
      (texture/make-gpu-cubemap-texture request-id [texture-request-datas-by-side-kw] texture-units texture/default-cubemap-texture-params))))

(defn- cubemap-gpu-texture-gen-fn
  [{:keys [request-id texture-request-datas-by-side-kw-delay]}]
  (let [texture-request-datas-by-side-kw (force texture-request-datas-by-side-kw-delay)]
    (make-cubemap-gpu-texture request-id texture-request-datas-by-side-kw)))

(defn cubemap-gpu-texture-generator?
  [value]
  (and (generator? value)
       (let [args (:args value)]
         (and (graphics.types/request-id? (:request-id args))
              (delay? (:texture-request-datas-by-side-kw-delay args))))))

(defn make-cubemap-gpu-texture-generator
  [request-id content-generators-by-side-kw texture-profile]
  {:pre [(graphics.types/request-id? request-id)
         (map? content-generators-by-side-kw)
         (every? texture/cubemap-side-kw? (keys content-generators-by-side-kw))
         (every? content-generator? (vals content-generators-by-side-kw))
         (or (nil? texture-profile)
             (graphics.types/texture-profile? texture-profile))]
   :post [(cubemap-gpu-texture-generator? %)]}
  (let [texture-request-datas-by-side-kw-delay
        (delay
          (make-texture-request-datas-by-key content-generators-by-side-kw texture-profile false))]
    {:f cubemap-gpu-texture-gen-fn
     :args {:request-id request-id
            :texture-request-datas-by-side-kw-delay texture-request-datas-by-side-kw-delay}}))

(defn construct-cubemap-gpu-texture
  ^TextureLifecycle [request-id content-generators-by-side-kw texture-profile]
  {:post [(or (g/error-value? %)
              (texture/texture-lifecycle? %))]}
  (let [texture-request-datas-by-side-kw (make-texture-request-datas-by-key content-generators-by-side-kw texture-profile false)]
    (make-cubemap-gpu-texture request-id texture-request-datas-by-side-kw)))
