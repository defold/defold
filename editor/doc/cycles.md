Dependency cycles in the graph
==============================

We have two recurring problems caused by dependency cycles in the
graph

* An assertion when building, caused by `:build-targets` cycles from
  * A `.go` game object with a factory component transitively
    referring to the same `.go`
  * A `.go` with a collection factory component transitively referring
    to the same `.go`
  * A `.lua` module transitively `require`'ing the same `.lua` module
    (... and this module is transitively `require`'d from a `.script`,
    `.render_script` or `.gui_script`)
* The editor hangs when
  * A `.collection` with a sub collection transitively refers to the
    same `.collection`
  * A `.gui` with a template node transitively refers to the same
    `.gui`
  
## Assertion

The assert comes from our "in production" check in
`internal.node/mark-in-production`. One way to fix this error is to
not consider it an error or "exceptional" at all. We _do_ let the user
create dependency cycles, so it's not a programming error, but a data
validation issue, and as such we could present it as a normal build
error so the user can fix it. Easiest way to acheive this is to let
the result be a `g/error` decorated with the "production path" that
led in to the cycle. In the build errors view we can look up the
corresponding node types and outputs in production and recognise the
known causes listed above.

## Hang

This case has to do with overrides, and so is automatically more
complicated :)

We will discuss the `.gui` case.

Say in your `menu.gui` you add a template node referring to
`button.gui`. The idea is that you should be able to modify some
parameters of the nodes in `button.gui` to adapt it to your
`menu.gui`. For instance, you might want to change a `Text` node which
is the label of the `button.gui` to something descriptive of its
function. The modifications are stored within `menu.gui`.

This is implemented using Overrides. This is a way to, given a start
node and a traversal predicate, create a parallel set of nodes that
behaves just as the initial nodes _until_ you `set-property` or
`connect` new stuff to them. You can reset individual properties so
they flip back to returning the original value, and also reset the
overriding `connect` to make the original connections be in
effect. The magic sauce in this is that the parallel set of nodes -
override nodes - is automatically expanded/contracted when new nodes
are connected/disconnected to the original nodes, or original nodes
deleted. This is handled by the graph system as part of performing
transactions, using the traversal predicate to determine for what
nodes there should exist corresponding override nodes.

In our gui case, when we add `button.gui` to `menu.gui` (using a
template gui node), the editor creates an override (`g/override`) of
the entire `button.gui`. This creates override nodes for every node in
`button.gui`. The override node of the `button.gui` scene node is then
connected into the node hierarchy of `menu.gui`. The nodes we see in
the outline of `menu.gui` are in fact override nodes of the stuff in
the actual `button.gui`. The nice thing here is that if you go to
`button.gui` and add f.i. a ParticleFX node, the override system will
automatically create a corresponding override node that pops up when
you look at it from `menu.gui`. Any changes you do to the button nodes
from `menu.gui` will affect the override nodes only.

Gui's can be nested arbitrarily deep through template nodes. You can
add the `menu.gui` to your `screen.gui`, which will create another
parallel layer of override nodes on top of the nodes in
`menu.gui`. Some of these will already be override nodes, but that's
fine - the mechanism of setting / resetting properties and connections
will work the same as if the original nodes were normal nodes.

The order you nest these gui's also does not matter. You could at this
point add a contrieved `tooltip.gui` to the `button.gui` and new
layers of override nodes would be created for `button.gui`, `menu.gui`
and `screen.gui`.

Now the plot twist!

Say we have only our `button.gui` with a Box node and a Text node and
we want to add our `tooltip.gui`. We add a Template node. Then we
accidentally select `button.gui` for the template property. What will happen?

Using `g/override` we create an Override starting at the `button.gui`
scene node and a traversal predicate stating roughly "make override
nodes for every gui node below". At this point, the template property
isn't really set - it's being set.

This will initially create override nodes for the `button.gui` scene
node, the Box, the Text, and the "still unset" Template node. All is
fine.

Then, we do a fatal `connect` - we connect the structure of the
overridden `button.gui` under the `button.gui` Template node. This
connect will invoke the override machinery. That will realise that
since something was connected into the Template node, and the Template
node is part of an Override, we must do a new traversal and see if we
should create new override nodes for the newly connected nodes.

