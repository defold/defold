(ns dynamo.project.test-support
  (:require [clojure.test :refer :all]
            [clojure.core.async :as a]
            [dynamo.project :as p :refer :all])
  (:import [org.eclipse.core.runtime Path IPath]
           [org.eclipse.core.resources IFile IFolder]))

;; create a fake project type (IFile has the methods we need)
;; needs to be able to getFullPath, returning an IPath impl

(defn fake-project []
  (reify
    IFile
    (getFullPath [this] (Path. "testing-project-path/contents"))
    ))

(def ^:dynamic *calls*)

(defn ->bitbucket [] (a/chan (a/dropping-buffer 1)))

(def ^:dynamic *test-project*)

(defn tally [node fn-symbol]
  (swap! *calls* update-in [(:_id node) fn-symbol] (fnil inc 0)))

(defn get-tally [resource fn-symbol]
  (get-in @*calls* [(:_id resource) fn-symbol] 0))

(defmacro expect-call-when [node fn-symbol & body]
  `(let [calls-before# (get-tally ~node ~fn-symbol)]
     ~@body
     (is (= (inc calls-before#) (get-tally ~node ~fn-symbol)))))

(defmacro expect-no-call-when [node fn-symbol & body]
  `(let [calls-before# (get-tally ~node ~fn-symbol)]
     ~@body
     (is (= calls-before# (get-tally ~node ~fn-symbol)))))

(defn clean-project []
  (ref (p/make-project (fake-project) "test" (->bitbucket))))

(defn with-clean-project
  [f]
  (binding [*test-project* (clean-project)]
    (f)))