(ns editor.code)

(defn match-while [body p]
  (loop [body body
         length 0]
    (if-let [c (first body)]
      (if (p c)
        (recur (rest body) (inc length))
        {:body body :length length})
      {:body body :length length})))

(defn match-while-eq [body ch]
  (match-while body (fn [c] (= c ch))))

(defn match-until-string [body s]
  (if-let [s (seq s)]
    (let [slen (count s)]
      (when-let [index (ffirst (->> (partition slen 1 body)
                                    (map-indexed vector)
                                    (filter #(= (second %) s))))]
        (let [length (+ index slen)]
          {:body (nthrest body length) :length length})))
    {:body body :length 0}))

(defn match-until-eol [body]
  (match-while body (fn [ch] (and ch (not (#{\newline \return} ch))))))

(defn match-string [body s]
  (if-let [s (seq s)]
    (let [length (count s)]
      (when (= s (take length body))
        {:body (nthrest body length) :length length}))
    {:body body :length 0}))

(defn combine-matches [& matches]
  {:body (last matches) :length (apply + (map :length matches))})
