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

(ns util.defonce
  (:refer-clojure :exclude [type])
  (:import [clojure.lang Var]))

(set! *warn-on-reflection* true)

(defmacro protocol
  "Like core.defprotocol, but will never be redefined."
  [name & opts+sigs]
  (if (not (some-> ^Var (resolve name) (.hasRoot)))
    (list* 'defprotocol name opts+sigs)))

(defmacro record
  "Like core.defrecord, but will never be redefined."
  {:arglists '([name [& fields] & opts+specs])}
  [name & args]
  (if (not (class? (resolve name)))
    (list* 'defrecord name args)))

(defmacro type
  "Like core.deftype, but will never be redefined."
  {:arglists '([name [& fields] & opts+specs])}
  [name & args]
  (if (not (class? (resolve name)))
    (list* 'deftype name args)))

(defmacro interface
  "Like core.definterface, but will never be redefined."
  {:arglists '([name [& fields] & opts+specs])}
  [name & args]
  (if (not (class? (resolve name)))
    (list* 'definterface name args)))
