(ns internal.either)

(declare ->Right ->Left)

(defmacro bind [expr]
  `(try
     (->Right ~expr)
     (catch Throwable e#
       (->Left e#))))

(defprotocol Either
  (result  [this])
  (or-else [this f]))

(defrecord Right [value]
  Either
  (result  [_] value)
  (or-else [this _] this))

(defrecord Left [exception]
  Either
  (result  [_] (throw exception))
  (or-else [_ f] (bind (f exception))))
