;; Copyright 2020-2024 The Defold Foundation
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
            [editor.resource :as resource]
            [dynamo.graph :as g])
  (:import [java.io FileNotFoundException]))

(defn file-not-found-error [node-id label severity resource]
  (g/->error node-id label severity nil
             (format "The file '%s' could not be found."
                     (resource/proj-path resource))
             {:type :file-not-found
              :resource resource}))

(defn file-not-found-error? [error]
  (= :file-not-found (-> error :user-data :type)))

(defn invalid-content-error [node-id label severity resource exception]
  (g/->error node-id label severity nil
             (format "The file '%s' could not be loaded%s"
                     (resource/proj-path resource)
                     (if-some [message (ex-message exception)]
                       (str ": " message)
                       "."))
             {:type :invalid-content
              :resource resource
              :exception exception}))

(defmacro with-error-translation
  "Perform body, translate io exceptions to g/errors"
  [resource node-id label & body]
  `(try
     ~@body
     (catch FileNotFoundException _e#
       (file-not-found-error ~node-id ~label :fatal ~resource))
     (catch Exception e#
       (invalid-content-error ~node-id ~label :fatal ~resource e#))))

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
