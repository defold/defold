# Editor styling
The editor uses JavaFX stylesheets to control look and feel. A single stylesheet is set on the root node (by convention) in the scene. The stylesheet `editor.css` is loaded as a regular java resource, from the uberjar or from the file-system in dev-mode. If an `editor.css` is found in the current working directory that file will take precedence over the aforementioned java resource.

The stylesheet can be reloaded with the function key `F5`.

The CSS is divided into multiple files and grouped into `base`, `mixins`, `components` and `modules`.

* Base are things like palette and typography.
* Mixins contain SASS mixins.
* Components are JavaFX componenents that are reused on several places within the Editor
* Modules are specific parts of the editor that need to override default component behaviour

Note: The best way to understand how JavaFX styling works is by studying the default stylesheet `modena.css` included in `jfxrt.jar`

## Generating the stylesheet
The `editor.css` stylesheet is generated from the the sass/scss files in `styling/stylesheets`. To generate the file you can use either leiningen or gulp:

### Using leiningen

Generate once:

```sh
lein sass once
```

Watch and re-generate css on changes:

```sh
lein sass watch
```

### Using nodej

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
