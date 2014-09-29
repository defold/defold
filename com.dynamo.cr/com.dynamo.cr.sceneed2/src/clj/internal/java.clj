(ns internal.java
  (:import [java.lang.reflect Method Modifier]))

(set! *warn-on-reflection* true)

(defn invoke-no-arg-class-method
  [^Class class ^Method method]
  (-> class (.getDeclaredMethod method (into-array Class []))
      (.invoke nil (into-array Object []))))

(defn daemonize
  [^Runnable f]
  (doto (Thread. f)
    (.setDaemon true)
    (.start)))


(defrecord Field
  [public static final type ^String name java-field]
  clojure.lang.Named
  (getName [this] name))

(defn- jlrf->Field [^java.lang.reflect.Field fld]
  (let [mod (.getModifiers fld)]
  (Field.
    (Modifier/isPublic mod)
    (Modifier/isStatic mod)
    (Modifier/isFinal mod)
    (.getType fld)
    (.getName fld)
    fld)))

(defn- constant-value [^java.lang.Class cls ^Field f]
  (.get ^java.lang.reflect.Field (:java-field f) cls))

(defn- psf [^Field fld]
  (and (:public fld) (:static fld) (:final fld)))

(defn fields [^java.lang.Class cls]
  (map jlrf->Field (.getFields cls)))

(defn constants [^java.lang.Class cls]
  (let [flds (filter psf (fields cls))]
    (zipmap flds (map #(constant-value cls %) flds))))
