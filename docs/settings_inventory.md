# Skyline Settings Inventory (pre-refactor)

Reference snapshot of the settings system before the category-screens refactor.
All `key`s and defaults MUST remain unchanged after the refactor.

## Architecture (before)

- `SettingsActivity` (single activity, `R.id.settings` container, Material 3 theme `BaseAppTheme`)
  hosts exactly one `PreferenceFragmentCompat`:
  - `GlobalSettingsFragment` (no `AppItem` extra) — inflates `app_preferences.xml` +
    `emulation_preferences.xml` + `input_preferences.xml` + `credits_preferences.xml` into one flat list
  - `GameSettingsFragment` (per-game, `AppItemTag` extra) — inflates `custom_game_preferences.xml` +
    `emulation_preferences.xml` into one flat list (per-game prefs stored in a separate
    `SharedPreferences` file, `EmulationSettings.prefNameForTitle(titleId)`)
- Search: `SearchView` in the toolbar (`R.menu.settings_menu`) live-filters categories/preferences by
  title (comma-split query terms). In release builds `category_debug`, `category_credits`,
  `category_licenses` are hidden from search results (`hiddenCategoriesFromSearch`).
- Dialogs: `SettingsActivity.onPreferenceDisplayDialog` renders Material dialogs for
  `IntegerListPreference`, `EditTextPreference`, `ListPreference`.
- Storage: `PreferenceManager.getDefaultSharedPreferences` (global) / per-game files. Keys are read in
  Kotlin via `emu.skyline.utils.sharedPreferences` delegate (property name → camelToSnakeCase) and
  forwarded to C++ via `NativeSettings` (JNI). `prod_keys`/`title_keys` are read by `KeyReader`.

## Global preferences (single list today)

### Category `category_content` ("Content")
| key | type | default | read by | notes |
|---|---|---|---|---|
| document_provider | DocumentsProviderPreference | — | (action) opens internal directory via DocumentsProvider | |
| search_location | FolderPickerPreference | "" | AppSettings.searchLocation | SAF folder picker |
| prod_keys | KeyPickerPreference | — | KeyReader.KeyType.Prod | SAF file picker, simple summary |
| title_keys | KeyPickerPreference | — | KeyReader.KeyType.Title | SAF file picker, simple summary |

### Category `category_appearance` ("Appearance")
| key | type | default | read by | notes |
|---|---|---|---|---|
| app_theme | ThemePreference | "2" | AppSettings.appTheme | realtime theme switch |
| use_material_you | SwitchPreferenceCompat | false | AppSettings.useMaterialYou | listener: relaunches app |
| app_language | LanguagePreference | (system) | AppCompatDelegate.setApplicationLocales (persistent=false) | in-app language switch |
| layout_type | IntegerListPreference | 1 | AppSettings.layoutType | game display mode |
| sort_apps_by | IntegerListPreference | 0 | AppSettings.sortAppsBy | refreshRequired=true |
| group_by_format | RefreshSwitchPreferenceCompat | true | AppSettings.groupByFormat | refreshRequired |
| select_action | SwitchPreferenceCompat | false | AppSettings.selectAction | |

### Category `category_system` ("System")
| key | type | default | read by | notes |
|---|---|---|---|---|
| is_docked | SwitchPreferenceCompat | true | EmulationSettings.isDocked → NativeSettings | |
| username_value | CustomEditTextPreference | @string/username_default | EmulationSettings.usernameValue | limit 31 chars |
| profile_picture_value | ProfilePicturePreference | "" | EmulationSettings.profilePictureValue | image picker |
| system_language | IntegerListPreference | 1 | EmulationSettings.systemLanguage | refreshRequired |
| system_region | IntegerListPreference | -1 | EmulationSettings.systemRegion | |

### Category `category_presentation` ("Display")
| key | type | default | read by | notes |
|---|---|---|---|---|
| perf_stats | SwitchPreferenceCompat | false | EmulationSettings.perfStats | |
| max_refresh_rate | SwitchPreferenceCompat | false | EmulationSettings.maxRefreshRate | |
| orientation | IntegerListPreference | 6 | EmulationSettings.orientation | |
| aspect_ratio | IntegerListPreference | 0 | EmulationSettings.aspectRatio | |
| respect_display_cutout | SwitchPreferenceCompat | false | EmulationSettings.respectDisplayCutout | |

### Category `category_audio` ("Audio")
| key | type | default | read by | notes |
|---|---|---|---|---|
| is_audio_output_disabled | SwitchPreferenceCompat | false | EmulationSettings.isAudioOutputDisabled | |

