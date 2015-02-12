(ns clojure.osgi.services
  (:use (clojure.osgi core))
  (:import
    (clojure.osgi IClojureOSGi)
    (clojure.osgi BundleClassLoader)
    (java.util Properties)
		(org.osgi.util.tracker ServiceTracker ServiceTrackerCustomizer)
    (org.osgi.framework Filter FrameworkUtil)))

;name of java interface that corresponds to given protocol
(defn protocol-interface-name [protocol]
  {:pre [(map? protocol)]}
  (.getName ^Class (:on-interface protocol)))

(defn- ^Properties map-to-properties [m]
	{:pre [(map? m)]}

	(let [result (Properties.)]
		(doseq [[k v] m]
			(.put result k v))

		result))

(defn- ocfilter [name]
  (str "(objectClass=" name ")"))

; service filter
(defprotocol FilterProtocol
	(get-filter ^Filter [this]))

(extend-protocol FilterProtocol
	Filter
		(get-filter [f] f)

	String
		(get-filter [s] (FrameworkUtil/createFilter s)))

(extend-protocol FilterProtocol
  clojure.lang.PersistentArrayMap ; assuming protocol is represented by a map
  (get-filter [p]
    (get-filter
      (ocfilter
       (protocol-interface-name p))))

  java.lang.Class
  (get-filter [c]
    (get-filter
      (ocfilter (.getName c)))))

(defprotocol TrackerDestination
	(adding [callback reference service])
	(removed [callback reference service])
	(modified [clallback reference service]))

(defn track-service [filter ^ServiceTrackerCustomizer customizer]
  (let
    [context (.getBundleContext *bundle*)
     tracker (ServiceTracker. context (get-filter filter) customizer)]
   	 (.open tracker)
   	 tracker))

(defn register-service* [protocol options service]
  (let [context (.getBundleContext *bundle*)
        ^String klass (cond
                        (class? protocol)
                          (.getName ^Class protocol)
                        (map? protocol)
                          (protocol-interface-name protocol)
                        :default
                          (throw (IllegalStateException.)))]
    (.registerService context klass service (map-to-properties options))))

(defmacro register-service [protocol & methods]
	(let [options (if (map? (first methods)) (first methods) {})
        methods (if (map? (first methods)) (next  methods) methods)]

	   `(register-service* ~protocol ~options
		   (reify ~protocol
	     		~@methods))))


(when *bundle*
  (register-service IClojureOSGi
    (unload [_ bundle]
      (unload-namespaces-for-bundle bundle))

      (require [_ bundle name]
        (with-bundle bundle
           (require (symbol name))))

      (withBundle [_ bundle r]
        (with-bundle* bundle false #(.call r)))

      (loadAOTClass [_ bundle name]
        (with-bundle bundle
                     (Class/forName name true
                       (BundleClassLoader. bundle))))))
