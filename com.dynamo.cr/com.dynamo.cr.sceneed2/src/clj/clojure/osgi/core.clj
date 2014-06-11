(ns clojure.osgi.core
  (:import [org.osgi.framework Bundle])
  (:import [java.util.concurrent Callable])
  (:import [clojure.osgi BundleClassLoader IClojureOSGi])
  (:import [clojure.lang Namespace]))

(def ^{:private true} osgi-debug false)

(def ^:dynamic *bundle* nil)
(def ^:dynamic *clojure-osgi-bundle*)

(defn bundle-for-ns [ns]
  (let [ns-meta (meta ns)]
    (and ns-meta (::bundle ns-meta))))

(defn bundle-is-usable [^Bundle bundle]
  (not= 0
        (bit-and (bit-or Bundle/RESOLVED Bundle/STARTING Bundle/STOPPING Bundle/ACTIVE)
                 (.getState bundle))))

(defn namespaces-for-bundle [^Bundle bundle]
  (let [bundle-id (.getBundleId bundle)]
    (filter
      (fn [^Namespace ns]
        (let [ns-bundle (bundle-for-ns ns)]
          (and ns-bundle
                   (= (.getBundleId ns-bundle) bundle-id))))
      (all-ns))))

(defn- really-unload-namespace [^Namespace ns]
  (let [ns-sym (.getName ns)
          loaded-libs (.get (clojure.lang.RT/var "clojure.core" "*loaded-libs*"))]
      (dosync
        (alter loaded-libs disj ns-sym))
      (remove-ns (.getName ns))))

(defn namespaces-for-unused-bundles []
  (for [^Namespace ns (all-ns)
        :let [bundle (::bundle (meta ns))]
        :when (and bundle
                   (not (bundle-is-usable bundle)))]
    ns))

(defn unload-namespaces-for-unused-bundles []
  (doseq [ns (namespaces-for-unused-bundles)]
    (really-unload-namespace ns)))

(defn unload-namespaces-for-bundle [^Bundle bundle]
  (doseq [^Namespace ns (namespaces-for-bundle bundle)]
    (really-unload-namespace ns)))

; copy from clojure.core BEGIN
(defn- libspec?
  "Returns true if x is a libspec"
  [x]
  (or (symbol? x)
      (and (vector? x)
           (or
            (nil? (second x))
            (keyword? (second x))))))

(defn- prependss
  "Prepends a symbol or a seq to coll"
  [x coll]
  (if (symbol? x)
    (cons x coll)
    (concat x coll)))

(defn- root-resource
  "Returns the root directory path for a lib"
  {:tag String}
  [lib]
  (str \/
       (.. (name lib)
           (replace \- \_)
           (replace \. \/))))

(defn- root-directory
  "Returns the root resource path for a lib"
  [lib]
  (let [d (root-resource lib)]
    (subs d 0 (.lastIndexOf d "/"))))

