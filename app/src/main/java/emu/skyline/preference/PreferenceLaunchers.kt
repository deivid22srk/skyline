/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.Intent
import android.net.Uri
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.fragment.app.Fragment

/**
 * Central holder of the [ActivityResultLauncher]s used by the settings preferences
 *
 * `registerForActivityResult` must be called before the owning LifecycleOwner reaches the
 * STARTED state. Registering from a Preference's constructor on the hosting Activity used to
 * work when the whole settings screen was inflated during the Activity's `onCreate`, but the
 * category subscreens are swapped in dynamically while the Activity is already RESUMED, which
 * would crash with `IllegalStateException`.
 *
 * Instead, the settings fragment hosting the current screen registers every launcher in its
 * `onCreatePreferences` — at which point the fragment is still in the CREATED state — and the
 * launchers are stored here so the preferences can reach them when clicked. The result of a
 * pick is delivered to the preference instance that started it via the `pending*` fields.
 */
object PreferenceLaunchers {
    private var owner : Fragment? = null

    var folderPicker : ActivityResultLauncher<Uri?>? = null
    var keyPicker : ActivityResultLauncher<Array<String>>? = null
    var gpuDriver : ActivityResultLauncher<Intent>? = null
    var controller : ActivityResultLauncher<Intent>? = null
    var profilePicture : ActivityResultLauncher<PickVisualMediaRequest>? = null

    // The preference instance that started the current pick, used to deliver its result
    var pendingFolderPicker : FolderPickerPreference? = null
    var pendingKeyPicker : KeyPickerPreference? = null
    var pendingGpuDriver : GpuDriverPreference? = null
    var pendingController : ControllerPreference? = null
    var pendingProfilePicture : ProfilePicturePreference? = null

    /**
     * Registers every launcher used by the preferences on [fragment], which must not have
     * reached the STARTED state yet; this replaces the launchers of any previously displayed
     * settings screen
     */
    fun register(fragment : Fragment) {
        owner = fragment
        folderPicker = fragment.registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri ->
            pendingFolderPicker?.onFolderPicked(uri)
            pendingFolderPicker = null
        }
        keyPicker = fragment.registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
            pendingKeyPicker?.onKeyPicked(uri)
            pendingKeyPicker = null
        }
        gpuDriver = fragment.registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
            pendingGpuDriver?.onDriverConfigured()
            pendingGpuDriver = null
        }
        controller = fragment.registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
            pendingController?.onControllerConfigured()
            pendingController = null
        }
        profilePicture = fragment.registerForActivityResult(ActivityResultContracts.PickVisualMedia()) { uri ->
            pendingProfilePicture?.onPicturePicked(uri)
            pendingProfilePicture = null
        }
    }

    /**
     * Drops the launchers registered by [fragment] when it is destroyed, but only if it is
     * still the owning screen; a new settings screen always registers its own launchers
     * before inflating its preferences
     */
    fun unregister(fragment : Fragment) {
        if (owner === fragment) {
            owner = null
            folderPicker = null
            keyPicker = null
            gpuDriver = null
            controller = null
            profilePicture = null
            pendingFolderPicker = null
            pendingKeyPicker = null
            pendingGpuDriver = null
            pendingController = null
            pendingProfilePicture = null
        }
    }
}
