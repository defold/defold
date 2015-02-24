(ns internal.either)

(declare ->Right ->Left)

(defmacro bind [expr]
  `(try
     (->Right ~expr)
     (catch Throwable e#
       (->Left e#))))

(defprotocol Either
  (result  [this])
  (or-else [this f])
  (exists? [this]))

(deftype Right [value]
  Either
  (result  [_] value)
  (or-else [this _] this)
  (exists? [_] true))

(deftype Left [exception]
  Either
  (result  [_] (throw exception))
  (or-else [_ f] (bind (f exception)))
  (exists? [_] false))
