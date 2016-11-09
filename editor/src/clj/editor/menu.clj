(ns editor.menu)

(set! *warn-on-reflection* true)

;; *menus* is a map from id to a list of extensions, extension with location nil effectively root menu
(defonce ^:dynamic *menus* (atom {}))

(defn extend-menu [id location menu]
  (swap! *menus* update id (comp distinct concat) (list {:location location :menu menu})))

(defn- collect-menu-extensions []
  (->>
    (flatten (vals @*menus*))
    (filter :location)
    (reduce (fn [acc x] (update-in acc [(:location x)] concat (:menu x))) {})))

(defn- do-realize-menu [menu exts]
  (->> menu
     (mapv (fn [x] (if (:children x)
                     (update-in x [:children] do-realize-menu exts)
                     x)))
     (mapcat (fn [x]
               (if (and (contains? x :id) (contains? exts (:id x)))
                 (into [x] (do-realize-menu (get exts (:id x)) exts))
                 [x])))
     vec))

(defn realize-menu [id]
  (let [exts (collect-menu-extensions)
        menu (:menu (some (fn [x] (and (nil? (:location x)) x)) (get @*menus* id)))]
    (do-realize-menu menu exts)))

;; For testing only
(defmacro with-test-menus [& body]
  `(with-bindings {#'*menus* (atom {})}
     ~@body))
