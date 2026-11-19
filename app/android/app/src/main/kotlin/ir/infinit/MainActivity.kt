package ir.infinit

import android.content.ComponentName
import android.content.Context
import android.content.pm.PackageManager
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    private val channelName = "ir.infinit/settings"
    private val prefsName = "infinit_settings"

    // LauncherRose is the only alias enabled in the manifest so a fresh install
    // has an icon.  Every other alias starts disabled and is enabled at
    // runtime.  LauncherRoseAlt is a runtime copy of the rose icon: the boot
    // alias is disabled on the first switch and stays that way, so returning to
    // rose uses the copy.  Aliases target the LauncherActivity trampoline, so
    // switching one never closes the running task and can happen at once.
    private val bootAlias = "ir.infinit.LauncherRose"
    private val roseAlias = "ir.infinit.LauncherRoseAlt"
    private val runtimeAliases = listOf(
        roseAlias,
        "ir.infinit.LauncherOcean",
        "ir.infinit.LauncherIndigo",
        "ir.infinit.LauncherEmerald",
        "ir.infinit.LauncherAmber",
        "ir.infinit.LauncherGraphite",
    )
    private val paletteAliases = mapOf(
        "ocean" to "ir.infinit.LauncherOcean",
        "indigo" to "ir.infinit.LauncherIndigo",
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
    }

    /// Desktop mode is a full-window view, so the status bar only steals taps
    /// from the scaled desktop header.  The legacy Flutter flags are ignored
    /// on Android 15+, so use the insets controller directly.
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
            // LauncherRose, which is enabled in the manifest, needs an
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
        // Any unknown value, including "rose", falls back to rose.  Once the
        // boot alias has been disabled it must stay disabled, so use the copy.
        return if (componentState(bootAlias) == PackageManager.COMPONENT_ENABLED_STATE_DISABLED) {
            roseAlias
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
