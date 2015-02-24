(ns service.registry
  (require [dynamo.types :as t]))

(defprotocol Registry
  (register [this ent])
  (swap [this ent f])
  (purge [this ent])
  (size [this]))

(deftype DisposableKeyedRegistry [store key-fn dispose-fn]
  Registry
  (register [this ent]
    (swap! store assoc (key-fn ent) ent))
  (swap [this ent f]
    (swap! store update-in (key-fn ent) f))
  (purge [this ent]
    (swap! store dissoc (key-fn ent)))
  (size [this]
    (count @store))
  clojure.lang.ILookup
  (valAt [this key]
    (get @store key))
  (valAt [this key not-found]
    (get @store key not-found))
  t/IDisposable
  (dispose [this]
    (doseq [e (vals @store)]
      (if (t/disposable? e) (dispose-fn e)))))

(defn registered
  [make-fn arg-key cache-key-fn dispose-fn]
  (let [cache (DisposableKeyedRegistry. (atom {}) cache-key-fn dispose-fn)]
    (fn [& args]
      (let [regkey (apply arg-key args)
            old (get cache regkey)]
        (if old old
          (let [new (apply make-fn args)]
            (register cache new)
            new))))))
