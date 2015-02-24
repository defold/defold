(ns editor.jfx
  (:import  [java.io File]
            [java.util ArrayList]
            [javafx.stage FileChooser FileChooser$ExtensionFilter]))

(defn choose-file [title init-dir init-file ext-descr exts]
  (let [chooser (FileChooser.)
        init-dir (File. init-dir)
        init-dir (if (.exists init-dir) init-dir (File. (System/getProperty "user.home")))]
    (.setTitle chooser title)
    (.setInitialDirectory chooser init-dir)
    (.setInitialFileName chooser init-file)
    (.add (.getExtensionFilters chooser) (FileChooser$ExtensionFilter. ext-descr (java.util.ArrayList. exts)))
    (let [file (.showOpenDialog chooser nil)]
      (if file (.getAbsolutePath file)))
    ))