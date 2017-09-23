(ns editor.vcs-status)

(def ^:const change-type-icons
  {:add     "icons/32/Icons_M_07_plus.png"
   :modify  "icons/32/Icons_M_10_modified.png"
   :delete  "icons/32/Icons_M_11_minus.png"
   :rename  "icons/32/Icons_S_08_arrow-d-right.png"
   :unsaved "icons/32/Icons_M_10_modified.png"})

(def ^:const change-type-styles
  {:add     #{"added-file"}
   :modify  #{"modified-file"}
   :delete  #{"deleted-file"}
   :rename  #{"renamed-file"}
   :unsaved #{"unsaved-file"}})

(defn path [change]
  (or (:new-path change)
      (:old-path change)))

(defn- text [change]
  (path change))

(defn- verbose-text [change]
  (if (= :rename (:change-type change))
    (format "%s \u2192 %s" ; "->" (RIGHTWARDS ARROW)
            (:old-path change)
            (:new-path change))
    (text change)))

(defn render [change]
  {:text (text change)
   :icon (get change-type-icons (:change-type change))
   :style (get change-type-styles (:change-type change) #{})})

(defn render-verbose [change]
  {:text (verbose-text change)
   :icon (get change-type-icons (:change-type change))
   :style (get change-type-styles (:change-type change) #{})})
