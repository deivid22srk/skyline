/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.settings

import android.content.Intent
import android.os.Bundle
import android.view.View
import androidx.preference.Preference
import androidx.preference.PreferenceCategory
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.TwoStatePreference
import androidx.preference.forEach
import emu.skyline.BuildConfig
import emu.skyline.MainActivity
import emu.skyline.R
import emu.skyline.utils.GpuDriverHelper
import emu.skyline.utils.WindowInsetsHelper

/**
 * Base fragment for the global settings category screens, providing the logic that is shared
 * between all of them; each statement only applies when its preference exists on the screen
 */
abstract class BaseCategoryFragment : PreferenceFragmentCompat() {
    override fun onViewCreated(view : View, savedInstanceState : Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val recyclerView = view.findViewById<View>(R.id.recycler_view)
        WindowInsetsHelper.setPadding(recyclerView, bottom = true)
    }

    override fun onCreatePreferences(savedInstanceState : Bundle?, rootKey : String?) {
        onInflatePreferences()
        applySharedSettingsLogic()
        onAfterPreferencesInflated()
    }

    /**
     * Inflates the preference XML resources of this screen
     */
    protected abstract fun onInflatePreferences()

    /**
     * Called after the preferences have been inflated and the shared logic has been applied
     */
    protected open fun onAfterPreferencesInflated() {}

    /**
     * Shared logic of the global settings screens, every lookup is guarded so it no-ops on
     * screens that don't contain the corresponding preference
     */
    protected fun applySharedSettingsLogic() {
        // Re-launch the app if Material You is toggled
        findPreference<Preference>("use_material_you")?.setOnPreferenceChangeListener { _, _ ->
            requireActivity().finishAffinity()
            startActivity(Intent(requireContext(), MainActivity::class.java))
            true
        }

        // Uncheck `disable_frame_throttling` if `force_triple_buffering` gets disabled
        val disableFrameThrottlingPref = findPreference<TwoStatePreference>("disable_frame_throttling")
        findPreference<TwoStatePreference>("force_triple_buffering")?.setOnPreferenceChangeListener { _, newValue ->
            if (newValue == false)
                disableFrameThrottlingPref?.isChecked = false
            true
        }

        // Only show validation layer setting in debug builds
        @Suppress("SENSELESS_COMPARISON")
        if (BuildConfig.BUILD_TYPE != "release")
            findPreference<Preference>("validation_layer")?.isVisible = true

        if (!GpuDriverHelper.supportsForceMaxGpuClocks()) {
            findPreference<TwoStatePreference>("force_max_gpu_clocks")?.apply {
                isSelectable = false
                isChecked = false
                summary = requireContext().getString(R.string.force_max_gpu_clocks_desc_unsupported)
            }
        }

        // The credits entries are dynamically added, skip them when the category is hidden (search screen)
        findPreference<PreferenceCategory>("category_credits")?.takeIf { it.isVisible }?.let { category ->
            resources.getStringArray(R.array.credits_entries).asIterable().shuffled().forEach {
                category.addPreference(Preference(requireContext()).apply { title = it })
            }
        }
    }

    /**
     * Live-filters the preferences of this screen against the search query, mirroring the original
     * behavior of `SettingsActivity`: query terms are comma separated and hidden keys are dropped
     */
    fun applySearchFilter(newText : String, hiddenCategories : Array<out String>) {
        val queries = newText.split(",")
        if (newText.isNotEmpty()) {
            preferenceScreen.forEach { preferenceCategory ->
                if (hiddenCategories.contains(preferenceCategory.key)) {
                    preferenceCategory.isVisible = false
                    return@forEach
                }
                val queryMatchesCategory = queries.any { preferenceCategory.title?.contains(it, true) ?: false }
                // Tracks whether all preferences under this category are hidden
                var areAllPrefsHidden = true
                (preferenceCategory as PreferenceCategory).forEach { preference ->
                    preference.isVisible = queryMatchesCategory || queries.any { preference.title?.contains(it, true) ?: false }
                    if (preference.isVisible && areAllPrefsHidden)
                        areAllPrefsHidden = false
                }
                // Hide PreferenceCategory if none of its preferences match the search and neither the category title
                preferenceCategory.isVisible = !areAllPrefsHidden || queryMatchesCategory
            }
        } else { // If user input is empty, show all preferences
            preferenceScreen.forEach { preferenceCategory ->
                preferenceCategory.isVisible = true
                (preferenceCategory as PreferenceCategory).forEach { preference ->
                    preference.isVisible = true
                }
            }
        }
    }
}