### Category `category_gpu` ("GPU")
| key | type | default | read by | notes |
|---|---|---|---|---|
| gpu_driver | GpuDriverPreference | "system" | EmulationSettings.gpuDriver → NativeSettings | launches GpuDriverActivity |
| force_triple_buffering | SwitchPreferenceCompat | true | EmulationSettings.forceTripleBuffering | |
| disable_frame_throttling | SwitchPreferenceCompat | false | EmulationSettings.disableFrameThrottling | **dependency: force_triple_buffering**; listener unchecks it when triple buffering off |
| executor_slot_count_scale | SeekBarPreference | 4 (1..6) | EmulationSettings.executorSlotCountScale | NOTE: Kotlin default is 6, XML default 4 — XML wins for fresh installs (kept as-is) |
| executor_flush_threshold | SeekBarPreference | 256 (0..1024) | EmulationSettings.executorFlushThreshold | |
| use_direct_memory_import | SwitchPreferenceCompat | false | EmulationSettings.useDirectMemoryImport | |
| force_max_gpu_clocks | SwitchPreferenceCompat | false | EmulationSettings.forceMaxGpuClocks | disabled+unchecked with summary change when device unsupported |
| free_guest_texture_memory | SwitchPreferenceCompat | false (XML) / true (Kotlin) | EmulationSettings.freeGuestTextureMemory | kept as-is |
| disable_shader_cache | SwitchPreferenceCompat | false | EmulationSettings.disableShaderCache | summary inverted semantics (checked = disabled) |

### Category `category_hacks` ("Hacks")
| key | type | default | read by | notes |
|---|---|---|---|---|
| enable_fast_gpu_readback_hack | SwitchPreferenceCompat | false | EmulationSettings.enableFastGpuReadbackHack | |
| enable_fast_readback_writes | SwitchPreferenceCompat | false | EmulationSettings.enableFastReadbackWrites | **dependency: enable_fast_gpu_readback_hack** |
| disable_subgroup_shuffle | SwitchPreferenceCompat | false | EmulationSettings.disableSubgroupShuffle | |

### Category `category_debug` ("Debug")
| key | type | default | read by | notes |
|---|---|---|---|---|
| log_level | LogLevelPreference | 2 | AppSettings.logLevel | |
| validation_layer | SwitchPreferenceCompat | false | EmulationSettings.validationLayer | visible only in debug builds (`isPreferenceVisible=false` in XML) |

### Category `category_input` ("Input")
| key | type | default | read by | notes |
|---|---|---|---|---|
| ControllerPreference index 0..7 | ControllerPreference | — | InputManager (per-controller profiles) | 8 items, `initialExpandedChildrenCount=4` |

### Category `category_credits` ("Credits") + `category_licenses` ("Licenses")
- credits: dynamically added (shuffled) `Preference` rows from `R.array.credits_entries`
- licenses: static `LicensePreference` rows (Skyline, Ryujinx, shader-compiler, sirit, vkhpp, vkma,
  Vulkan validation layers, Oboe, …) opening `LicenseDialog`
- both hidden from search results in all builds today

## Per-game preferences (GameSettingsFragment — single list)

### Category `category_game` ("Game")
| key | type | default | notes |
|---|---|---|---|
| use_custom_settings | SwitchPreferenceCompat | false | master switch; all emulation categories below get `dependency = use_custom_settings` in code |
| reset_custom_settings | ResetSettingsPreference | — | resets per-game prefs |
| copy_global_settings | CopyGlobalSettingsPreference | — | copies global values |

- Plus all `emulation_preferences.xml` categories (system, presentation, audio, gpu, hacks, debug)
  with the dependency above; `profile_picture_value` and `log_level` are REMOVED per-game;
  `category_debug` hidden in release builds; `gpu_driver` receives the `AppItem`.

## Cross-cutting rules honored by the refactor

1. Every `key`, type and XML default stays identical.
2. `force_triple_buffering` ↔ `disable_frame_throttling` stay in the same screen (GPU).
3. `enable_fast_gpu_readback_hack` ↔ `enable_fast_readback_writes` stay in the same screen (Hacks).
4. Per-game keeps `use_custom_settings` and all dependent categories in ONE flat screen
   (splitting would break the code-assigned dependency across screens).
5. Search behavior preserved: comma-split live filtering; debug/credits/licenses hidden from search
   in release builds (credits/licenses also hidden in debug builds).
6. Material You relaunch listener, realtime theme/language, SAF pickers and GpuDriverActivity
   navigation untouched.

## Refactored layout (after)

Home = category menu (`SettingsCategoriesFragment`, `res/xml/settings_categories.xml`),
each entry opens a dedicated `GlobalCategoryFragment` subscreen:

| Subscreen | XML | Contains (categories kept) |
|---|---|---|
| Content | prefs_content.xml | category_content |
| Appearance | prefs_appearance.xml | category_appearance |
| System | prefs_system.xml | category_system + category_audio (merged: audio had a single item) |
| Display | prefs_presentation.xml | category_presentation |
| GPU | prefs_gpu.xml | category_gpu |
| Hacks | prefs_hacks.xml | category_hacks |
| Debug | prefs_debug.xml | category_debug |
| Input | prefs_input.xml | category_input |
| About | prefs_about.xml | category_credits + category_licenses |

Search across all categories is served by `SearchableSettingsFragment` (aggregates all XMLs, same
filter logic); tapping a result opens the owning subscreen with the preference scrolled into view.
