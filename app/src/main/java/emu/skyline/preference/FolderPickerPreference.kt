/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.util.AttributeSet
import androidx.preference.Preference
import androidx.preference.Preference.SummaryProvider
import androidx.preference.PreferenceManager
import androidx.preference.R
import emu.skyline.di.getSettings

class FolderPickerPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {
    init {
        summaryProvider = SummaryProvider<FolderPickerPreference> { preference ->
            Uri.decode(preference.getPersistedString(""))
        }
    }

    /**
     * Called by [PreferenceLaunchers] with the result of the folder picker started by [onClick]
     */
    fun onFolderPicked(uri : Uri?) {
        uri ?: return

        context.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)

        context.getSettings().refreshRequired = true
        PreferenceManager.getDefaultSharedPreferences(context).edit().putString(key, uri.toString()).apply()
        notifyChanged()
    }

    override fun onClick() {
        PreferenceLaunchers.pendingFolderPicker = this
        PreferenceLaunchers.folderPicker?.launch(null)
    }
}
