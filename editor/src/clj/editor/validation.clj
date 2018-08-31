(ns editor.validation
  (:require [dynamo.graph :as g]
            [editor.protobuf :as protobuf]
            [clojure.string :as str]
            [editor.properties :as properties]
            [editor.resource :as resource]))

(set! *warn-on-reflection* true)

(defmacro blend-mode-tip [field pb-blend-type]
  `(g/fnk [~field]
     (when (= ~field :blend-mode-add-alpha)
       (let [options# (protobuf/enum-values ~pb-blend-type)
             options# (zipmap (map first options#) (map (comp :display-name second) options#))]
         (format "\"%s\" has been replaced by \"%s\"",
                 (options# :blend-mode-add-alpha) (options# :blend-mode-add))))))

(defn prop-id-duplicate? [id-counts id]
  (when (> (id-counts id) 1)
    (format "'%s' is in use by another instance" id)))

(defn prop-negative? [v name]
  (when (< v 0)
    (format "'%s' must be positive" name)))

(defn prop-zero-or-below? [v name]
  (when (<= v 0)
    (format "'%s' must be greater than zero" name)))

(defn prop-nil? [v name]
  (when (nil? v)
    (format "'%s' must be specified" name)))

(defn prop-empty? [v name]
  (when (empty? v)
    (format "'%s' must be specified" name)))

(defn prop-resource-not-exists? [v name]
  (and v (not (resource/exists? v)) (format "%s '%s' could not be found" name (resource/resource->proj-path v))))

(defn prop-resource-missing? [v name]
  (or (prop-nil? v name)
      (prop-resource-not-exists? v name)))

(defn prop-resource-ext? [v ext name]
  (or (prop-resource-missing? v name)
      (when-not (= (resource/type-ext v) ext)
        (format "%s '%s' is not of type .%s" name (resource/resource->proj-path v) ext))))

(defn prop-member-of? [v val-set message]
  (when (and val-set (not (val-set v)))
    message))

(defn prop-anim-missing? [animation anim-ids]
  (when (and anim-ids (not-any? #(= animation %) anim-ids))
    (format "'%s' could not be found in the specified image" animation)))

(defn prop-outside-range? [[min max] v name]
  (let [tmpl (if (integer? min)
               "'%s' must be between %d and %d"
               "'%s' must be between %f and %f")]
    (when (not (<= min v max))
      (format tmpl name min max))))

(def prop-0-1? (partial prop-outside-range? [0.0 1.0]))

(def prop-1-1? (partial prop-outside-range? [-1.0 1.0]))

(defn prop-error
  ([severity _node-id prop-kw f prop-value & args]
   (when-let [msg (apply f prop-value args)]
     (g/->error _node-id prop-kw severity prop-value msg {}))))

(defmacro prop-error-fnk
  [severity f property]
  (let [name-kw# (keyword property)
        name# (properties/keyword->name name-kw#)]
    `(g/fnk [~'_node-id ~property]
            (prop-error ~severity ~'_node-id ~name-kw# ~f ~property ~name#))))

(defn file-not-found-error [node-id label severity resource]
  (g/->error node-id label severity nil (format "The file '%s' could not be found." (resource/proj-path resource)) {:type :file-not-found :resource resource}))

(defn invalid-content-error [node-id label severity resource]
  (g/->error node-id label severity nil (format "The file '%s' could not be loaded." (resource/proj-path resource)) {:type :invalid-content :resource resource}))

(defn resource-io-with-errors
  "Helper function that performs resource io and translates exceptions to g/errors"
  [io-fn resource node-id label]
  (try
    (io-fn resource)
    (catch java.io.FileNotFoundException e
      (file-not-found-error node-id label :fatal resource))
    (catch Exception _
      (invalid-content-error node-id label :fatal resource))))
