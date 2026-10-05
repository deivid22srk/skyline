/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.settings

import androidx.preference.Preference
import emu.skyline.R

/**
 * This fragment displays the list of global settings categories, each entry opens its own subscreen
 */
class SettingsCategoriesFragment : BaseCategoryFragment() {
    override fun onInflatePreferences() {
        addPreferencesFromResource(R.xml.settings_categories)
    }

    override fun onResume() {
        super.onResume()
        (activity as? SettingsActivity)?.setToolbarTitle(R.string.settings)
    }

    override fun onPreferenceTreeClick(preference : Preference) : Boolean {
        GlobalCategoryFragment.Screen.fromScreenKey(preference.key)?.let { screen ->
            (activity as? SettingsActivity)?.showScreen(screen)
            return true
        }
        return super.onPreferenceTreeClick(preference)
    }
}
