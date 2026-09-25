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

(ns editor.gl.shader-test
  (:require [clojure.test :refer :all]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.scene-cache :as scene-cache]))

;; Verifies that binding a texture to a shader without samplers does not throw,
;; which previously aborted GUI rendering and logged an exception on every redraw.
(deftest set-samplers-by-index-without-samplers-test
  (let [test-shader (shader/make-shader-lifecycle
                      ::without-samplers
                      (shader/make-shader-request-data [] [] {} nil)
                      [] {})]
    (with-redefs [scene-cache/request-object! (fn [_cache-id _request-id _gl _request-data]
                                                {:program 7
                                                 :uniform-infos {}
                                                 :sampler-index->sampler-name []
                                                 :sampler-name->uniform-names {}})
                  gl/gl-current-program (fn ^long [_gl] 7)]
      (is (nil? (shader/set-samplers-by-index test-shader nil 0 [0]))))))

;; Verifies that an existing sampler still binds each slice to its texture unit
;; when missing samplers are ignored.
(deftest set-samplers-by-index-with-samplers-test
  (let [uniform-updates (atom [])
        test-shader (shader/make-shader-lifecycle
                      ::with-samplers
                      (shader/make-shader-request-data [] [] {} nil)
                      [] {})]
    (with-redefs [scene-cache/request-object! (fn [_cache-id _request-id _gl _request-data]
                                                {:program 7
                                                 :uniform-infos {"texture_sampler_0" {:location 3}
                                                                 "texture_sampler_1" {:location 4}}
                                                 :sampler-index->sampler-name ["texture_sampler"]
                                                 :sampler-name->uniform-names {"texture_sampler" ["texture_sampler_0" "texture_sampler_1"]}})
                  gl/gl-current-program (fn ^long [_gl] 7)
                  shader/set-uniform-at-index (fn [_gl program location value]
                                                (swap! uniform-updates conj [program location value]))]
      (shader/set-samplers-by-index test-shader nil 0 [2 5])
      (is (= [[7 3 2] [7 4 5]] @uniform-updates)))))
