Overrides
=========

For the most current writeup/braindump on overrides, see
`internal.graph`.

The original design document for "Graph Instancing" is here:
https://docs.google.com/document/d/1jBk7FKT1nu27FON9-z-Zb4A9j2qqOo3FhHlmPLiRlJ0/edit#
And some background / motivating problems here:
https://docs.google.com/document/d/1tjavS1oEaz8p3qcfGOjRJC7_n-JAspCQ-pvy8bmIT7I/edit#heading=h.sbddrngmkrrp

Note that some concepts have changed or been removed since these
documents were written, so keep a grain of salt nearby.

Codewise, the override nodes are implemented using
`internal.node/OverrideNode`s instead of `NodeImpl`. `OverrideNode`
keeps a map of overridden properties. Mainly for properties without a
`(set ...)` clause but also for keeping track of which properties have
been overridden. The `produce-value` implementation roughly runs the
behavior of the original node type passing itself, with special
treatment of the `_properties` and `_declared-properties` for flagging
properties as overridden etc.

The somewhat hairy semantics of connections / arcs is all in
`internal.graph`, see methods `arcs-by-source` and `arcs-by-target`
and associated helpers.

Keeping the node structure in sync with the originals is all part of
`internal.transaction`. Just follow the `connect`, `disconnect`,
`delete-node` transactions - and of course `override` and
`transfer-overrides`.

### What problems does it solve?

The override support clearly complicates core parts of the graph and
is almost consistently an extra thing to consider whenever we design a
new feature. For what it's worth, it has made script property
overrides possible in the collection / game object world (the only
thing we actually override there), and overriding gui node attributes
in the the nested guis case using `TemplateNode`s. A nice property is
that adding another layer of overrides vertically (wrapping a wrapped
gui in yet another gui), and horizontally (wrapping a gui in several
different gui's) - "just works".

### What problems doesn't it solve?

The overridde values (values of overridden properties), typically
logically belong to some contextual node. In the case of collections,
overrides for sub collection nodes are to be saved with the including
collection collection. Similarly for guis. This requires graph
connection plumbing to pass the data up to the right node.

Overrides require special care during resource sync: we must
transplant overrides to newly loaded versions of resource nodes.










The node types
involved in the overrides do need some changes in order to for
instance mark some properties as read only when not an original node

Most of the node
definitions in these cases don't need to change in order to be
overridden, which is a nice property.

















* OverrideNode
* arcs
* originals
* node-value / produce-value
* api: g/override, overrides, override-original, override?, override-id, property-overridden?, property-value-origin? transfer-overrides
** Node: overridden-properties, property-overridden?
** OverrideNode: clear-property, override-id, original, set-original
** IBasis: overrde-node, override-node-clear (?), add-override, delete-override, replace-override

* befintlig dokumentation i internal.graph/graph, google drive? 


