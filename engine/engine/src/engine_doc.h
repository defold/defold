// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

/*# Engine runtime documentation
 *
 * @document
 * @name Engine runtime
 * @namespace engine
 * @language C++
 */

/*# override the save directory
 * Overrides the save directory when using `sys.get_save_file()`
 *
 * @macro
 * @name DM_SAVE_HOME
 */

/*# set the engine service port
 * Overrides the engine service port, when creating the internal http server (e.g web profiler)
 *
 * Valid values are integers between [0, 65535] and the string "dynamic" (which translates to 0).
 * Default value is 8001.
 *
 * @macro
 * @name DM_SERVICE_PORT
 */

/*# enables quit on escape key
 * Enables quitting the app directly by pressing the `ESCAPE` key.
 * Set to "1" to enable this feature.
 *
 * @macro
 * @name DM_QUIT_ON_ESC
 */

/*# sets the logging port
 * Enables receiving the log on a designated port (e.g. using telnet)
 * Valid values are integers between [0, 65535]
 *
 * @macro
 * @name DM_LOG_PORT
 */

/*# disables OpenGL error checking
 * Disables OpenGL error checking.
 * This is especially beneficial for running debug builds in the Chrome browser,
 * in which the calls to `glGetError()` is notoriously slow.
 *
 * Default value is "true" for debug builds, and "false" for release builds.
 *
 * @macro
 * @name --verify-graphics-calls=
 * @examples
 * ```bash
 * $ ./dmengine --verify-graphics-calls=false
 * ```
 */

/*# launch with a specific project
 * Launch the engine with a specific project file
 *
 * If no project is specified, it will default to "./game.projectc", "build/default/game.projectc"
 *
 * @note It has to be the first argument
 *
 * @macro
 * @name launch_project
 * @examples
 * ```bash
 * $ ./dmengine some/folder/game.projectc
 * ```
 */

/*# override game property
 * Override game properties with the format `--config=section.key=value`
 *
 * @macro
 * @name --config=
 * @examples
 * ```bash
 * $ ./dmengine --config=project.mode=TEST --config=project.server=http://testserver.com
 * ```
 */
