import java.io.File
import java.io.FileInputStream
import java.util.Properties

plugins {
    id("com.android.application")
    // The Flutter Gradle Plugin must be applied after the Android and Kotlin Gradle plugins.
    id("dev.flutter.flutter-gradle-plugin")
}

// Pick an NDK that is actually installed.  Flutter's default revision is not
// always present, and Gradle otherwise tries to download it mid-build.
val localProperties = Properties().apply {
    val f = rootProject.file("local.properties")
    if (f.exists()) FileInputStream(f).use { load(it) }
}
val sdkDir: String? = System.getenv("ANDROID_HOME")
    ?: System.getenv("ANDROID_SDK_ROOT")
    ?: localProperties.getProperty("sdk.dir")
val installedNdks: List<String> = sdkDir
    ?.let { File(it, "ndk").listFiles()?.filter { f -> f.isDirectory }?.map { f -> f.name } }
    ?.sorted()
    ?: emptyList()
val ndkOverride: String? = System.getenv("INFINIT_NDK_VERSION")?.takeIf { it.isNotBlank() }
    ?: installedNdks.lastOrNull()

android {
    namespace = "ir.infinit"
    compileSdk = flutter.compileSdkVersion
    ndkVersion = ndkOverride ?: flutter.ndkVersion

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    defaultConfig {
        // TODO: Specify your own unique Application ID (https://developer.android.com/studio/build/application-id.html).
        applicationId = "ir.infinit"
        // You can update the following values to match your application needs.
        // For more information, see: https://flutter.dev/to/review-gradle-config.
        minSdk = flutter.minSdkVersion
        targetSdk = flutter.targetSdkVersion
        // Uses the version code from pubspec.yaml. When using split APKs, 1000 * ABI_VERSION
        // is added automatically by Flutter. (https://developer.android.com/studio/build/configure-apk-splits#configure-APK-versions)
        // You can force using the value of versionCode by specifying the `-P force-version-code-ignoring-abi=true`
        // flag during build.
        versionCode = flutter.versionCode
        versionName = flutter.versionName
    }

    buildTypes {
        release {
            // TODO: Add your own signing config for the release build.
            // Signing with the debug keys for now, so `flutter run --release` works.
            signingConfig = signingConfigs.getByName("debug")
        }
    }
}

kotlin {
    compilerOptions {
        jvmTarget = org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17
    }
}

flutter {
    source = "../.."
}

// ---------------------------------------------------------------------------
// Native engine (libodecore.so)
//
// Flutter only packages the shared libraries that already sit in jniLibs, so a
// plain `flutter build apk` used to produce an APK without the exact-arithmetic
// core — the app then reported "native engine unavailable" at runtime.  This
// task cross-compiles the C++ core with the NDK before every build and stages
// it for the ABIs below, so the APK is always self-contained.
//
//   skip it:            -Pinfinit.skipCore=true
//   limit the ABIs:     INFINIT_ANDROID_ABIS=arm64-v8a,x86_64
// ---------------------------------------------------------------------------
val odeCoreAbis: String =
    System.getenv("INFINIT_ANDROID_ABIS") ?: "arm64-v8a,armeabi-v7a,x86_64"
val skipOdeCore: Boolean = (project.findProperty("infinit.skipCore") as String?) == "true"

val buildOdeCoreAndroid by tasks.registering {
    group = "build"
    description = "Cross-compiles libodecore.so for Android and stages it in jniLibs."

    val script = rootProject.file("../../scripts/build_android.sh")
    val coreDir = rootProject.file("../../core")
    val jniDir = file("src/main/jniLibs")

    inputs.files(script)
    inputs.files(fileTree(coreDir) {
        include("CMakeLists.txt", "include/**", "src/**")
    })
    inputs.property("abis", odeCoreAbis)
    outputs.dir(jniDir)

    doLast {
        if (skipOdeCore) {
            logger.lifecycle("libodecore: skipped (-Pinfinit.skipCore=true)")
            return@doLast
        }
        if (!script.exists()) {
            throw GradleException("Missing build script: ${script.absolutePath}")
        }
        logger.lifecycle("libodecore: building for $odeCoreAbis")
        val process = ProcessBuilder("bash", script.absolutePath, "--core-only")
            .directory(rootProject.projectDir)
            .redirectErrorStream(true)
            .also { it.environment()["ABIS"] = odeCoreAbis.replace(',', ' ') }
            .start()
        process.inputStream.bufferedReader().useLines { lines ->
            lines.forEach { logger.lifecycle("  $it") }
        }
        val code = process.waitFor()
        if (code != 0) {
            throw GradleException(
                "libodecore.so could not be built for Android (exit code $code).\n" +
                    "Install the NDK and the host cmake/ninja, or run " +
                    "scripts/build_android.sh manually.",
            )
        }
    }
}

tasks.named("preBuild").configure { dependsOn(buildOdeCoreAndroid) }

