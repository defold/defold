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

(ns editor.library
  (:require [editor.progress :as progress]
            [editor.settings-core :as settings-core]
            [util.path :as path])
  (:import [com.dynamo.bob Progress Progress$Reporter]
           [com.dynamo.bob Project]
           [com.dynamo.bob.util Library]))

(set! *warn-on-reflection* true)

(defn parse-uris [uri-string]
  (settings-core/parse-setting-value {:type :list :element {:type :url}} uri-string))

(defn cached [project-directory library-uris]
  (Library/cached (or library-uris []) (path/of project-directory Project/LIB_DIR)))

(defn fetch! [project-directory library-uris render-progress!]
  (Library/fetch
    (or library-uris [])
    (path/of project-directory Project/LIB_DIR)
    nil
    nil
    (Progress.
      (reify Progress$Reporter
        (report [_ message fraction] (render-progress! (progress/bob message fraction)))
        (close [_] (render-progress! progress/done))
        (isCanceled [_] false)))))
