package ir.infinit

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.ImageFormat
import android.graphics.Matrix
import android.graphics.Rect
import android.graphics.SurfaceTexture
import android.hardware.camera2.CameraCaptureSession
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraDevice
import android.hardware.camera2.CameraManager
import android.hardware.camera2.CaptureRequest
import android.hardware.camera2.params.MeteringRectangle
import android.media.ImageReader
import android.os.Build
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.util.Size
import android.view.Surface
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.view.TextureRegistry
import java.io.File
import java.io.FileOutputStream
import kotlin.math.abs

/// In-app camera preview built on Camera2 and a Flutter SurfaceProducer. No
/// third-party camera package: the system framework already provides the
/// preview surface, still capture and torch control. The Dart side shows the
/// texture and calls `previewCapture`.
class CameraPreviewController(
    private val activity: Activity,
    engine: FlutterEngine,
) {
    private val channel = MethodChannel(engine.dartExecutor.binaryMessenger, "ir.infinit/camera")
    private val renderer = engine.renderer
    private val manager = activity.getSystemService(Context.CAMERA_SERVICE) as CameraManager
    private val mainHandler = Handler(Looper.getMainLooper())
    private var captureThread: HandlerThread? = null
    private var captureHandler: Handler? = null

    private var device: CameraDevice? = null
    private var session: CameraCaptureSession? = null
    private var request: CaptureRequest.Builder? = null
    private var producer: TextureRegistry.SurfaceProducer? = null
    private var reader: ImageReader? = null
    private var captureSize: Size? = null
    private var previewSize: Size? = null
    private var activeArray: Rect? = null
    private var characteristics: CameraCharacteristics? = null
    private var maxZoom = 1f
    private var zoom = 1f
    private var rotation = 0
    private var torch = false
    private var pendingStart: MethodChannel.Result? = null
    private var pendingCapture: MethodChannel.Result? = null

    init {
        channel.setMethodCallHandler(::onCall)
    }

    private fun onCall(call: MethodCall, result: MethodChannel.Result) {
        when (call.method) {
            "previewStart" -> start(result)
            "previewStop" -> {
                stop()
                result.success(null)
            }
            "previewCapture" -> capture(result)
            "previewTorch" -> setTorch(call.argument<Boolean>("on") == true, result)
            "previewZoom" -> setZoom(call.argument<Double>("value")?.toFloat() ?: 1f, result)
            "previewFocus" -> focus(
                call.argument<Double>("x") ?: 0.5,
                call.argument<Double>("y") ?: 0.5,
                result,
            )
            else -> result.notImplemented()
        }
    }

    private fun hasPermission(): Boolean =
        ContextCompat.checkSelfPermission(activity, Manifest.permission.CAMERA) ==
            PackageManager.PERMISSION_GRANTED

    private fun start(result: MethodChannel.Result) {
        if (!hasPermission()) {
            if (pendingStart != null) {
                result.error("busy", "A camera start is already running", null)
                return
            }
            pendingStart = result
            ActivityCompat.requestPermissions(activity, arrayOf(Manifest.permission.CAMERA), permissionRequest)
            return
        }
        openPreview(result)
    }

    /// MainActivity forwards the permission result here. A denied request is a
    /// normal answer, not an exception: the page offers the system camera.
    fun onPermissionResult(requestCode: Int, grantResults: IntArray) {
        if (requestCode != permissionRequest) return
        val pending = pendingStart ?: return
        pendingStart = null
        if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            openPreview(pending)
        } else {
            pending.error("permission", "Camera permission was denied", null)
        }
    }

    private fun openPreview(result: MethodChannel.Result) {
        try {
            stop()
            val id = chooseCamera()
            if (id == null) {
                result.error("unavailable", "No camera is available", null)
                return
            }
            val characteristics = manager.getCameraCharacteristics(id)
            this.characteristics = characteristics
            activeArray = characteristics.get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE)
            maxZoom = characteristics
                .get(CameraCharacteristics.SCALER_AVAILABLE_MAX_DIGITAL_ZOOM)
                ?.coerceAtLeast(1f) ?: 1f
            zoom = 1f
            val sensor = characteristics.get(CameraCharacteristics.SENSOR_ORIENTATION) ?: 0
            rotation = (sensor - displayRotation() + 360) % 360
            val previewSize = chooseSize(
                characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)
                    ?.getOutputSizes(SurfaceTexture::class.java),
                1280,
            ) ?: Size(1280, 720)
            this.previewSize = previewSize
            captureSize = chooseSize(
                characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP)
                    ?.getOutputSizes(ImageFormat.JPEG),
                2000,
            ) ?: previewSize

            val surfaceProducer = renderer.createSurfaceProducer()
            surfaceProducer.setSize(previewSize.width, previewSize.height)
            producer = surfaceProducer

            val thread = HandlerThread("infinit-camera").also { it.start() }
            captureThread = thread
            captureHandler = Handler(thread.looper)
            val imageReader = ImageReader.newInstance(
                captureSize!!.width,
                captureSize!!.height,
                ImageFormat.JPEG,
                2,
            )
            imageReader.setOnImageAvailableListener({ source ->
                val image = source.acquireNextImage() ?: return@setOnImageAvailableListener
                val buffer = image.planes[0].buffer
                val bytes = ByteArray(buffer.remaining())
                buffer.get(bytes)
                image.close()
                saveCapture(bytes)
            }, captureHandler)
            reader = imageReader

            manager.openCamera(id, object : CameraDevice.StateCallback() {
                override fun onOpened(camera: CameraDevice) {
                    device = camera
                    createSession(result, surfaceProducer.surface, imageReader.surface)
                }

                override fun onDisconnected(camera: CameraDevice) {
                    camera.close()
                    if (device === camera) device = null
                }

                override fun onError(camera: CameraDevice, error: Int) {
                    camera.close()
                    if (device === camera) device = null
                    result.error("unavailable", "Camera error $error", null)
                }
            }, mainHandler)
        } catch (error: SecurityException) {
            result.error("permission", error.message ?: "Camera permission is needed", null)
        } catch (error: Exception) {
            stop()
            result.error("unavailable", error.message ?: "Camera could not start", null)
        }
    }

    private fun createSession(
        result: MethodChannel.Result,
        previewSurface: Surface,
        stillSurface: Surface,
    ) {
        val camera = device
        if (camera == null) {
            result.error("unavailable", "Camera closed during start", null)
            return
        }
        try {
            val builder = camera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW).apply {
                addTarget(previewSurface)
                set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE)
                set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_ON)
                set(CaptureRequest.FLASH_MODE, if (torch) CaptureRequest.FLASH_MODE_TORCH else CaptureRequest.FLASH_MODE_OFF)
            }
            applyZoom(builder, zoom)
            request = builder
            camera.createCaptureSession(
                listOf(previewSurface, stillSurface),
                object : CameraCaptureSession.StateCallback() {
                    override fun onConfigured(configured: CameraCaptureSession) {
                        session = configured
                        try {
                            configured.setRepeatingRequest(builder.build(), null, mainHandler)
                            val texture = producer
                            if (texture == null) {
                                result.error("unavailable", "Preview surface was released", null)
                                return
                            }
                            result.success(
                                mapOf(
                                    "textureId" to texture.id(),
                                    "width" to (previewSize?.width ?: 0),
                                    "height" to (previewSize?.height ?: 0),
                                    "rotation" to rotation,
                                    "torch" to torch,
                                    "zoom" to zoom,
                                    "maxZoom" to maxZoom,
                                ),
                            )
                        } catch (error: Exception) {
                            result.error("unavailable", error.message ?: "Preview failed", null)
                        }
                    }

                    override fun onConfigureFailed(configured: CameraCaptureSession) {
                        result.error("unavailable", "Camera session could not be configured", null)
                    }
                },
                mainHandler,
            )
        } catch (error: Exception) {
            result.error("unavailable", error.message ?: "Camera session failed", null)
        }
    }

    private fun capture(result: MethodChannel.Result) {
        val camera = device
        val configured = session
        val stillReader = reader
        if (camera == null || configured == null || stillReader == null) {
            result.error("unavailable", "The camera is not ready", null)
            return
        }
        if (pendingCapture != null) {
            result.error("busy", "A capture is already running", null)
            return
        }
        try {
            pendingCapture = result
            val builder = camera.createCaptureRequest(CameraDevice.TEMPLATE_STILL_CAPTURE).apply {
                addTarget(stillReader.surface)
                set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE)
                set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_ON)
                set(CaptureRequest.FLASH_MODE, if (torch) CaptureRequest.FLASH_MODE_TORCH else CaptureRequest.FLASH_MODE_OFF)
            }
            configured.capture(builder.build(), null, captureHandler)
        } catch (error: Exception) {
            pendingCapture = null
            result.error("unavailable", error.message ?: "Capture failed", null)
        }
    }

    /// Rotates the JPEG into display orientation before writing it, so the Dart
    /// crop screen and the native scanner agree on the pixels. The capture is
    /// capped at 2000 px on the long side to keep this copy small.
    private fun saveCapture(bytes: ByteArray) {
        val pending = pendingCapture
        pendingCapture = null
        try {
            val bitmap = BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
                ?: throw IllegalStateException("The camera returned an empty frame")
            val upright = if (rotation == 0) {
                bitmap
            } else {
                val matrix = Matrix().apply { postRotate(rotation.toFloat()) }
                Bitmap.createBitmap(bitmap, 0, 0, bitmap.width, bitmap.height, matrix, true)
                    .also { if (it !== bitmap) bitmap.recycle() }
            }
            val file = File(activity.cacheDir, "capture-${System.currentTimeMillis()}.jpg")
            FileOutputStream(file).use { out ->
                upright.compress(Bitmap.CompressFormat.JPEG, 94, out)
            }
            upright.recycle()
            pending?.success(file.absolutePath)
        } catch (error: Exception) {
            pending?.error("capture", error.message ?: "Capture failed", null)
        }
    }

    /// Digital zoom is a crop of the sensor's active array. Flutter sends the
    /// pinch ratio; the value is clamped to what the camera advertises.
    private fun setZoom(value: Float, result: MethodChannel.Result) {
        val builder = request
        val configured = session
        if (builder == null || configured == null) {
            result.error("unavailable", "The camera is not ready", null)
            return
        }
        try {
            zoom = value.coerceIn(1f, maxZoom)
            applyZoom(builder, zoom)
            configured.setRepeatingRequest(builder.build(), null, mainHandler)
            result.success(zoom.toDouble())
        } catch (error: Exception) {
            result.error("unavailable", error.message ?: "Zoom failed", null)
        }
    }

    private fun applyZoom(builder: CaptureRequest.Builder, value: Float) {
        val array = activeArray ?: return
        if (value <= 1.001f) {
            builder.set(CaptureRequest.SCALER_CROP_REGION, array)
            return
        }
        val width = (array.width() / value).toInt().coerceAtLeast(1)
        val height = (array.height() / value).toInt().coerceAtLeast(1)
        val halfW = width / 2
        val halfH = height / 2
        val cx = array.centerX()
        val cy = array.centerY()
        builder.set(
            CaptureRequest.SCALER_CROP_REGION,
            Rect(
                (cx - halfW).coerceAtLeast(array.left),
                (cy - halfH).coerceAtLeast(array.top),
                (cx + halfW).coerceAtMost(array.right),
                (cy + halfH).coerceAtMost(array.bottom),
            ),
        )
    }

    /// Tap-to-focus/expose. The Dart point is normalised in the *display*
    /// orientation, so it is rotated into sensor coordinates first.
    private fun focus(x: Double, y: Double, result: MethodChannel.Result) {
        val camera = device
        val builder = request
        val configured = session
        val array = activeArray
        if (camera == null || builder == null || configured == null || array == null) {
            result.error("unavailable", "The camera is not ready", null)
            return
        }
        val modes = characteristics
            ?.get(CameraCharacteristics.CONTROL_AF_AVAILABLE_MODES) ?: intArrayOf()
        val hasAutoFocus = modes.any {
            it == CaptureRequest.CONTROL_AF_MODE_AUTO ||
                it == CaptureRequest.CONTROL_AF_MODE_MACRO ||
                it == CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE
        }
        if (!hasAutoFocus) {
            result.success(false)
            return
        }
        val nx = x.coerceIn(0.0, 1.0).toFloat()
        val ny = y.coerceIn(0.0, 1.0).toFloat()
        val (sx, sy) = when (rotation) {
            90 -> ny to 1f - nx
            180 -> 1f - nx to 1f - ny
            270 -> 1f - ny to nx
            else -> nx to ny
        }
        try {
            val side = (minOf(array.width(), array.height()) * 0.22f).toInt()
            val half = side / 2
            val cx = (array.left + sx * array.width()).toInt()
            val cy = (array.top + sy * array.height()).toInt()
            val left = (cx - half).coerceIn(array.left, array.right - 1)
            val top = (cy - half).coerceIn(array.top, array.bottom - 1)
            val right = (cx + half).coerceIn(left + 1, array.right)
            val bottom = (cy + half).coerceIn(top + 1, array.bottom)
            val area = arrayOf(MeteringRectangle(Rect(left, top, right, bottom), 800))
            builder.set(CaptureRequest.CONTROL_AF_REGIONS, area)
            builder.set(CaptureRequest.CONTROL_AE_REGIONS, area)
            val surface = producer?.surface
            if (surface == null) {
                result.error("unavailable", "Preview surface was released", null)
                return
            }
            val capture = camera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW).apply {
                addTarget(surface)
                set(CaptureRequest.CONTROL_AF_REGIONS, area)
                set(CaptureRequest.CONTROL_AE_REGIONS, area)
                set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_AUTO)
                set(CaptureRequest.CONTROL_AF_TRIGGER, CaptureRequest.CONTROL_AF_TRIGGER_START)
                set(CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER, CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_START)
            }
            configured.capture(capture.build(), null, captureHandler)
            // Return to the continuous mode for the repeating preview.
            builder.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE)
            builder.set(CaptureRequest.CONTROL_AF_TRIGGER, CaptureRequest.CONTROL_AF_TRIGGER_IDLE)
            builder.set(CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER, CaptureRequest.CONTROL_AE_PRECAPTURE_TRIGGER_IDLE)
            configured.setRepeatingRequest(builder.build(), null, mainHandler)
            result.success(true)
        } catch (error: Exception) {
            result.error("unavailable", error.message ?: "Focus failed", null)
        }
    }

    private fun setTorch(on: Boolean, result: MethodChannel.Result) {
        torch = on
        try {
            request?.set(
                CaptureRequest.FLASH_MODE,
                if (on) CaptureRequest.FLASH_MODE_TORCH else CaptureRequest.FLASH_MODE_OFF,
            )
            val configured = session
            val builder = request
            if (configured != null && builder != null) {
                configured.setRepeatingRequest(builder.build(), null, mainHandler)
            }
            result.success(torch)
        } catch (error: Exception) {
            result.error("unavailable", error.message ?: "Torch is unavailable", null)
        }
    }

    private fun chooseCamera(): String? {
        val ids = manager.cameraIdList
        for (id in ids) {
            val facing = manager.getCameraCharacteristics(id)
                .get(CameraCharacteristics.LENS_FACING)
            if (facing == CameraCharacteristics.LENS_FACING_BACK) return id
        }
        return ids.firstOrNull()
    }

    /// Largest output whose long side stays under `limit`; falls back to the
    /// smallest size when every option is bigger.
    private fun chooseSize(sizes: Array<Size>?, limit: Int): Size? {
        if (sizes == null || sizes.isEmpty()) return null
        val within = sizes.filter { maxOf(it.width, it.height) <= limit }
        val pool = within.ifEmpty { sizes.toList() }
        return pool.minByOrNull { abs(maxOf(it.width, it.height) - limit) }
    }

    private fun displayRotation(): Int {
        val value = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            activity.display?.rotation ?: Surface.ROTATION_0
        } else {
            @Suppress("DEPRECATION")
            activity.windowManager.defaultDisplay.rotation
        }
        return when (value) {
            Surface.ROTATION_90 -> 90
            Surface.ROTATION_180 -> 180
            Surface.ROTATION_270 -> 270
            else -> 0
        }
    }

    fun stop() {
        pendingCapture?.error("cancelled", "The camera page was closed", null)
        pendingCapture = null
        pendingStart?.error("cancelled", "The camera page was closed", null)
        pendingStart = null
        try {
            session?.close()
        } catch (_: Exception) {
        }
        session = null
        try {
            device?.close()
        } catch (_: Exception) {
        }
        device = null
        request = null
        try {
            reader?.close()
        } catch (_: Exception) {
        }
        reader = null
        try {
            producer?.release()
        } catch (_: Exception) {
        }
        producer = null
        previewSize = null
        activeArray = null
        characteristics = null
        zoom = 1f
        maxZoom = 1f
        captureHandler = null
        captureThread?.quitSafely()
        captureThread = null
        torch = false
    }

    fun dispose() {
        stop()
        channel.setMethodCallHandler(null)
    }

    companion object {
        private const val permissionRequest = 7801
    }
}
