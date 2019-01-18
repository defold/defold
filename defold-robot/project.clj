(defproject defold-robot "0.3.0"
  :description "FIXME: write description"
  :url "https://www.defold.com"
  :license {:name "Eclipse Public License"
            :url "http://www.eclipse.org/legal/epl-v10.html"}
  :dependencies [[org.clojure/clojure "1.10.0"]
                 [org.clojure/tools.cli "0.3.5"]
                 [commons-io/commons-io "2.4"]
                 [org.apache.ant/ant "1.10.1"]
                 [org.openjfx/javafx-graphics "12-ea+6"]
                 [org.openjfx/javafx-swing "12-ea+6"]]
  :resource-paths ["resources"]
  :main defold-robot.robot
  :aot [defold-robot.robot]
  :aliases {"tar-install" ["do" "uberjar" ["run" "-m" "lein-plugins.tar-install" :project/name :project/version :project/target-path]]}
  :release-tasks [["do" "tar-install"]])
