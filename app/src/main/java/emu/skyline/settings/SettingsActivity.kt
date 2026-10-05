/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.settings

import android.annotation.SuppressLint
import android.os.Bundle
import android.text.TextUtils
import android.view.KeyEvent
import android.view.Menu
import android.view.MenuItem
import android.view.ViewTreeObserver
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.SearchView
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.WindowCompat
import androidx.preference.EditTextPreference
import androidx.preference.ListPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.fragment.app.FragmentManager
import com.google.android.material.appbar.AppBarLayout
import com.google.android.material.internal.ToolbarUtils
import emu.skyline.BuildConfig
import emu.skyline.R
import emu.skyline.data.AppItemTag
import emu.skyline.databinding.SettingsActivityBinding
import emu.skyline.preference.IntegerListPreference
import emu.skyline.preference.dialog.EditTextPreferenceMaterialDialogFragmentCompat
import emu.skyline.preference.dialog.IntegerListPreferenceMaterialDialogFragmentCompat
import emu.skyline.preference.dialog.ListPreferenceMaterialDialogFragmentCompat
import emu.skyline.utils.WindowInsetsHelper

private const val PREFERENCE_DIALOG_FRAGMENT_TAG = "androidx.preference.PreferenceFragment.DIALOG"
private const val QUERY_KEY = "current_query"

class SettingsActivity : AppCompatActivity(), PreferenceFragmentCompat.OnPreferenceDisplayDialogCallback {
    val binding by lazy { SettingsActivityBinding.inflate(layoutInflater) }
    val hiddenCategoriesFromSearch = if (BuildConfig.BUILD_TYPE == "release") {
        arrayOf("category_debug", "category_credits", "category_licenses")
    } else {
        arrayOf("category_credits", "category_licenses")
    }

    /**
     * The current search query, preserved across configuration changes
     */
    var currentQuery : String = ""

    /**
     * The fragment shown at the root of the settings navigation: the categories menu for global
     * settings or the per-game settings for a specific game
     */
    private val rootFragment by lazy {
        if (intent.hasExtra(AppItemTag))
            GameSettingsFragment().apply { arguments = intent.extras }
        else
            SettingsCategoriesFragment()
    }

