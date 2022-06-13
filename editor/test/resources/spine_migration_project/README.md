# Testing the Spine migration

This project is a project that needs migrating from our old Spine to the new version.
Assets for the new spine scene is included in "assets/spineboy/" (old assets are still in "assets/")

To be able to build this project, we need to migrate it by re-setting some properties of objects


# Main collection

    * Set the "Spine Scene" property to "assets/spineboy/spineboy.spinescene"

    * Make sure the spine model material is set to "/defold-spine/assets/spine.material"



# Gui

    * Add a new spine scene to "Spine Scenes":
        * Select "assets/spineboy/spineboy.spinescene"

    * In the existing gui spine node ("Nodes -> spine")
        * Choose this spine scene "spineboy"
        * Select the "walk animation"


Save project.
You should now be able to "Build and Run" the project.


Expected gui diff:

    $ diff testdiff/main/main.gui spineupgradeexample/main/main.gui
    39c39
    <   type: TYPE_SPINE
    ---
    >   type: TYPE_CUSTOM
    56a57
    >   custom_type: 405028931
    61c62
    < spine_scenes {
    ---
    > resources {
    63c64
    <   spine_scene: "/assets/spineboy.spinescene"
    ---
    >   path: "/assets/spineboy/spineboy.spinescene"


Expected collection diff:

    $ diff testdiff/main/main.collection spineupgradeexample/main/main.collection
    23c23
    <   "  data: \"spine_scene: \\\"/assets/spineboy.spinescene\\\"\\n"
    ---
    >   "  data: \"spine_scene: \\\"/assets/spineboy/spineboy.spinescene\\\"\\n"
    27c27
    <   "material: \\\"/builtins/materials/spine.material\\\"\\n"
    ---
    >   "material: \\\"/defold-spine/assets/spine.material\\\"\\n"

