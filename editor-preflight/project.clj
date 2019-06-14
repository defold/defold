(defproject editor-preflight "1.0.0"
  :dependencies [[org.clojure/clojure "1.9.0"] ;; 1.9 works with native image. 1.10 has trouble with locking macro.
                 [clj-kondo "2019.06.23-alpha" :exclusions [org.clojure/clojure]]
                 [jonase/kibit "0.1.6" :exclusions [org.clojure/clojure]]
                 [cljfmt-mg "0.6.4" :exclusions [org.clojure/clojure]]]
  :main ^:skip-aot editor-preflight.core
  :jvm-opts ["-Dclojure.compiler.direct-linking=true"]
  :target-path "target/%s"
  :profiles {:uberjar {:aot :all
                       :jvm-opts ["-Dclojure.spec.skip-macros=true"
                                  "-Dclojure.compiler.direct-linking=true"]}})

