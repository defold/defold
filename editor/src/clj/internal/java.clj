(ns internal.java
  (:require [camel-snake-kebab :refer [->kebab-case]])
  (:import [java.lang.reflect Modifier]))

(defn invoke-no-arg-class-method
  [^Class class method]
  (-> class (.getDeclaredMethod method (into-array Class []))
      (.invoke nil (into-array Object []))))

(defn invoke-class-method
  [^Class class ^String method-name & args]
  (clojure.lang.Reflector/invokeStaticMethod class method-name ^objects (into-array Object args)))

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

(defn instance-fields [^java.lang.Class cls]
  (filter #(not (:static %)) (fields cls)))

(defn- field-as-keyword [^Field f]     (keyword (->kebab-case (.name f))))
(defn- field-getter     [i ^Field f]   (list (symbol (str ".-" (.name f) )) i))
(defn- field-set-sexp   [i ^Field f v] (list `set! (list `. i (symbol (.name f))) v))

(defmacro bean-mapper [cls]
  (let [tagged-arg (with-meta (gensym "o") {:tag cls})
        flds (fields (resolve cls))]
    `(fn [~tagged-arg]
       (hash-map ~@(mapcat identity
                           (for [f flds]
                             [(field-as-keyword f)
                              (field-getter tagged-arg f)]))))))

(defmacro map-beaner [cls]
  (let [inst      (with-meta (gensym "o") {:tag cls})
        flds      (instance-fields (resolve cls))
        set-sexps (for [f flds
                        :let [kw (field-as-keyword f)]]
                    (list `when (list `contains? 'm kw)
                      (field-set-sexp 'inst f `(get ~'m ~kw))))]
    `(fn [~'m]
       (let [~'inst (new ~cls)]
         ~@set-sexps
         ~'inst))))
