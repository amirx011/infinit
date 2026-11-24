pluginManagement {
    val flutterSdkPath =
        run {
            val properties = java.util.Properties()
            file("local.properties").inputStream().use { properties.load(it) }
            val flutterSdkPath = properties.getProperty("flutter.sdk")
            require(flutterSdkPath != null) { "flutter.sdk not set in local.properties" }
            flutterSdkPath
        }

    includeBuild("$flutterSdkPath/packages/flutter_tools/gradle")

    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

plugins {
    id("dev.flutter.flutter-plugin-loader") version "1.0.0"
    id("com.android.application") version "9.1.0" apply false
    id("org.jetbrains.kotlin.android") version "2.4.0" apply false
}

include(":app")

// Some Flutter plugins pin their own Android Gradle Plugin and AndroidX
// versions.  file_selector_android asks for AGP 8.13.1 and annotation 1.10.0,
// but the project uses AGP 9.1.0 and this machine resolves artifacts from the
// local Gradle cache only (Google Maven and Flutter's artifact host answer 403
// here).  Pin those requests to versions the app already resolves so a plugin
// cannot drag in an artifact that is not cached.
fun org.gradle.api.artifacts.ResolutionStrategy.pinLocalVersions() {
    eachDependency {
        if (requested.group == "com.android.tools.build" && requested.name == "gradle") {
            useVersion("9.1.0")
        }
        if (requested.group == "androidx.annotation" && requested.name == "annotation") {
            useVersion("1.8.1")
        }
    }
}

gradle.beforeProject {
    buildscript.configurations.configureEach { resolutionStrategy.pinLocalVersions() }
    configurations.configureEach { resolutionStrategy.pinLocalVersions() }
}
