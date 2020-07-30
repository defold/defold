; Copyright 2013 Relevance, Inc.
; Copyright 2014 Cognitect, Inc.

; The use and distribution terms for this software are covered by the
; Eclipse Public License 1.0 (http://opensource.org/licenses/eclipse-1.0)
; which can be found in the file epl-v10.html at the root of this distribution.
;
; By using this software in any fashion, you are agreeing to be bound by
; the terms of this license.
;
; You must not remove this notice, or any other, from this software.

(ns service.log
  "Logging via slf4j. Each logging level is a macro: trace, debug,
  info, warn, and error. Each namespace gets its own Logger. Arguments
  are key-value pairs, which will be printed as with 'pr'. The special
  key :exception should have a java.lang.Throwable as its value, and
  will be passed separately to the underlying logging API."
  (:import (org.slf4j LoggerFactory))
  (:require [editor.util :as util]))

(set! *warn-on-reflection* true)

(def ^:dynamic *logging-suppressed* false)

(defmacro without-logging [& body]
  `(binding [*logging-suppressed* true]
     ~@body))

(defn- log-expr [form level keyvals]
  ;; Pull out :exception, otherwise preserve order
  (let [exception' (:exception (apply array-map keyvals))
        keyvals' (mapcat identity (remove #(= :exception (first %))
                                          (partition 2 keyvals)))
        logger' (gensym "logger")  ; for nested syntax-quote
        string' (gensym "string")
        enabled-method' (symbol (str ".is"
                                     (util/capitalize* (name level))
                                     "Enabled"))
        log-method' (symbol (str "." (name level)))]
    `(when-not *logging-suppressed*
       (let [~logger' (LoggerFactory/getLogger ~(name (ns-name *ns*)))]
         (when (~enabled-method' ~logger')
           (let [~string' (binding [*print-length* 80]
                            (pr-str (array-map :line ~(:line (meta form)) ~@keyvals')))]
             ~(if exception'
                `(~log-method' ~logger'
                               ~(with-meta string'
                                  {:tag 'java.lang.String})
                               ~(with-meta exception'
                                  {:tag 'java.lang.Throwable}))
                `(~log-method' ~logger' ~string'))))))))

(defmacro trace [& keyvals] (log-expr &form :trace keyvals))

(defmacro debug [& keyvals] (log-expr &form :debug keyvals))

(defmacro info [& keyvals] (log-expr &form :info keyvals))

(defmacro warn [& keyvals] (log-expr &form :warn keyvals))

(defmacro error [& keyvals] (log-expr &form :error keyvals))

(defmacro spy
  "Logs expr and its value at DEBUG level, returns value."
  [expr]
  (let [value' (gensym "value")]
    `(let [~value' ~expr]
       ~(log-expr &form 'debug (vector :spy (list 'quote expr)
                                       :value value'))
       ~value')))

(defmacro logging-exceptions [where & body]
  (assert (seq? body) "The rest of the arguments to logging-exceptions must be the code body to run with logging.")
  `(try
     (do ~@body)
     (catch Throwable thrown#
       (error :exception thrown# :from ~where)
       nil)))
