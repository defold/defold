Overrides
=========

For the most recent writeup/braindump on overrides, see
`internal.graph`.

The original design document for "Graph Instancing" is here:
https://docs.google.com/document/d/1jBk7FKT1nu27FON9-z-Zb4A9j2qqOo3FhHlmPLiRlJ0/edit#
And some background / motivating problems here:
https://docs.google.com/document/d/1tjavS1oEaz8p3qcfGOjRJC7_n-JAspCQ-pvy8bmIT7I/edit#heading=h.sbddrngmkrrp

Note that some concepts have changed or been removed since these
documents were written, so read with a grain of salt.

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
is almost consistently an extra problem to consider whenever we design
a new feature.

For what it's worth, it has made script property overrides possible in
the collection / game object world (the only thing we actually
override there), and overriding gui node attributes in the the nested
guis case using `TemplateNode`s.

It has some nice properties.

* It does not require unreasonable changes to the node types involved.
* Creating overrides does not affect the behavior of the original
nodes.
* Adding another layer of overrides vertically or horizontally "just
works". Vertically: Say you have a `tooltip` gui added to a
`button`. We then have override nodes of the `tooltip` nodes in the
`button`. Adding the `button` to a `panel` would create override nodes
from the `button` nodes, and override nodes of the already once
overridden `tooltip` nodes. The overridden values take precendence in
expected order. Horizontally: You can add the `button` to several
guis, all overrides are independent.

### What problems doesn't it solve?

The overridde values (values of overridden properties), typically
belong to some contextual node. In the case of collections, overrides
for sub collection nodes should be saved with the including collection
collection. Similarly for gui, where the values are stored in the
scene containing the template node. This requires explicit graph
connections to pass the data up to the right node.

Overrides require special care during resource sync. We must
transplant any overrides to newly loaded versions of resource
nodes. Some values are still lost when reloading from disk. We only do
the transplant on resource nodes. If any internal/implementation nodes
owned by the resource nodes are overridden, those values will be lost.

The most urgent problem we can't easily solve with overrides in it's
current form is the famed gui/templates/layouts problem.


### Gui:s, templates and layouts

A gui is represented in the editor using a hierarchy of nodes like so:

GuiSceneNode        (represents the top level .gui file)
     |      \
  NodeTree   LayoutsNode
     |              \
   GuiNode<--      LayoutNode
     |      |         |
     |______|      (will be discussed later)

GuiNode is an inherited node type - the actual user visible types are
BoxNode, PieNode, TextNode, SpineNode, ParticleFXNode, and most
importantly TemplateNode. These can be arbitrarily nested _except_ a
TemplateNode whose children come from a different gui scene. No extra
nodes can be added as children to a TemplateNode (so the graph above
is slightly lying).

#### Templates

The idea with templates is that you can add any number of instances of
a template gui scene - typically some gui component like a button - to
another gui. For each instance you can override properties of the
nodes in the templated scene, to make it fit the use. (Have a look at
the manual https://www.defold.com/manuals/gui-template/)

This is of course implemented using overrides. When you specify a gui
scene to be used as template, we do `g/override` starting at the
specified scene and with a traversal predicate that will follow any
GuiSceneNode, NodeTree, GuiNode, LayoutsNode or LayoutNode. The
overriden GuiSceneNode's :scene and :node-outline outputs and outputs
for saving and building etc are then connected to the
TemplateNode. For the user, the specified scene seems to end up under
the TemplateNode.  If you pick-select a node from the templated scene
(or select via the outline), you will be selecting an override node,
and any changes you do will affect that override node, not the
original. (**Side note: I think you can currently change some resource
properties that should be read only when on an override node**).

#### Layouts

Layouts are used to automatically adapt the gui to changes in
orientation, as specified in the `.display_profiles` selected in
`game.project`. (See
https://www.defold.com/manuals/gui-layouts/). When editing a gui you
are effectively editing how it will look when used in a particular
layout. There is always a default layout, even if the display profiles
is empty. There is currently no constraint that the layout has to be
in the display profiles - for instance if you add a `.gui` originally
edited with a different more elaborate display profiles.

Layouts work much like templates in that we use `g/override` and the
override machinery to implement them. Layouts are overrides of the
default layout, and the default layout is simply what is found under
the NodeTree. When we add a layout to a gui scene, we use `g/override`
on the scene's NodeTree, with a traversal predicate following every
GuiNode, NodeTree and GuiSceneNode. (Note this differs from the
template override predicate). We create a LayoutNode for the layout,
add it under the LayoutsNode and attach the overridden NodeTree's
:scene, :node-outline and outputs for save and build etc to the
LayoutNode.

Now, changing the currently selected layout in the scene editor must
show the scene with layout adjustments/overriden values applied, and
picking must select the appropriate override node. Likewise, when we
change layout, the outline must be updated so selection selects the
override node. We'd effectively like the gui scene's :scene and
:node-outline to be produced by either the original NodeTree
immediately under the GuiSceneNode (default layout), or by an
overridden NodeTree under a LayoutNode (for a specific layout). This
is controlled by a property `visible-layout` on the top level gui
scene node. This value is then piped through the entire gui node
hierarchy as `current-layout` and all GuiSceneNode:s below use it to
pick what NodeTree's outputs are used. (**I think putting this view
state on a project node is ugly. It also still makes the graph produce
all varieties of node outlines and scenes, while only the selected
layout's is used**)


#### The problem

Considered separately templates and layouts look like they can be
solved with overrides. The complexity comes when we combine them:

The "layers" of override nodes created for layouts does not really
match how the layout adjustments should be applied, nor does the
layering automatically readjust when layers are added or removed from
templated gui scenes.

The template nodes overrides the whole node tree, and any layout node
trees, in the templated scene. The override values 

The layouts only override the node tree branch - skipping any layouts
in the templated scenes. Then, by impressive ingenuity, layout
override values for stuff under the template nodes are stored in the
templatenode's override of the templated gui scene's layoutnode's
nodetree. For stuff "above" the templatenode the layout overrides are
stored in the layoutnode's override nodes of the current gui scenes node tree... hhnggg 




* What does it mean for a layout to be specified in the templated
scene but not the templating scene?






* OverrideNode
* arcs
* originals
* node-value / produce-value
* api: g/override, overrides, override-original, override?, override-id, property-overridden?, property-value-origin? transfer-overrides
** Node: overridden-properties, property-overridden?
** OverrideNode: clear-property, override-id, original, set-original
** IBasis: overrde-node, override-node-clear (?), add-override, delete-override, replace-override


