/*
 * Copyright (C) 2014-2025 Muhammad Tayyab Akram
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _SB_PUBLIC_CONFIG_H
#define _SB_PUBLIC_CONFIG_H

/* #define SB_CONFIG_DLL_EXPORT */
/* #define SB_CONFIG_DLL_IMPORT */
/* #define SB_CONFIG_LOG */
/* #define SB_CONFIG_UNITY */

#ifdef SB_CONFIG_UNITY
#define SB_INTERNAL static
#else
#define SB_INTERNAL
#endif

#endif
