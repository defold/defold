(ns internal.ui.menus)

(def ^:private menu-configs (atom {:SCENE-MENU ["Scene"]
                                   :EDIT-MENU  ["Edit"]}))

;; get-menu-config data structure:
;; '(label category-id)
(defn get-menu-config
  [menu-id]
  (get @menu-configs menu-id))

(defn get-menu-ids
  []
  (keys @menu-configs))

(defn get-category-ids
  [menu-ids]
  (prn "requesting " menu-ids)
  (map #((second (get @menu-configs %))) menu-ids))

(defn make-menu-item [{:keys [name id] :as command}]
  [name id])

(defn collect-category-items
  [category]
  [])

(defn collect-menu-items
  "given a menu-id, returns a vec of:\n
  '(label command-id & [mnemonic image-id disabled-image-id])"
  [menu-id]
  (if-let [menu-cfg (get @menu-configs menu-id)]
    (collect-category-items menu-cfg)
    []))
