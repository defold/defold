(ns editor.system)

(defonce mac? (-> (System/getProperty "os.name")
                (.toLowerCase)
                (.indexOf "mac")
                (>= 0)))