(defonce
  ^{:private true
    :dynamic true
    :doc "the set of paths currently being loaded by this thread"}
  *pending-paths* #{})

; copy from clojure.core - END

(defn- full-path [path]
	(if (.startsWith path "/")
	  path
	  (str (root-directory (ns-name *ns*)) \/ path))
)


(defonce 
  ^{:private true :dynamic true}
  *currently-loading* nil)


(defn bundle-class-loader [bundle]
  (BundleClassLoader. bundle))

(let [bundle *bundle*]
  (defn bundle-name []
    (when bundle (.getSymbolicName bundle)))

  (defn get-bundle [^String bsn]
    (when bundle
      (first (filter #(= bsn (.getSymbolicName %))
                     (.. bundle (getBundleContext) (getBundles)))))))

(defn- libspecs [args]
  (flatten 
    (map 
	    (fn [arg]
	      (cond
	        (keyword arg) 
	          arg
	
	        (and (vector? arg) (or (nil? (second arg)) (keyword? (second arg)))) 
	          (first arg)
	
	        :default
	          (let [[prefix & args] arg]
	             (map #(str (name prefix) "."  (if (coll? %) (name (first %)) (name %))) args))))

      (filter (complement keyword?) args))))

(def system-vendor
  (let [vendor-property (System/getProperty "org.osgi.framework.vendor")]
    (if vendor-property
      (constantly vendor-property)
      (fn [& args]
        (when *bundle*
          (-> *bundle* .getBundleContext (.getProperty "org.osgi.framework.vendor")))))))

(declare with-bundle*)
(defmulti bundle-for-resource system-vendor)

(defn- host-part-header-bundle-id [url]
  "Extracts bundle ID from resource URLs in when the bundle ID is at
  the beginning of the host part of a resource URL.
  This is known to be true for both Eclipse/Equinox and current Apache Felix."
  (let [host (.getHost url) dot (.indexOf host  (int \.))]
    (Long/parseLong
      (if (and (>= dot 0) (< dot (- (count host) 1)))
        (.substring host 0 dot) host))))

(defn- host-part-header-bundle-for-resource [bundle resource]
  "Finds the bundle to use given a resource URL, for use with OSGi
  implementations which put the bundle ID at the beginning of resource
  URIs."
  (let [url (.getResource bundle resource)]
    (when osgi-debug
      (println "url for " resource " = " url))
    (when url
      (let [result (.getBundle (.getBundleContext *clojure-osgi-bundle*) (host-part-header-bundle-id url))]
        (when osgi-debug (println "result is" result))
        result))))

; both Apache Felix and Eclipse Equinox comply with this mechanism.
(defmethod bundle-for-resource "Eclipse" [bundle resource]
  (host-part-header-bundle-for-resource bundle resource))
(defmethod bundle-for-resource "Apache Software Foundation" [bundle resource]
  (host-part-header-bundle-for-resource bundle resource))

(defn- available [lib]
  "Return the bundle from which the given resource is available"
  (let [forced-bundle (bundle-for-resource *bundle* (str (root-resource lib) "/.bundle"))
        bundle (or forced-bundle *bundle*)]
    (when (and osgi-debug forced-bundle)
      (println (str "marker for " lib " specifies non-default bundle " bundle)))
    (when (or
           (let [cname (str (namespace-munge lib) "__init")]
             (when osgi-debug (println "trying to load as a class"))
             (try
               (.loadClass bundle cname)
               (catch ClassNotFoundException e
                 (when osgi-debug (println "class not found: " cname)))
               (catch RuntimeException e
                 (if (instance? ClassNotFoundException (.getCause e))
                   (when osgi-debug (println "class not found: " cname))
                   (throw e)))))
     
           (let [rname (str (root-resource lib) ".clj")]
             (when osgi-debug (println "trying to load as a resource"))
             (or (and forced-bundle
                      (.getEntry bundle rname))
                 (.getResource bundle rname)
                 (when osgi-debug (println "resource not found: " rname)))))
      (when osgi-debug (println "success; returning bundle " bundle " for " lib))
      bundle)))

(defn check-libs [libs]
  (doseq [lib libs]
    (when-not (available lib)
      (when osgi-debug
        (println (str lib " is not available from bundle " (.getSymbolicName *bundle*))))
      (throw (Exception. (str "cannot load " lib " from bundle " (.getSymbolicName *bundle*)))))))

(when *bundle*
  (alter-var-root (find-var (symbol "clojure.core" "use"))
    (fn [original]
      (fn [& args]
        (when *bundle*
          (when osgi-debug
            (println (str "use " args " from " (.getSymbolicName *bundle*) ", currently loading: " *currently-loading*)))
          (check-libs (libspecs args)))
        (apply original args))))
  
  (alter-var-root (find-var (symbol "clojure.core" "require")) 
    (fn [original]
      (fn [& args]
        (when *bundle*
          (when osgi-debug
            (println (str "require " args " from " (.getSymbolicName *bundle*) ", currently loading: " *currently-loading*)))
          (check-libs (libspecs args)))
        (apply original args)))))

(defn set-context-classloader! [l]
  (-> (Thread/currentThread) 
    (.setContextClassLoader l)))

(when osgi-debug 
  (println "System vendor detected as <" (system-vendor) ">"))

(alter-var-root (find-var (symbol "clojure.core" "load"))
  (fn [original]
    (fn [path]
      (if (not *bundle*)
        (do
          (when osgi-debug
            (println (str "Bundle not defined in thread-local context; falling back for load of " path)))
          (original path))
        (do
          (when osgi-debug
            (println (str "load " path " from " (.getSymbolicName *bundle*))))
          (let [path (full-path path)]
            (if-not (*pending-paths* path)
              (binding [*pending-paths* (conj *pending-paths* path)
                        *currently-loading* path]
                (let [load (fn [] (clojure.lang.RT/load (.substring path 1)))]
                  (let [forced-bundle (bundle-for-resource *bundle* (str path "/.bundle"))
                        bundle (or forced-bundle
                                   (bundle-for-resource *bundle* (str path ".clj"))
                                   (bundle-for-resource *bundle* (str path "__init.class")))]
                    (if bundle
                      (do
                        (when osgi-debug
                          (println "loading " (.substring path 1) " with bundle " (.getSymbolicName bundle)))
                        (with-bundle* bundle (boolean forced-bundle) load))
                      (do
                        (when osgi-debug
                          (println "loading " (.substring path 1) " with no bundle"))
                        (load)))))))))))))

(alter-var-root (find-var (symbol "clojure.core" "in-ns"))
  (fn [original]
    (fn [n]
      (if *bundle*
        (do
          (when osgi-debug
            (println (str "Associating namespace " n " with bundle " *bundle*)))
          (original (vary-meta n assoc ::bundle *bundle*)))
        (do
          (when osgi-debug
            (println (str "Namespace " n " being loaded, but *bundle* is not set")))
          (original n))))))

(alter-var-root (find-var (symbol "clojure.java.io" "resource"))
  (fn [original]
    (fn 
      ([n]
        (if (not *bundle*)
          (do
            (when osgi-debug
              (println (str "Bundle not defined in thread-local context; falling back for resource " n)))
            (original n))
          (do
            (when osgi-debug
              (println (str "looking for resource " n " from " (.getSymbolicName *bundle*))))
            (if-let [bundle (bundle-for-resource *bundle* n)]
              (do
                (when osgi-debug
                  (println "loading resource " n " with bundle " (.getSymbolicName bundle)))
                (let [new-loader (BundleClassLoader. bundle)
                      old-loader (.getContextClassLoader (Thread/currentThread))]
                  (when osgi-debug
                    (println "new-loader " new-loader))
                  (try
                    (set-context-classloader! new-loader)
                    (original n)
                    (finally 
                      (set-context-classloader! old-loader)))))
              (do
                (when osgi-debug
                  (println "loading " n " with no bundle"))
                (original n))))))
      ([n loader]
        (original n loader)))))

; invokes function in the environment set-up for specified bundle:
;   - classloader is is set to appropriate bundle class loader
;   - clojure.core/load is re-bound with osgi-load
;   - clojure.core/use is re-bound with osgi-use
(defn with-bundle* [bundle force-direct function & params]
  (binding [*bundle* bundle]
    (clojure.osgi.internal.ClojureOSGi/withLoader (BundleClassLoader. bundle force-direct)
      (if (instance? Callable function) 
        function
        (reify Callable
          (call [_]
            (if (seq? params)
              (apply function params) 
              (function))))))))

; convinience macro
(defmacro with-bundle [bundle & body]
  `(with-bundle* ~bundle false
      (fn [] ~@body)))
