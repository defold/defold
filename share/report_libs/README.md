# inline_libraries.py
Generates inline HTML content tags from external JavaScript, CSS and image files.

To avoid having to copy third party JS/CSS libraries when generating build reports inside the Defold editor we instead inline the needed files so we can serve just one HTML file.

## Usage
Running the script will print the concatenation of all JS and CSS files (under `js`/ and `css/`) as two HTML tags (`<style>`/`<script>`). It also tries to inline images references by CSS `url()` entries by looking for the files under `images/` and replacing them with inline base64 `data:` URLs.

    $ ./inline_libraries.py

The output should be used when updating the libraries for `com.dynamo.cr/com.dynamo.cr.bob/lib/report_template.html`.