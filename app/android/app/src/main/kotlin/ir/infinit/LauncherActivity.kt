package ir.infinit

import android.app.Activity
import android.content.Intent
import android.os.Bundle

/**
 * The target of every launcher alias.
 *
 * Android records the component a task was started from; disabling that
 * component finishes the task even with `DONT_KILL_APP`.  When the aliases
 * pointed at [MainActivity] directly, switching the icon therefore had to wait
 * until the app was in the background, which is exactly when the launcher was
 * rebuilding its icons: the icon greyed out for a few seconds.
 *
 * This trampoline owns nothing: it hands the launch to [MainActivity] and
 * finishes at once.  The task then belongs to [MainActivity], so the app can
 * enable the new alias and disable the old one immediately while the user is
 * still on the settings screen, and the launcher has the new icon ready before
 * the user goes home.
 */
class LauncherActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Join the task that already hosts MainActivity instead of stacking a
        // second copy of the Flutter screen; on a cold start the task is new.
        startActivity(
            Intent(this, MainActivity::class.java).apply {
                addFlags(
                    Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP
                )
            }
        )
        finish()
        overridePendingTransition(0, 0)
    }
}
