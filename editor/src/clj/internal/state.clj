(ns internal.state)

;; Only marked dynamic so tests can rebind. Should never be rebound "for real".
(defonce ^:dynamic *the-system* (atom nil))
