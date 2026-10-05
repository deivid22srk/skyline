/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.settings

import androidx.preference.Preference
import androidx.preference.PreferenceCategory
import emu.skyline.BuildConfig
import emu.skyline.R

/**
 * This fragment aggregates the preferences of every global settings category so the search can
 * filter across all of them; tapping a result opens the subscreen that owns the preference
 */
class SearchableSettingsFragment : BaseCategoryFragment() {
    override fun onInflatePreferences() {
        addPreferencesFromResource(R.xml.prefs_content)
        addPreferencesFromResource(R.xml.prefs_appearance)
        addPreferencesFromResource(R.xml.prefs_system)
        addPreferencesFromResource(R.xml.prefs_presentation)
        addPreferencesFromResource(R.xml.prefs_gpu)
        addPreferencesFromResource(R.xml.prefs_hacks)
        addPreferencesFromResource(R.xml.prefs_debug)
        addPreferencesFromResource(R.xml.prefs_input)
        addPreferencesFromResource(R.xml.prefs_about)
        // Hide the categories excluded from search results, mirroring the original behavior
        hiddenCategoryKeys.forEach { key -> findPreference<PreferenceCategory>(key)?.isVisible = false }
    }

    private val hiddenCategoryKeys : Array<out String>
        get() = if (BuildConfig.BUILD_TYPE == "release")
            arrayOf("category_debug", "category_credits", "category_licenses")
        else
            arrayOf("category_credits", "category_licenses")

    override fun onResume() {
        super.onResume()
        (activity as? SettingsActivity)?.let { activity ->
            activity.setToolbarTitle(R.string.settings)
            // Re-apply the current query after this fragment has been (re)created
            applySearchFilter(activity.currentQuery, activity.hiddenCategoriesFromSearch)
        }
    }

    override fun onPreferenceTreeClick(preference : Preference) : Boolean {
        // While searching, tapping a result opens the owning subscreen with the preference focused
        if ((activity as? SettingsActivity)?.currentQuery?.isNotEmpty() == true) {
            var parent = preference.parent
            while (parent != null && parent !is PreferenceCategory)
                parent = parent.parent
            GlobalCategoryFragment.Screen.fromCategoryKey(parent?.key)?.let { screen ->
                (activity as? SettingsActivity)?.showScreen(screen, preference.key)
                return true
            }
        }
        return super.onPreferenceTreeClick(preference)
    }
}
