(ns mikera.cljunit.core
  (:import [org.junit.runner.notification RunNotifier Failure])
  (:use clojure.test))

(set! *warn-on-reflection* true)

;; intended for binding to capture failures
(def ^:dynamic *reports* nil)

(defn assertion-message [m]
  (str "Assertion failed: {:expected " (:expected m) " :actual " (:actual m) "}"
       " <" (:file m) ":" (:line m) ">"))

(defn truncate-stacktrace [off-top]
  (let [st (.getStackTrace (Thread/currentThread))
        stlen (alength st)
        st (java.util.Arrays/copyOfRange st (int off-top) stlen)]
    st))

(def report-fn
  (fn [m]
     ;;(println m)
     (let [m (if (= :fail (:type m))
               (assoc m :stacktrace (truncate-stacktrace 4))
               m)]
       (swap! *reports* conj m))))

(defn notifier-fixture
  [^RunNotifier n desc f]
  (.fireTestStarted n desc)
  (try
	   (f)
	   (catch Throwable t
	     (.fireTestFailure n (Failure. desc t)))
	   (finally
      (.fireTestFinished n desc))))

(defn invoke-test [n desc v]
  (when-let [t v]   ;; (:test (meta v))
    (let [each-fixture (join-fixtures (cons (partial notifier-fixture n desc) (:clojure.test/each-fixtures (meta (:ns (meta v))))))]
      (binding [clojure.test/report report-fn
                *reports* (atom [])]
        (each-fixture
          (fn []
            (eval `(~t))
            ;; (println @*reports*)
            (doseq [m @*reports*]
              (let [type (:type m)]
                (cond
                  (= :pass type) m
                  (= :fail type)
                    (let [ex (junit.framework.AssertionFailedError. (assertion-message m))]
                      ;; (println m)
                      (.setStackTrace ex (:stacktrace m))
                      (throw ex))
                  (= :error type) (throw (:actual m))
                  :else "OK")))))))))

;; (deftest failing-test (is (= 2 3)))
(deftest test-in-core
  (testing "In Core"
    (is (= 1 1))))

(defn ns-for-name [name]
  (namespace (symbol name)))

(defn get-test-vars
  "Gets the vars in a namespace which represent tests, as defined by :test metadata"
  ([ns]
    (filter
      (fn [v] (:test (meta v)))
      (vals (ns-interns ns)))))

(defn invoke-ns-tests
  [notifier desc ns inner-loop]
  (let [once-fixture-fn (join-fixtures (cons (partial notifier-fixture notifier desc) (:clojure.test/once-fixtures (meta (symbol ns)))))]
    (once-fixture-fn
      (fn []
        (inner-loop)))))

(defn get-test-var-names
  "Gets the names of all vars for a given namespace name"
  ([ns-name]
  (try
    (require (symbol ns-name))
    (vec (map
           #(str (first %))
           (filter
             (fn [[k v]] (:test (meta v)))
             (ns-interns (symbol ns-name)))))
    (catch Throwable t
      (throw (ex-info (str "Error attempting to get var names for namespace [" ns-name "]") {} t))))))

;; we need to exclude clojure.parallel, as loading it causes an error if ForkJoin framework not present
(def DEFAULT-EXCLUDES
  ["clojure.parallel"])

(defn test-results [test-vars]
  (vec (map
    (fn [var]
      (let [t (:test (meta test-var))]
        (try
          (t)
          (catch Throwable e
            e)))
    test-vars))))