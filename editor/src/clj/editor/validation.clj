(ns editor.validation
  (:require [dynamo.graph :as g]
            [clojure.string :as str]))

(defn- error-seq [e]
  (tree-seq :causes :causes e))

(defn- error-messages [e]
  (letfn [(extract-message [user-data]
          (cond
            (string? user-data) user-data
            (map? user-data) (:message user-data)
            true nil))]
    (distinct (keep (comp extract-message :user-data) (error-seq e)))))

(defn error-message [e]
  (str/join "\n" (error-messages e)))

(defn error-aggregate [vals]
  (when-let [errors (seq (remove nil? (distinct (filter g/error? vals))))]
    (-> (g/error-aggregate errors))))

(defmacro validate-positive [field message]
  `(g/fnk [~field]
     (when (neg? ~field)
       (g/error-severe ~message))))

(defmacro validate-blend-mode [field pb-blend-type]
  `(g/fnk [~field]
     (when (= ~field :blend-mode-add-alpha)
       (let [options# (protobuf/enum-values ~pb-blend-type)
             options# (zipmap (map first options#) (map (comp :display-name second) options#))]
         (g/error-info (format "\"%s\" has been replaced by \"%s\"",
                         (options# :blend-mode-add-alpha) (options# :blend-mode-add)))))))

(defmacro validate-resource [field message inputs]
  `(g/fnk [~field ~@inputs :as all#]
     (if (nil? ~field)
       (g/error-warning ~message)
       (error-aggregate (vals all#)))))

(defmacro validate-animation [animation anim-data]
  `(g/fnk [~animation ~anim-data]
     (when (or (g/error? ~anim-data) (not (contains? ~anim-data ~animation)))
       (g/error-severe (format "The animation \"%s\" could not be found in the specified image" ~animation)))))
