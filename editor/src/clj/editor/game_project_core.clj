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

(ns editor.game-project-core
  (:require [clojure.java.io :as io]
            [editor.settings-core :as settings-core]))

(set! *warn-on-reflection* true)

(def basic-meta-info
  (with-open [rdr (io/reader (io/resource "com/dynamo/bob/meta.properties"))]
    (settings-core/load-meta-properties rdr)))

(def meta-settings (:settings basic-meta-info))

(def default-settings
  (-> meta-settings
      settings-core/make-default-settings
      settings-core/make-settings-map))
