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

(ns hooks.clojure-core
  (:refer-clojure :exclude [bounded-count definterface defprotocol defrecord deftype empty? every? map not-any? not-empty not-every? some])
  (:require [clj-kondo.hooks-api :as api]))

(defn- warn-prefer-util-coll! [node function-name]
  (let [warning-node (or (first (:children node)) node)]
    (api/reg-finding!
      (assoc (meta warning-node)
             :message (format "Use util.coll/%s instead of clojure.core/%s."
                              function-name
                              function-name)
             :type :defold/prefer-util-coll))))

(defn- returnable-tail-expr? [arg expr]
  (or (= arg expr)
      (when (seq? expr)
        (case (first expr)
          do (returnable-tail-expr? arg (last expr))
          let (returnable-tail-expr? arg (last expr))
          let* (returnable-tail-expr? arg (last expr))
          when (returnable-tail-expr? arg (last expr))
          when-not (returnable-tail-expr? arg (last expr))
          when-let (returnable-tail-expr? arg (last expr))
          when-some (returnable-tail-expr? arg (last expr))
          if (or (returnable-tail-expr? arg (nth expr 2 nil))
                 (returnable-tail-expr? arg (nth expr 3 nil)))
          if-not (or (returnable-tail-expr? arg (nth expr 2 nil))
                     (returnable-tail-expr? arg (nth expr 3 nil)))
          if-let (or (returnable-tail-expr? arg (nth expr 2 nil))
                     (returnable-tail-expr? arg (nth expr 3 nil)))
          if-some (or (returnable-tail-expr? arg (nth expr 2 nil))
                      (returnable-tail-expr? arg (nth expr 3 nil)))
          and (returnable-tail-expr? arg (last expr))
          false))))

(defn- arg-symbol [arg]
  (cond
    (symbol? arg) arg
    (vector? arg) (reduce (fn [_ pair]
                            (when (= :as (first pair))
                              (reduced (second pair))))
                          nil
                          (partition 2 1 arg))
    (map? arg) (:as arg)
    :else nil))

(defn- function-returning-argument? [function-sexpr]
  (when (and (seq? function-sexpr)
             (#{'fn 'fn*} (first function-sexpr)))
    (let [[_ arg-vector body] function-sexpr
          arg (arg-symbol (first arg-vector))]
      (and (= 1 (count arg-vector))
           (some? arg)
           (returnable-tail-expr? arg body)))))

(defn- warn-prefer-coll-some! [node replacement]
  (api/reg-finding!
    (assoc (meta node)
           :message (format "Use util.coll/%s instead of clojure.core/some."
                            replacement)
           :type :defold/prefer-util-coll)))

(defn- compare-function-replacement [function-sexpr]
  (when (and (seq? function-sexpr)
             (#{'fn 'fn*} (first function-sexpr)))
    (let [function-tail (rest function-sexpr)
          [_ function-tail] (if (symbol? (first function-tail))
                              [(first function-tail) (rest function-tail)]
                              [nil function-tail])
          [arg-vector body] function-tail
          first-arg (first arg-vector)
          second-arg (second arg-vector)]
      (when (and (= 2 (count arg-vector))
                 (symbol? first-arg)
                 (symbol? second-arg)
                 (seq? body)
                 (= 'compare (first body)))
        (let [[_ compare-first-arg compare-second-arg] body]
          (when (and (= first-arg compare-second-arg)
                     (= second-arg compare-first-arg))
            "descending-order"))))))

(defn- warn-prefer-coll-order! [node replacement]
  (api/reg-finding!
    (assoc (meta node)
           :message (format "Use util.coll/%s instead of an inline compare function."
                            replacement)
           :type :defold/prefer-util-coll)))

(defn bounded-count [{:keys [node]}]
  (warn-prefer-util-coll! node "bounded-count")
  {:node node})

(defn- warn-prefer-defonce! [node defonce-form core-form]
  (let [[form-node] (:children node)]
    (when-not (:defold/defonce (meta form-node))
      (api/reg-finding!
        (assoc (meta form-node)
               :message (format "Use util.defonce/%s instead of clojure.core/%s."
                                defonce-form
                                core-form)
               :type :defold/prefer-defonce))))
  {:node node})

(defn defprotocol [{:keys [node]}]
  (warn-prefer-defonce! node "protocol" "defprotocol"))

(defn definterface [{:keys [node]}]
  (warn-prefer-defonce! node "interface" "definterface"))

(defn defrecord [{:keys [node]}]
  (warn-prefer-defonce! node "record" "defrecord"))

(defn deftype [{:keys [node]}]
  (warn-prefer-defonce! node "type" "deftype"))

(defn fn-call [{:keys [node]}]
  (let [[fn-node] (:children node)
        finding-node (if (:row (meta fn-node)) fn-node node)]
    (when-let [replacement (compare-function-replacement (api/sexpr node))]
      (warn-prefer-coll-order! finding-node replacement))
    {:node node}))

(defn empty? [{:keys [node]}]
  (warn-prefer-util-coll! node "empty?")
  {:node node})

(defn every? [{:keys [node]}]
  (warn-prefer-util-coll! node "every?")
  {:node node})

(defn- map-fn-node [map-node arity]
  (let [args (mapv #(api/token-node (symbol (str "_x" % "__"))) (range arity))]
    (api/list-node
      [(api/token-node 'fn)
       (api/vector-node args)
       (api/list-node
         (list* (api/token-node 'get)
                map-node
                args))])))

(defn map [{:keys [node]}]
  (let [[map-symbol-node f-node & coll-nodes] (:children node)
        f-arity (case (count coll-nodes)
                  0 1
                  1 1
                  2 2
                  nil)]
    {:node
     (if (and f-arity (= :map (:tag f-node)))
       (api/list-node
         (list* map-symbol-node
                (map-fn-node f-node f-arity)
                coll-nodes))
       node)}))

(defn not-any? [{:keys [node]}]
  (warn-prefer-util-coll! node "not-any?")
  {:node node})

(defn not-empty [{:keys [node]}]
  (warn-prefer-util-coll! node "not-empty")
  {:node node})

(defn not-every? [{:keys [node]}]
  (warn-prefer-util-coll! node "not-every?")
  {:node node})

(defn some [{:keys [node]}]
  (let [[some-node pred-node] (:children node)
        replacement (if (function-returning-argument? (api/sexpr pred-node))
                      "first-where"
                      "some")]
    (warn-prefer-coll-some! some-node replacement)
    {:node node}))
