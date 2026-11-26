package ir.infinit

import android.app.Activity
import android.content.ActivityNotFoundException
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.provider.MediaStore
import androidx.core.content.FileProvider
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    private val channelName = "ir.infinit/settings"
    private val scannerChannelName = "ir.infinit/scanner"
    private val prefsName = "infinit_settings"
    private val samplesKey = "scanner_samples"

    private val inkSamplesKey = "ink_samples"
    private val captureRequest = 4127
    private var pendingCapture: MethodChannel.Result? = null
    private var captureFile: java.io.File? = null
    private var cameraPreview: CameraPreviewController? = null

    // LauncherIndigo is the only alias enabled in the manifest so a fresh
    // install has the default icon.  Every other alias starts disabled and is
    // enabled at runtime.  LauncherIndigoAlt is a runtime copy of the indigo
    // icon: the boot alias is disabled on the first switch and stays that way,
    // so returning to indigo uses the copy.  Aliases target the LauncherActivity
    // trampoline, so switching one never closes the running task and can happen
    // at once.
    private val bootAlias = "ir.infinit.LauncherIndigo"
    private val indigoAlias = "ir.infinit.LauncherIndigoAlt"
    private val runtimeAliases = listOf(
        indigoAlias,
        "ir.infinit.LauncherRose",
        "ir.infinit.LauncherOcean",
        "ir.infinit.LauncherEmerald",
        "ir.infinit.LauncherAmber",
        "ir.infinit.LauncherGraphite",
    )
    private val paletteAliases = mapOf(
        "rose" to "ir.infinit.LauncherRose",
        "ocean" to "ir.infinit.LauncherOcean",
        "emerald" to "ir.infinit.LauncherEmerald",
        "amber" to "ir.infinit.LauncherAmber",
        "graphite" to "ir.infinit.LauncherGraphite",
    )

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, channelName)
            .setMethodCallHandler { call, result ->
                val prefs = getSharedPreferences(prefsName, Context.MODE_PRIVATE)
                when (call.method) {
                    "load" -> {
                        val values = HashMap<String, String>()
                        for ((key, value) in prefs.all) {
                            if (value != null) values[key] = value.toString()
                        }
                        result.success(values)
                    }
                    "save" -> {
                        val args = call.arguments as? Map<*, *>
                        if (args == null) {
                            result.error("bad_args", "Expected a map", null)
                            return@setMethodCallHandler
                        }
                        val editor = prefs.edit()
                        for ((key, value) in args) {
                            if (key is String && value is String) editor.putString(key, value)
                        }
                        editor.apply()
                        result.success(null)
                    }
                    "load_samples" -> {
                        result.success(prefs.getString(samplesKey, null))
                    }
                    "save_samples" -> {
                        val data = (call.arguments as? Map<*, *>)?.get("data") as? String
                        if (data == null) {
                            result.error("bad_args", "Expected base64 data", null)
                        } else {
                            prefs.edit().putString(samplesKey, data).apply()
                            result.success(null)
                        }
                    }
                    "clear_samples" -> {
                        prefs.edit().remove(samplesKey).apply()
                        result.success(null)
                    }
                    // Personal stroke prototypes for the handwriting path are a
                    // separate bank (§12A.5), so they get their own key.
                    "load_ink_samples" -> {
                        result.success(prefs.getString(inkSamplesKey, null))
                    }
                    "save_ink_samples" -> {
                        val data = (call.arguments as? Map<*, *>)?.get("data") as? String
                        if (data == null) {
                            result.error("bad_args", "Expected base64 data", null)
                        } else {
                            prefs.edit().putString(inkSamplesKey, data).apply()
                            result.success(null)
                        }
                    }
                    "clear_ink_samples" -> {
                        prefs.edit().remove(inkSamplesKey).apply()
                        result.success(null)
                    }
                    "set_desktop_mode" -> {
                        applyDesktopMode(call.arguments as? Boolean ?: false)
                        result.success(null)
                    }
                    "set_launcher_icon" -> {
                        val palette = call.arguments as? String
                        if (palette == null) {
                            result.error("bad_args", "Expected a palette id", null)
                        } else {
                            result.success(applyLauncherIcon(palette))
                        }
                    }
                    else -> result.notImplemented()
                }
            }

        // The in-app preview lives in CameraPreviewController; the intent
        // below stays as the fallback for devices where Camera2 cannot start.
        cameraPreview = CameraPreviewController(this, flutterEngine)
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, scannerChannelName)
            .setMethodCallHandler { call, result ->
                when (call.method) {
                    "capturePhoto" -> capturePhoto(result)
                    else -> result.notImplemented()
                }
            }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray,
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        cameraPreview?.onPermissionResult(requestCode, grantResults)
    }

    override fun onDestroy() {
        cameraPreview?.dispose()
        cameraPreview = null
        super.onDestroy()
    }

    private fun capturePhoto(result: MethodChannel.Result) {
        if (pendingCapture != null) {
            result.error("busy", "A photo capture is already running", null)
            return
        }
        val file = java.io.File(
            cacheDir,
            "scan-" + System.currentTimeMillis() + ".jpg",
        )
        try {
            val uri = FileProvider.getUriForFile(this, "$packageName.fileprovider", file)
            val intent = Intent(MediaStore.ACTION_IMAGE_CAPTURE).apply {
                putExtra(MediaStore.EXTRA_OUTPUT, uri)
                addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
            }
            pendingCapture = result
            captureFile = file
            startActivityForResult(intent, captureRequest)
        } catch (error: ActivityNotFoundException) {
            pendingCapture = null
            captureFile = null
            result.error("no_camera", "No camera app is available", null)
        } catch (error: Exception) {
            pendingCapture = null
            captureFile = null
            result.error("no_camera", error.message ?: "Camera unavailable", null)
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != captureRequest) return
        val pending = pendingCapture
        val file = captureFile
        pendingCapture = null
        captureFile = null
        if (pending == null) return
        if (resultCode == Activity.RESULT_OK && file != null) {
            // The Dart side reads this file and deletes it once the bytes are
            // in memory. Return a real filesystem path, not the content URI:
            // FileProvider URIs are not readable with dart:io File.
            pending.success(file.absolutePath)
        } else {
            file?.delete()
            pending.success(null)
        }
    }

    // Desktop mode is a full-window view, so the status bar only steals taps
    // from the scaled desktop header.  The legacy Flutter flags are ignored
    // on Android 15+, so use the insets controller directly.
    private fun applyDesktopMode(enabled: Boolean) {
        val controller = WindowCompat.getInsetsController(window, window.decorView)
        controller.systemBarsBehavior =
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        if (enabled) {
            controller.hide(WindowInsetsCompat.Type.statusBars())
        } else {
            controller.show(WindowInsetsCompat.Type.statusBars())
        }
    }

    private fun applyLauncherIcon(palette: String): Boolean {
        val target = targetAlias(palette)
        val current = currentAlias()
        // The new alias is enabled before the old one is turned off, so the
        // package always keeps at least one launcher entry and the launcher
        // never shows it as disabled.
        return if (current == target) true else switchAlias(target, current)
    }

    private fun switchAlias(target: String, previous: String?): Boolean = try {
        val manager = packageManager
        setAliasState(manager, target, PackageManager.COMPONENT_ENABLED_STATE_ENABLED)
        if (previous != null && previous != target) {
            // A runtime alias's manifest default is disabled, so restoring that
            // default turns it off without an explicit disable request.  Only
            // LauncherIndigo, which is enabled in the manifest, needs an
            // explicit disable.
            val state = if (previous == bootAlias) {
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED
            } else {
                PackageManager.COMPONENT_ENABLED_STATE_DEFAULT
            }
            setAliasState(manager, previous, state)
        }
        true
    } catch (error: Exception) {
        false
    }

    private fun targetAlias(palette: String): String {
        val alias = paletteAliases[palette]
        if (alias != null) return alias
        // Any unknown value, including "indigo", falls back to indigo.  Once
        // the boot alias has been disabled it must stay disabled, so use the
        // copy.
        return if (componentState(bootAlias) == PackageManager.COMPONENT_ENABLED_STATE_DISABLED) {
            indigoAlias
        } else {
            bootAlias
        }
    }

    private fun currentAlias(): String? {
        for (alias in runtimeAliases) {
            if (componentState(alias) == PackageManager.COMPONENT_ENABLED_STATE_ENABLED) return alias
        }
        return if (componentState(bootAlias) != PackageManager.COMPONENT_ENABLED_STATE_DISABLED) {
            bootAlias
        } else {
            null
        }
    }

    private fun componentState(alias: String): Int =
        packageManager.getComponentEnabledSetting(ComponentName(this, alias))

    // Skip aliases whose state already matches so a normal launch does not make
    // the launcher rebuild its entries.
    private fun setAliasState(manager: PackageManager, alias: String, state: Int) {
        val component = ComponentName(this, alias)
        if (manager.getComponentEnabledSetting(component) == state) return
        manager.setComponentEnabledSetting(component, state, PackageManager.DONT_KILL_APP)
    }
}
