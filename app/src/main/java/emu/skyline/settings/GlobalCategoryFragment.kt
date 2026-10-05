/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.settings

import android.graphics.Typeface
import android.os.Bundle
import android.text.SpannableString
import android.text.style.ForegroundColorSpan
import android.text.style.StyleSpan
import androidx.lifecycle.lifecycleScope
import androidx.preference.Preference
import com.google.android.material.color.MaterialColors
import emu.skyline.R
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/**
 * This fragment displays the settings of a single global settings category subscreen
 */
class GlobalCategoryFragment : BaseCategoryFragment() {
    companion object {
        private const val SCREEN_KEY = "screen"
        private const val FOCUS_KEY = "focus_key"

        /**
         * Creates a fragment for [screen], optionally scrolling to and highlighting the
         * preference with [focusKey] once the screen is laid out
         */
        fun newInstance(screen : Screen, focusKey : String? = null) = GlobalCategoryFragment().apply {
            arguments = Bundle().apply {
                putString(SCREEN_KEY, screen.name)
                putString(FOCUS_KEY, focusKey)
            }
        }
    }

    /**
     * All global settings categories mapped to their preference XML resource and toolbar title
     */
    enum class Screen(val xmlRes : Int, val titleRes : Int) {
        Content(R.xml.prefs_content, R.string.content),
        Appearance(R.xml.prefs_appearance, R.string.appearance),
        System(R.xml.prefs_system, R.string.system),
        Display(R.xml.prefs_presentation, R.string.display),
        Gpu(R.xml.prefs_gpu, R.string.gpu),
        Hacks(R.xml.prefs_hacks, R.string.hacks),
        Debug(R.xml.prefs_debug, R.string.debug),
        Input(R.xml.prefs_input, R.string.input),
        About(R.xml.prefs_about, R.string.about);

        companion object {
            /**
             * Maps a preference category key to the subscreen that contains it, null if unknown
             */
            fun fromCategoryKey(key : String?) : Screen? = when (key) {
                "category_content" -> Content
                "category_appearance" -> Appearance
                "category_system", "category_audio" -> System
                "category_presentation" -> Display
                "category_gpu" -> Gpu
                "category_hacks" -> Hacks
                "category_debug" -> Debug
                "category_input" -> Input
                "category_credits", "category_licenses" -> About
                else -> null
            }

            /**
             * Maps a categories-home entry key to its subscreen, null if unknown
             */
            fun fromScreenKey(key : String?) : Screen? = when (key) {
                "screen_content" -> Content
                "screen_appearance" -> Appearance
                "screen_system" -> System
                "screen_display" -> Display
                "screen_gpu" -> Gpu
                "screen_hacks" -> Hacks
                "screen_debug" -> Debug
                "screen_input" -> Input
                "screen_about" -> About
                else -> null
            }
        }
    }

    private val screen by lazy { Screen.valueOf(requireArguments().getString(SCREEN_KEY)!!) }
    private val focusKey by lazy { requireArguments().getString(FOCUS_KEY) }

    override fun onInflatePreferences() {
        addPreferencesFromResource(screen.xmlRes)
    }

    override fun onResume() {
        super.onResume()
        (activity as? SettingsActivity)?.setToolbarTitle(screen.titleRes)
    }

    override fun onAfterPreferencesInflated() {
        val key = focusKey ?: return
        val preference = findPreference<Preference>(key) ?: return
        scrollToPreference(preference)
        // Briefly highlight the target preference so the user spots it after scrolling
        val originalTitle = preference.title
        val highlightColor = MaterialColors.getColor(requireContext(), com.google.android.material.R.attr.colorPrimary)
        preference.title = SpannableString(originalTitle).apply {
            setSpan(ForegroundColorSpan(highlightColor), 0, length, SpannableString.SPAN_INCLUSIVE_INCLUSIVE)
            setSpan(StyleSpan(Typeface.BOLD), 0, length, SpannableString.SPAN_INCLUSIVE_INCLUSIVE)
        }
        viewLifecycleOwner.lifecycleScope.launch {
            delay(1500)
            preference.title = originalTitle
        }
    }
}
