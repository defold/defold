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

(ns local-extensions
  (:require [clojure.string :as string]))

(set! *warn-on-reflection* true)

(defn local-extension-path [property-key]
  (when-let [[_ extension-name] (re-matches #"defold\.extension\.(.+?)\.url" property-key)]
    (System/getProperty (str "defold.extension." extension-name ".path"))))

(defn inject-jvm-properties [^String raw-setting-value]
  (string/replace
    raw-setting-value
    #"\{\{(.+?)\}\}"
    (fn [[_ jvm-property-key]]
      (let [value (or (System/getProperty jvm-property-key)
                      (throw (ex-info (format "Required JVM property `%s` is not defined."
                                              jvm-property-key)
                                      {:jvm-property-key jvm-property-key
                                       :raw-setting-value raw-setting-value})))]
        (if-not (local-extension-path jvm-property-key)
          value
          ;; URL defaults are parsed during parallel namespace loading. Keep
          ;; the server's graph dependencies out of ordinary substitution.
          ((requiring-resolve 'local-extensions.server/url) jvm-property-key))))))
