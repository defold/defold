(ns editor.validation
  (:require [dynamo.graph :as g]
            [editor.protobuf :as protobuf]
            [clojure.string :as str]
            [editor.properties :as properties]
            [editor.resource :as resource]))

(set! *warn-on-reflection* true)

(defmacro validate-positive
  ([field]
    (validate-positive field "must be greater than or equal to 0"))
  ([field message]
    `(g/fnk [~field]
       (when (neg? ~field)
         (g/error-fatal ~message)))))

(defmacro blend-mode-tip [field pb-blend-type]
  `(g/fnk [~field]
     (when (= ~field :blend-mode-add-alpha)
       (let [options# (protobuf/enum-values ~pb-blend-type)
             options# (zipmap (map first options#) (map (comp :display-name second) options#))]
         (format "\"%s\" has been replaced by \"%s\"",
                 (options# :blend-mode-add-alpha) (options# :blend-mode-add))))))

(defn- validate-resource-fn
  ([field] (validate-resource-fn field (str "Missing " (str/replace (name field) "-" " "))))
  ([field message] (validate-resource-fn field message []))
  ([field message inputs]
   `(g/fnk [~field ~@inputs]
           (when (nil? ~field)
             (g/error-warning ~message)))))

(defmacro validate-resource [& args]
  (apply validate-resource-fn args))

(defmacro validate-animation [animation anim-data]
  `(g/fnk [~animation ~anim-data]
     (when (and (some? ~anim-data) (not (contains? ~anim-data ~animation)))
       (g/error-fatal (format "The animation \"%s\" could not be found in the specified image" ~animation)))))

(defn prop-negative? [v name]
  (when (< v 0)
    (format "'%s' must be positive" name)))

(defn prop-nil? [v name]
  (when (nil? v)
    (format "'%s' must be specified" name)))

(defn prop-empty? [v name]
  (when (empty? v)
    (format "'%s' must be specified" name)))

(defn prop-resource-not-exists? [v name]
  (and v (not (resource/exists? v)) (format "%s '%s' could not be found" name (resource/resource-name v))))

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
