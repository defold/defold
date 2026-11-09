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
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.fs :as fs]
            [editor.resource :as resource]
            [util.http-server :as http-server])
  (:import [java.io File]
           [java.nio.file.attribute FileTime]
           [java.util.zip ZipEntry ZipOutputStream]))

(set! *warn-on-reflection* true)

(defn- local-extension-path [property-key]
  (when-let [[_ extension-name] (re-matches #"defold\.extension\.(.+?)\.url" property-key)]
    (System/getProperty (str "defold.extension." extension-name ".path"))))

(defn- handle-request [request]
  (if-let [local-path (local-extension-path (-> request :path-params :id))]
    (let [local-dir (io/file local-path)]
      (if (.isDirectory local-dir)
        (let [output-file (fs/create-temp-file! (str "local-" (.getName local-dir)) ".zip")]
          (with-open [zip-output-stream (ZipOutputStream. (io/output-stream output-file))]
            (let [local-dir (.getCanonicalFile local-dir)]
              (reduce
                (fn [_ ^File source-file]
                  (let [entry-name (resource/relative-path local-dir source-file)
                        entry (doto (ZipEntry. entry-name)
                                (.setSize (.length source-file))
                                (.setLastModifiedTime (FileTime/fromMillis (.lastModified source-file))))]
                    (.putNextEntry zip-output-stream entry)
                    (io/copy source-file zip-output-stream)
                    (.closeEntry zip-output-stream)))
                nil
                (fs/file-walker local-dir false ["build"])))
            (.finish zip-output-stream))
          (http-server/response 200 output-file))
        http-server/not-found))
    http-server/not-found))

(defonce ^:private server
  (delay (http-server/start! (http-server/router-handler {"/{id}" {"GET" #'handle-request}}))))

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
        (if (local-extension-path jvm-property-key)
          (str (http-server/local-url @server) "/" jvm-property-key)
          value)))))
