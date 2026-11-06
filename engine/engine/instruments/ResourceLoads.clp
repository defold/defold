; LogLoadProfile emits one formatted text argument. Parse after the LAST
; separator so arbitrary timing delimiters in the resource path remain intact.
(deffunction RECORDER::resource-field (?timing ?prefix ?suffix)
    (bind ?start 1)
    (if (neq ?prefix "") then
        (bind ?start (+ (str-index ?prefix ?timing) (str-length ?prefix))))
    (bind ?tail (sub-string ?start (str-length ?timing) ?timing))
    (return (sub-string 1 (- (str-index ?suffix ?tail) 1) ?tail))
)

(deffunction RECORDER::resource-duration (?timing ?prefix ?suffix)
    (return (integer (+ 0.5 (* 1000000 (string-to-field (resource-field ?timing ?prefix ?suffix))))))
)

(defrule RECORDER::record-resource-load
    (table-attribute (table-id ?output) (has schema defold-resource-load))
    (table (table-id ?output) (side append))
    (os-signpost
        (subsystem "com.defold.profiler")
        (category "PointsOfInterest")
        (name "Log")
        (event-type "Event")
        (time ?reported&~0)
        (process ?process)
        (message$ ?text&:(eq (str-index "INFO:RESOURCE: /" ?text) 1)
                        &:(str-index "ms (pre: " ?text)))
    =>
    (bind ?timing ?text)
    (while (str-index " took " ?timing) do
        (bind ?timing (sub-string (+ (str-index " took " ?timing) 6) (str-length ?timing) ?timing)))

    ; ProfileLogText can truncate a valid resource path's completion record.
    ; Check the complete suffix before appending a row with numeric columns.
    (bind ?remaining ?timing)
    (progn$ (?separator (create$ "ms (pre: " "ms, read: " "ms, create: "
                                "ms, post: " "ms [" " calls], queue: "
                                "ms, wait: " "ms, result: " ")"))
        (bind ?position (str-index ?separator ?remaining))
        (if (eq ?position FALSE) then (return))
        (bind ?remaining (sub-string (+ ?position (str-length ?separator)) (str-length ?remaining) ?remaining)))
    (if (neq (length$ (explode$ ?remaining)) 0) then (return))

    (create-new-row ?output)
    (set-column reported ?reported)
    (set-column resource (sub-string 16 (- (str-length ?text) (str-length ?timing) 6) ?text))
    (set-column total (resource-duration ?timing "" "ms (pre: "))
    (set-column preload (resource-duration ?timing "(pre: " "ms, read: "))
    (set-column read (resource-duration ?timing ", read: " "ms, create: "))
    (set-column create (resource-duration ?timing ", create: " "ms, post: "))
    (set-column post-create (resource-duration ?timing ", post: " "ms ["))
    (set-column post-create-calls (string-to-field (resource-field ?timing "ms [" " calls]")))
    (set-column queue (resource-duration ?timing ", queue: " "ms, wait: "))
    (set-column wait (resource-duration ?timing ", wait: " "ms, result: "))
    (set-column result (resource-field ?timing ", result: " ")"))
    (set-column process ?process)
)