    /**
     * This initializes all of the elements in the activity and displays the root settings fragment
     */
    override fun onCreate(savedInstanceState : Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(binding.root)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsHelper.applyToActivity(binding.root)

        setSupportActionBar(binding.titlebar.toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        var layoutDone = false // Tracks if the layout is complete to avoid retrieving invalid attributes
        binding.coordinatorLayout.viewTreeObserver.addOnTouchModeChangeListener { isTouchMode ->
            val layoutUpdate = {
                val params = binding.settings.layoutParams as CoordinatorLayout.LayoutParams
                if (!isTouchMode) {
                    binding.titlebar.appBarLayout.setExpanded(true)
                    params.height = binding.coordinatorLayout.height - binding.titlebar.toolbar.height
                } else {
                    params.height = CoordinatorLayout.LayoutParams.MATCH_PARENT
                }

                binding.settings.layoutParams = params
                binding.settings.requestLayout()
            }

            if (!layoutDone) {
                binding.coordinatorLayout.viewTreeObserver.addOnGlobalLayoutListener(object : ViewTreeObserver.OnGlobalLayoutListener {
                    override fun onGlobalLayout() {
                        // We need to wait till the layout is done to get the correct height of the toolbar
                        binding.coordinatorLayout.viewTreeObserver.removeOnGlobalLayoutListener(this)
                        layoutUpdate()
                        layoutDone = true
                    }
                })
            } else {
                layoutUpdate()
            }
        }

        fun enableMarquee(textView : TextView) {
            textView.ellipsize = TextUtils.TruncateAt.MARQUEE
            textView.isSelected = true
            textView.marqueeRepeatLimit = -1
        }

        // Set a temporary subtitle because the retrieval of the subtitle TextView fails if subtitle is null
        supportActionBar?.subtitle = "sub"

        @SuppressLint("RestrictedApi")
        val toolbarTitleTextView = ToolbarUtils.getTitleTextView(binding.titlebar.toolbar)

        @SuppressLint("RestrictedApi")
        val toolbarSubtitleTextView = ToolbarUtils.getSubtitleTextView(binding.titlebar.toolbar)
        toolbarTitleTextView?.let { enableMarquee(it) }
        toolbarSubtitleTextView?.let { enableMarquee(it) }

        // Reset the subtitle to null
        supportActionBar?.subtitle = null

        if (savedInstanceState != null)
            currentQuery = savedInstanceState.getString(QUERY_KEY) ?: ""

        if (savedInstanceState == null) {
            supportFragmentManager
                .beginTransaction()
                .replace(R.id.settings, rootFragment)
                .commit()
        } else if (currentQuery.isNotEmpty() && supportFragmentManager.findFragmentById(R.id.settings) is SettingsCategoriesFragment) {
            // Restore the aggregated search screen after a configuration change; if a subscreen was
            // on top it is restored by the fragment manager and keeps its own back stack
            supportFragmentManager
                .beginTransaction()
                .replace(R.id.settings, SearchableSettingsFragment())
                .commit()
        }
    }

    override fun onSaveInstanceState(outState : Bundle) {
        super.onSaveInstanceState(outState)
        outState.putString(QUERY_KEY, currentQuery)
    }

    override fun onCreateOptionsMenu(menu : Menu?) : Boolean {
        menuInflater.inflate(R.menu.settings_menu, menu)
        val menuItem = menu!!.findItem(R.id.app_bar_search)
        val searchView = menuItem.actionView as SearchView
        searchView.queryHint = getString(R.string.search)

        searchView.setOnQueryTextFocusChangeListener { _, focus ->
            (binding.titlebar.toolbar.layoutParams as AppBarLayout.LayoutParams).scrollFlags =
                if (focus)
                    AppBarLayout.LayoutParams.SCROLL_FLAG_NO_SCROLL
                else
                    AppBarLayout.LayoutParams.SCROLL_FLAG_SCROLL
        }

        searchView.setOnQueryTextListener(object : SearchView.OnQueryTextListener {
            override fun onQueryTextSubmit(query : String) : Boolean {
                return false
            }

            override fun onQueryTextChange(newText : String) : Boolean {
                currentQuery = newText
                val fragmentManager = supportFragmentManager
                when (val current = fragmentManager.findFragmentById(R.id.settings)) {
                    is GameSettingsFragment ->
                        // The per-game settings are a single list, filter it in place
                        current.applySearchFilter(newText, hiddenCategoriesFromSearch)
                    is SearchableSettingsFragment ->
                        if (newText.isEmpty())
                            showCategories()
                        else
                            current.applySearchFilter(newText, hiddenCategoriesFromSearch)
                    else ->
                        // While browsing the categories or a subscreen, a non-empty query opens the
                        // aggregated search screen that filters preferences from every category
                        if (newText.isNotEmpty())
                            showSearchResults()
                }
                return true
            }
        })
        return super.onCreateOptionsMenu(menu)
    }

    /**
     * Navigates to a global settings category subscreen, optionally scrolling to and highlighting
     * the preference with [focusKey]
     */
    fun showScreen(screen : GlobalCategoryFragment.Screen, focusKey : String? = null) {
        supportFragmentManager.beginTransaction()
            .setCustomAnimations(R.anim.slide_in_right, R.anim.slide_out_left, R.anim.slide_in_left, R.anim.slide_out_right)
            .replace(R.id.settings, GlobalCategoryFragment.newInstance(screen, focusKey))
            .addToBackStack(null)
            .commit()
    }

    /**
     * Swaps the fragment back to the categories menu when the search query is cleared
     */
    private fun showCategories() {
        supportFragmentManager.beginTransaction()
            .setCustomAnimations(R.anim.fade_in, R.anim.fade_out)
            .replace(R.id.settings, SettingsCategoriesFragment())
            .commit()
    }

    /**
     * Shows the aggregated search screen, dropping any subscreen currently on the back stack
     */
    private fun showSearchResults() {
        val fragmentManager = supportFragmentManager
        if (fragmentManager.backStackEntryCount > 0)
            fragmentManager.popBackStack(null, FragmentManager.POP_BACK_STACK_INCLUSIVE)
        fragmentManager.beginTransaction()
            .setCustomAnimations(R.anim.fade_in, R.anim.fade_out)
            .replace(R.id.settings, SearchableSettingsFragment())
            .commit()
    }

    /**
     * Updates the toolbar title, used by the fragments to reflect the visible screen
     */
    fun setToolbarTitle(titleRes : Int) {
        supportActionBar?.setTitle(titleRes)
    }

    override fun onOptionsItemSelected(item : MenuItem) : Boolean {
        return if (item.itemId == android.R.id.home) {
            onBackPressedDispatcher.onBackPressed()
            true
        } else {
            super.onOptionsItemSelected(item)
        }
    }

    /**
     * This handles on calling [onBackPressed] when [KeyEvent.KEYCODE_BUTTON_B] is lifted
     */
    override fun onKeyUp(keyCode : Int, event : KeyEvent?) : Boolean {
        if (keyCode == KeyEvent.KEYCODE_BUTTON_B) {
            onBackPressedDispatcher.onBackPressed()
            return true
        }

        return super.onKeyUp(keyCode, event)
    }

    override fun finish() {
        setResult(RESULT_OK)
        super.finish()
    }

    override fun onPreferenceDisplayDialog(caller : PreferenceFragmentCompat, pref : Preference) : Boolean {
        when (pref) {
            is IntegerListPreference -> {
                // Check if dialog is already showing
                if (supportFragmentManager.findFragmentByTag(PREFERENCE_DIALOG_FRAGMENT_TAG) != null)
                    return true

                val dialogFragment = IntegerListPreferenceMaterialDialogFragmentCompat.newInstance(pref.getKey())
                @Suppress("DEPRECATION")
                dialogFragment.setTargetFragment(caller, 0) // androidx.preference.PreferenceDialogFragmentCompat depends on the target fragment being set correctly even though it's deprecated
                dialogFragment.show(supportFragmentManager, PREFERENCE_DIALOG_FRAGMENT_TAG)
                return true
            }

            is EditTextPreference -> {
                if (supportFragmentManager.findFragmentByTag(PREFERENCE_DIALOG_FRAGMENT_TAG) != null)
                    return true

                val dialogFragment = EditTextPreferenceMaterialDialogFragmentCompat.newInstance(pref.getKey())
                @Suppress("DEPRECATION")
                dialogFragment.setTargetFragment(caller, 0)
                dialogFragment.show(supportFragmentManager, PREFERENCE_DIALOG_FRAGMENT_TAG)
                return true
            }

            is ListPreference -> {
                if (supportFragmentManager.findFragmentByTag(PREFERENCE_DIALOG_FRAGMENT_TAG) != null)
                    return true

                val dialogFragment = ListPreferenceMaterialDialogFragmentCompat.newInstance(pref.getKey())
                @Suppress("DEPRECATION")
                dialogFragment.setTargetFragment(caller, 0)
                dialogFragment.show(supportFragmentManager, PREFERENCE_DIALOG_FRAGMENT_TAG)
                return true
            }

            else -> return false
        }
    }
}
