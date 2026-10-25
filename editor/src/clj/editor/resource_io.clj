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

(ns editor.resource-io
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.localization :as localization]
            [editor.resource :as resource]
            [util.path :as path])
  (:import [java.io FileNotFoundException]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn file-not-found-error [node-id label severity resource]
  (let [symlink-target-pathname (when (resource/symlink? resource)
                                  (some-> resource path/symlink-target path/absolute str))
        path (resource/proj-path resource)
        message (if symlink-target-pathname
                  (localization/message "error.resource-is-a-broken-symlink" {"resource" path "path" symlink-target-pathname})
                  (localization/message "error.resource-not-found" {"resource" path}))
        user-data (cond-> {:type :file-not-found
                           :resource resource}
                    symlink-target-pathname (assoc :symlink-target-pathname symlink-target-pathname))]
    (g/->error node-id label severity nil message user-data)))

(defn file-not-found-error? [error]
  (= :file-not-found (-> error :user-data :type)))

(defn invalid-content-error [node-id label severity resource exception]
  (g/->error node-id label severity nil
             (if-let [message (ex-message exception)]
               (localization/message "error.resource-not-loaded-with-error"
                                     {"resource" (resource/proj-path resource)
                                      "error" message})
               (localization/message "error.resource-not-loaded"
                                     {"resource" (resource/proj-path resource)}))
             {:type :invalid-content :resource resource :exception exception}))

(defmacro with-error-translation
  "Perform body, translate io exceptions to g/errors"
  [resource node-id label & body]
  `(try
     ~@body
     (catch FileNotFoundException _e#
       (file-not-found-error ~node-id ~label :fatal ~resource))
     (catch Exception e#
       (invalid-content-error ~node-id ~label :fatal ~resource e#))))

(defn read-source-value!
  "Returns an unmaterialized resource node's cached source-value, or reads it
  and initializes source-value and disk-sha256 on the ShellNode. Read failures
  return an ErrorValue without caching it."
  [read-opts node-id resource evaluation-context]
  {:pre [(g/node-id? node-id)
         (resource/resource? resource)]}
  (with-error-translation resource node-id :source-value
    (or (g/node-value node-id :source-value evaluation-context)
        (let [{:keys [editable->type-ext->resource-type]} read-opts
              {:keys [read-fn]} (resource/lookup-resource-type resource editable->type-ext->resource-type)]
          (when read-fn
            (let [read-stream-fn #(read-fn read-opts resource %)
                  [source-value disk-sha256] (resource/read-source-value+sha256-hex resource read-stream-fn)
                  shell-node-properties {:source-value source-value
                                         :disk-sha256 disk-sha256}]
              (g/initialize-shell-properties! node-id shell-node-properties evaluation-context)
              source-value))))))

(defn with-translated-error-thrown!
  "Takes a value returned from the with-error-translation macro and throws a
  suitable exception if it is an ErrorValue translated from an exception.
  Otherwise, returns the value."
  [value]
  (or (when (file-not-found-error? value)
        (let [resource (-> value :user-data :resource)
              file (io/as-file resource)
              path (.getPath file)]
          (throw (FileNotFoundException. path))))
      (when (g/error-fatal? value)
        (when-some [exception (-> value :user-data :exception)]
          (throw exception)))
      value))
