(ns editor.resource-io
  (:require [editor.resource :as resource]
            [dynamo.graph :as g])
  (:import [java.io FileNotFoundException]))
            

(defn file-not-found-error [node-id label severity resource]
  (g/->error node-id label severity nil (format "The file '%s' could not be found." (resource/proj-path resource)) {:type :file-not-found :resource resource}))

(defn invalid-content-error [node-id label severity resource]
  (g/->error node-id label severity nil (format "The file '%s' could not be loaded." (resource/proj-path resource)) {:type :invalid-content :resource resource}))

(defmacro with-error-translation
  "Perform body, translate io exceptions to g/errors"
  [resource node-id label & body]
  `(try
     ~@body
     (catch java.io.FileNotFoundException e#
       (file-not-found-error ~node-id ~label :fatal ~resource))
     (catch Exception ~'_
       (invalid-content-error ~node-id ~label :fatal ~resource))))
