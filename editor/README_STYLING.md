# Editor styling

The editor uses JavaFX stylesheets to control look and feel. A single stylesheet is set on the root node (by convention) in the scene. The stylesheet `editor.css` is loaded as a regular java resource, from the uberjar or from the file-system in dev-mode. If an `editor.css` is found in the current working directory that file will take precedence over the aforementioned java resource.

Edit the SCSS files in `styling/stylesheets/`, not the generated `resources/editor.css` file.

The CSS is divided into multiple files and grouped into `base`, `mixins`, `components` and `modules`.

* Base are things like palette and typography.
* Mixins contain SASS mixins.
* Components are JavaFX componenents that are reused on several places within the Editor
* Modules are specific parts of the editor that need to override default component behaviour

Note: The best way to understand how JavaFX styling works is by studying the default stylesheet `modena.css` included in `jfxrt.jar`

## Generating the stylesheet

The `editor.css` stylesheet is generated from the Sass/SCSS files in `styling/stylesheets/`. To generate it, use Leiningen or Gulp:

### Using Leiningen

Run these commands from `editor/`. To generate the stylesheet once:

```sh
lein sass once
```

To watch SCSS files and regenerate CSS automatically after each change, keep this command running:

```sh
lein sass auto
```

Reload the stylesheet in the running editor after each change using **Help → Reload Stylesheet** or `F5`.

### Using Node.js

In the `styling` directory:

```sh
npm install
```

Gulp is used in combination with SASS to compile the CSS file from many smaller files. To generate once:

```sh
gulp
```

Watch and re-generate css on changes:

```sh
gulp watch
```
