(defproject defold-robot "0.7.0"
  :dependencies [[org.clojure/clojure "1.10.0"]
                 [org.clojure/tools.cli "0.3.5"]
                 [commons-io/commons-io "2.4"]
                 [org.apache.ant/ant "1.10.1"]
                 [org.openjfx/javafx-graphics "12"]
                 [org.openjfx/javafx-graphics "12" :classifier "linux"]
                 [org.openjfx/javafx-graphics "12" :classifier "mac"]
                 [org.openjfx/javafx-graphics "12" :classifier "win"]
                 [org.openjfx/javafx-swing "12"]
                 [org.openjfx/javafx-swing "12" :classifier "linux"]
                 [org.openjfx/javafx-swing "12" :classifier "mac"]
                 [org.openjfx/javafx-swing "12" :classifier "win"]]
  :resource-paths ["resources"]
  :main defold-robot.robot
  :aot [defold-robot.robot]
  :aliases {"tar-install" ["do" "uberjar" ["run" "-m" "lein-plugins.tar-install" :project/name :project/version :project/target-path]]}
  :release-tasks [["do" "tar-install"]])