The traversal will step in to the overriden `button.gui` scene node,
in to the Template node, where it will find a gui scene node
connected. Then, step in to the `button.gui` node again, and again in
to the Template node, where it will find a gui scene node
connected. Then, step in to the `button.gui` node yet again, and yet
again in to the Template node... Along with the traversal we also
create new layers of override nodes so eventually we run out of
memory.

Some ascii art to illustrate!

**Note that this is slightly simplified: we skip some intermediate
nodes, and the override traversal requires :cascade-delete connections
Right to Left in the diagrams below**

    BSN = `button.gui` scene node
    TN  = Template node where we accidentally connect `button.gui`
     -  = Is connected to 
     
    A'
    |   = A' is an override node of A
    v
    A


We start out with:

    BSN - TN

We set `button.gui` as Template on the Template node and start by
creating override nodes for BSN and TN.


    BSN - TN          BSN' -  TN'
                       |       |
                       v       v
                      BSN  -  TN
                  
Now we connect the BSN' to TN

    BSN - TN - BSN' -  TN'
                |       |
                v       v
               BSN  -  TN - ...
           
But, BSN' is also connected to the the bottom right TN

    BSN - TN - BSN' - TN' - ...
                |      |
                v      v
               BSN  - TN  - BSN' - TN' - ...
                             |      |
                             v      v
                            BSN  - TN  - BSN' - TN' - ...
                        
Since BSN' is connected to TN, and TN' is an override node of TN, we
need an override node of BSN' for TN'

    BSN - TN - BSN' - TN' - BSN'' - ...
                |      |     |
                v      v     v
               BSN  - TN  - BSN'  - TN' - BSN'' - ...
                             |       |     |
                             v       v     v
                            BSN   - TN  - BSN'  - TN' - BSN'' - ...
                                          |        |     |
                                          v        v     v
                                         BSN    - TN  - BSN'  - TN' - BSN'' - ...
                                                         |       |     |
                                                         v       v     v
                                                        BSN   - TN  - BSN'  -
                                                  
Since TN' is connected to BSN', and BSN'' is an override of BSN, we
need an override of TN' for BSN''

    BSN - TN - BSN' - TN' - BSN'' - TN'' - ...
                |      |     |       |
                v      v     v       v
               BSN  - TN  - BSN'  - TN'  - BSN'' - TN'' - ...
                             |       |      |       |
                             v       v      v       v
                            BSN   - TN   - BSN'  - TN'  - BSN'' - TN'' - ...
                                            |       |      |       |
                                            v       v      v       v
                                           BSN   - TN   - BSN'  - TN'  - BSN'' - TN'' - ...
                                                           |       |      |       |
                                                           v       v      v       v
                                                          BSN   - TN   - BSN'  - TN'  -
                                                                          |       |
                                                                          v       v
                                                                         BSN   - TN   -

... And so on.

### Ideas

#### Prevent it from happening

We can figure out the dependencies of gui scenes and collections and
prevent the user from choosing something that would lead to the error.
One complication is that we also have to guard for bad data when
loading. A merge conflict or sloppy manual editing could also lead to
a cycle.

#### Accept user/data errors

An alternative is to introduce some sort of error state to the
overrides. For instance, during the override traversal above, we could
keep track of already seen / overridden nodes, and if we detect a
cycle somehow signal an error. Perhaps it should be possible to funnel
this error into the graph. For instance, to `connect` an `:error`
output on the Override to some `:error` input on the Template node and
then we can show this in the outline etc. That might require the
Override to be node:ish.

One question is how do we "unsignal" the error - when is the Override
no longer broken? How do we detect that?

Consider if we had a cycle `screen.gui` -> `menu.gui` -> `button.gui`
-> `screen.gui` (we intended `tooltip.gui`). The user might want to
resolve it by picking `tooltip.gui` or, perhaps break the cycle
between `screen.gui` and `menu.gui`.

I think we would like to create override nodes "as far as possible"
and stop just where we detect the cycle, and that's where should point
the error. This to give the user more flexibility in how to fix it.
