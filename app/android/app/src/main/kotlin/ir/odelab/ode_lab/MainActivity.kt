package ir.odelab.ode_lab

import android.content.Context
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    private val channelName = "ir.odelab.ode_lab/settings"
    private val prefsName = "odelab_settings"

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
                    else -> result.notImplemented()
                }
            }
    }
}
