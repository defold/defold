(ns util.id-vec)

(defrecord IdVector [next-id v])

(defn- next! [iv]
  (swap! (:next-id iv) inc))

(defn iv-conj [iv e]
  (update iv :v conj [(next! iv) e]))

(defn iv-into [iv c]
  (reduce (fn [iv e]
            (iv-conj iv e))
          iv c))

(defn iv-vec [c]
  (-> (IdVector. (atom 0) [])
    (iv-into c)))

(defn iv-ids [iv]
  (mapv first (:v iv)))

(defn iv-vals [iv]
  (mapv second (:v iv)))

(defn iv-mapv [f iv]
  (mapv f (:v iv)))

(defn iv-filter-ids [iv ids]
  (if ids
    (let [ids (set ids)]
      (update iv :v (fn [v] (->> v
                              (filter (comp ids first))
                              vec))))
    iv))

(defn iv-remove-ids [iv ids]
  (let [ids (set ids)]
    (update iv :v (fn [v] (->> v
                            (filter (comp not ids first))
                            vec)))))
