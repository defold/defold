(ns editor.system)

(set! *warn-on-reflection* true)

(defonce mac? (-> (System/getProperty "os.name")
                (.toLowerCase)
                (.indexOf "mac")
                (>= 0)))
