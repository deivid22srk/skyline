/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.settings

import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.Preference
import androidx.preference.PreferenceCategory
import emu.skyline.BuildConfig
import emu.skyline.R
import emu.skyline.data.AppItem
import emu.skyline.data.AppItemTag
import emu.skyline.preference.GpuDriverPreference
import emu.skyline.utils.serializable

/**
 * This fragment is used to display custom game preferences
 */
class GameSettingsFragment : BaseCategoryFragment() {
    private val item by lazy { requireArguments().serializable<AppItem>(AppItemTag)!! }

    override fun onViewCreated(view : View, savedInstanceState : Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        (activity as AppCompatActivity).supportActionBar?.subtitle = item.title
    }

    /**
     * This constructs the preferences from XML preference resources
     */
    override fun onInflatePreferences() {
        preferenceManager.sharedPreferencesName = EmulationSettings.prefNameForTitle(item.titleId ?: item.key())
        addPreferencesFromResource(R.xml.custom_game_preferences)
        addPreferencesFromResource(R.xml.prefs_system)
        addPreferencesFromResource(R.xml.prefs_presentation)
        addPreferencesFromResource(R.xml.prefs_gpu)
        addPreferencesFromResource(R.xml.prefs_hacks)
        addPreferencesFromResource(R.xml.prefs_debug)
    }

    override fun onAfterPreferencesInflated() {
        // Toggle emulation settings enabled state based on use_custom_settings state
        listOf<Preference?>(
            findPreference("category_system"),
            findPreference("category_presentation"),
            findPreference("category_gpu"),
            findPreference("category_hacks"),
            findPreference("category_audio"),
            findPreference("category_debug")
        ).forEach { it?.dependency = "use_custom_settings" }

        findPreference<GpuDriverPreference>("gpu_driver")?.item = item

        // Hide settings that don't support per-game configuration
        var prefToRemove = findPreference<Preference>("profile_picture_value")
        prefToRemove?.parent?.removePreference(prefToRemove)
        prefToRemove = findPreference<Preference>("log_level")
        prefToRemove?.parent?.removePreference(prefToRemove)

        // TODO: remove this once we have more settings under the debug category
        // Avoid showing the debug category if no settings under it are visible
        @Suppress("SENSELESS_COMPARISON")
        if (BuildConfig.BUILD_TYPE == "release")
            findPreference<PreferenceCategory>("category_debug")?.isVisible = false
    }
}
