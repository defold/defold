(ns eclipse.reflect
  (:import [java.lang.reflect Modifier]))

(defrecord Field [public static final type name])

(defn- jlrf->Field [^java.lang.reflect.Field fld]
  (let [mod (.getModifiers fld)]
  (Field.
    (Modifier/isPublic mod)
    (Modifier/isStatic mod)
    (Modifier/isFinal mod)
    (.getType fld)
    (.getName fld))))

(defn- psf [^Field fld]
  (and (:public fld) (:static fld) (:final fld)))

(defn fields [^java.lang.Class cls]
  (map jlrf->Field (.getFields cls)))

(defn constants [^java.lang.Class cls]
  (filter psf (fields cls)))
